/**
 * Cache.h encapsulates header functionality for the Cache class.
 *
 * Created on: August 27th, 2021
 * Author: Stevan Dupor
 * Copyright (C) 2021 Stevan Dupor - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited.
 */

#include "Cache.h"
#include <iostream>
#include <cmath>
#include <algorithm>
#include <sstream>

Cache::Cache(cache_params params, uint8_t level) {
   // Initialize parameters and control switches
   this->params = params;
   this->level = level;
   this->main_memory = false;

   // Initialize statistics counters
   reads = 0, read_misses = 0, read_hits = 0, writes = 0, write_misses = 0, write_hits = 0, vc_swaps = 0,
      write_backs = 0, vc_swap_requests = 0;

   // Based on level parameter, instantiate this cache as an L1, an L2, or a main memory
   switch (level) {
      case MAIN_MEM: // This is a main  memory
         this->main_memory = true;
         victim_cache = nullptr;
         return;
      case L1: // This is an L1 cache
         local_size = params.l1_size;
         local_assoc = params.l1_assoc;
         initialize_cache_sets();

         // If we parameters indicate we are adding a victim cache (size>0), instantiate a victim cache,
         // otherwise, set victim_cache ptr to null.
         params.vc_num_blocks > 0 ? victim_cache = new Cache(params.vc_num_blocks, params.block_size) :
                 victim_cache = nullptr;

         // Increment level and recursively instantiate either a next-level cache or a main memory
         ++level;
         if (params.l2_size == 0)
            level = MAIN_MEM; // There is no L2 cache so the next level is Main Memory
         next_level = new Cache(params, level);
         return;
      case L2: // This is an L2 cache
         local_size = params.l2_size;
         local_assoc = params.l2_assoc;
         victim_cache = nullptr;
         initialize_cache_sets();

         // Recursively instantiate a main memory at the next-level
         level = MAIN_MEM;
         next_level = new Cache(params, level);
         return;
   }
}

// Create a fully-assoc victim cache
Cache::Cache(uint_fast32_t num_blocks, uint_fast32_t blocksize) {
   // Initialize parameters and control switches
   this->level = VC;
   this->main_memory = false;

   // Initialize statistics counters
   reads = 0, read_misses = 0, read_hits = 0, writes = 0, write_misses = 0, write_hits = 0, vc_swaps = 0, write_backs = 0, vc_swap_requests = 0;

   // Create the fully-associative victim cache
   block_size=blocksize;
   local_assoc = num_blocks;
   sets.emplace_back(Set(local_assoc));
   index_length = 0; // Fully-associative
   block_length = log2(block_size);
}

inline void Cache::initialize_cache_sets() {
   block_size = (uint8_t) params.block_size;
   size_t qty_sets = local_size / (local_assoc * block_size);
   for (size_t i = 0; i < qty_sets; ++i) {
      sets.emplace_back(Set(local_assoc));
   }
   index_length = log2(qty_sets);
   block_length = log2(block_size);
}

// Stub destructor
Cache::~Cache() = default;

void Cache::read(const unsigned long &addr) {
   if (this->main_memory) {
      ++this->reads;
      return;
   }

   // Separate tag, index, block offset
   uint_fast32_t tag,index;
   extract_tag_index(&tag, &index, &addr);

   // Search the set at the calculated index for the requested block
   auto block = std::find_if(sets[index].blocks.begin(), sets[index].blocks.end(), [&](Block b) {
      return b.valid && b.tag == tag;
   });

   if (block == sets[index].blocks.end()) {
      // Block was not found, cache MISS, increment counter and select a victim block to evict (LRU)
      ++read_misses;
      auto oldest_block = std::find_if(sets[index].blocks.begin(), sets[index].blocks.end(), [&](Block b) {
         return b.recency == local_assoc - 1;
      });

      // Always check if the requested block is in the victim cache. Evals to false and continues if no VC exists.
      if(attempt_vc_swap(addr, index, oldest_block)) {
         ++reads;
         update_set_recency(sets[index], *oldest_block);
         return;
      }

      // VC Does not exist or swap failed; if victim block is dirty, writeback to next level
      if (oldest_block->dirty) {
         ++write_backs;
         next_level->write(((oldest_block->tag << index_length) + index) << block_length);
      }

      // Retrieve requested block from next level
      next_level->read(addr);

      // Emplace retrieved block into set and update set recency counters
      oldest_block->valid = true;
      oldest_block->tag = tag;
      oldest_block->dirty = false;
      update_set_recency(sets[index], *oldest_block);
   } else {
      // Cache read HIT. Update counter and recencies.
      ++read_hits;
      update_set_recency(sets[index], *block);
   }
   ++reads;
}

void Cache::write(const unsigned long &addr) {
   // If (this) is a main memory, it always HITS. Increment counter and return.
   if (this->main_memory) {
      ++this->writes;
      return;
   }

   // Separate tag, index, block offset
   uint_fast32_t tag,index;
   extract_tag_index(&tag, &index, &addr);

   // Search the set at the calculated index for the requested block
   auto block = std::find_if(sets[index].blocks.begin(), sets[index].blocks.end(), [&](Block b) {
      return b.valid && b.tag == tag;
   });

   if (block == sets[index].blocks.end()) {
      // Block was not found, cache MISS.
      ++write_misses;
      // Find Oldest block
      auto oldest_block = std::find_if(sets[index].blocks.begin(), sets[index].blocks.end(), [&](Block b) {
         return b.recency == local_assoc - 1;
      });

      // Check if block is available in the victim cache, if so, swap. Evals false and continues if VC does not exist.
      if(attempt_vc_swap(addr, index, oldest_block)) {
         update_set_recency(sets[index], *oldest_block);
         oldest_block->dirty = true;
         ++writes;
         return;
      }

      // If victim block is dirty, writeback to next level
      if (oldest_block->dirty) {
         ++write_backs;
         next_level->write(((oldest_block->tag << index_length) + index) << block_length);
      }

      // Allocate this block from next level in preparation to write.
      next_level->read(addr);

      // Emplace allocated block into set
      oldest_block->valid = true;
      oldest_block->tag = tag;

      // WRITE TO this block, and set dirty bit.
      oldest_block->dirty = true;

      // Traverse and update recency
      update_set_recency(sets[index], *oldest_block);
   } else {
      //Block was found in this set, cache HIT. Write to block.
      block->dirty = true;
      ++write_hits;

      //If the recency hierarchy has changed, traverse the set and update recencies
      update_set_recency(sets[index], *block);
   }
   ++writes;
}

inline bool Cache::vc_has_block(const uint_fast32_t &addr) {
   uint_fast32_t tag = addr >> (block_length);
   uint_fast32_t index = 0;

   auto block = std::find_if(sets[index].blocks.begin(), sets[index].blocks.end(), [&](Block b) {
      return b.valid && b.tag == tag;
   });

   if (block == sets[index].blocks.end())
      return false;
   return true;
}

bool Cache::attempt_vc_swap(const unsigned long &addr, uint_fast32_t index,
                            std::vector<cache_blocks>::iterator &oldest_block) {
   if(victim_cache && victim_cache->vc_has_block(addr)) {
      // Victim cache exists, and possesses the requested block. Swap it for the selected victim block.
      victim_cache->vc_execute_swap(&*oldest_block, addr,
                                    (((oldest_block->tag << index_length) + index) << block_length));
      oldest_block->tag = oldest_block->tag >> index_length;
      ++vc_swap_requests;
      ++vc_swaps;
      update_set_recency(sets[index], *oldest_block);

      // Swap was a success, return true.
      return true;
   } else if (victim_cache && !victim_cache->vc_has_block(addr) && oldest_block->valid) {
      //Victim cache exists and doesn't have requested block. Push selected victim block into VC
      victim_cache->vc_insert_block(&*oldest_block, ((oldest_block->tag << index_length) + index) << block_length);

      // If swapped block from VC to be evicted is dirty, writeback to next level
      if(oldest_block->dirty && oldest_block->valid) {
         next_level->write(oldest_block->tag  << block_length);
         oldest_block->dirty = false;
         ++write_backs;
      }
      // Remove index bits from tag to match this cache's set-associativity.
      oldest_block->tag = oldest_block->tag >> index_length;
      ++vc_swap_requests;
   }

   // Either VC exists and did not have the block, or VC does not exist; oldest_block is now free to be evicted
   // to the next level, return false.
   return false;
}

inline void Cache::vc_execute_swap(Block *incoming_block, const unsigned long &wanted_addr,
                                   const unsigned long &sent_addr) {
   uint_fast32_t wanted_tag=wanted_addr>>block_length, wanted_index=0;
   uint_fast32_t sent_tag = sent_addr>>block_length;

   Block *outgoing_block = &*std::find_if(sets[wanted_index].blocks.begin(), sets[wanted_index].blocks.end(), [&](Block b) {
      return b.valid && b.tag == wanted_tag;
   });

   // swap the dirty bits
   bool temp_dirty = outgoing_block->dirty;
   outgoing_block->dirty = incoming_block->dirty;
   incoming_block->dirty = temp_dirty;

   // Swap the tags/data. NOTE: In the caller, we must right shift the wanted_index out of the returned block.
   incoming_block->tag = outgoing_block->tag;
   outgoing_block->tag = sent_tag;
   incoming_block->valid = outgoing_block->valid;
   outgoing_block->valid = true;

   //If the recency hierarchy has changed, traverse the set and update recencies
   update_set_recency(sets[wanted_index], *outgoing_block);
}

// Emplace this block into the VC, in place of oldest block. If oldest block is invalid/available, ensure dirty
//bit is cleared so no writeback is called.
inline void Cache::vc_insert_block(Block *incoming_block, const unsigned long &sent_addr) {

   uint_fast32_t sent_tag = sent_addr>>block_length, sent_index=0;

   // Find Oldest block
   auto oldest_block = std::find_if(sets[sent_index].blocks.begin(), sets[sent_index].blocks.end(), [&](Block b) {
      return b.recency == local_assoc - 1;
   });

   // swap the dirty bits
   bool temp_dirty = oldest_block->dirty;
   oldest_block->dirty = incoming_block->dirty;
   incoming_block->dirty = temp_dirty;

   // Swap the tags. In the caller, we must right shift the sent_index out to match caller set associativity.
   incoming_block->tag = oldest_block->tag;
   oldest_block->tag = sent_tag;
   incoming_block->valid = oldest_block ->valid;
   oldest_block->valid = true;

   //If the recency hierarchy has changed, traverse the set and update recencies
   update_set_recency(sets[sent_index], *oldest_block);
}

inline void Cache::extract_tag_index(uint_fast32_t *tag, uint_fast32_t *index, const unsigned long *addr) const {
   *tag = *addr >> (index_length + block_length);
   *index = *addr - (*tag << (index_length + block_length));
   *index = *index >> block_length;
}

inline void Cache::update_set_recency(Set &set, Block &block) {
   if (block.recency != 0) {
      for (Block &traversal_block: set.blocks)
         if (traversal_block < block)
            ++traversal_block.recency;
      block.recency = 0;
   }
}

void Cache::contents_report() {
   if (this->main_memory)
      return;
   else if(this->level==VC) {
      std::cout << "===== VC contents =====\n";
      cache_line_report(0);
      std::cout << std::endl;
      return;
   }
   else
      std::cout << "===== L" << std::to_string(this->level) << " contents =====\n";

   for (size_t i = 0; i < sets.size(); ++i)
      cache_line_report(i);
   std::cout << "\n";
   if(victim_cache)
      victim_cache->contents_report();
   this->next_level->contents_report();
}

void Cache::cache_line_report(uint8_t set_num) {
   std::string output = "  set  ";
   if (set_num <= 9 )
      output += " ";
   output += std::to_string(set_num);
   output += ": ";

   // Copy the set and sort it
   std::vector<Block> temp_set = sets[set_num].blocks;
   std::sort(temp_set.begin(), temp_set.end());

   // Traverse the set and Convert the contents to a string
   for (Block &b : temp_set) {
      this->level == VC ? output += " " : output += "  ";
      std::stringstream ss;
      ss << std::hex << b.tag;
      output += ss.str();
      output += " ";
      if (b.dirty)
         output += "D";
      else
         output += " ";
   }
   output += "\n";

   // Print contents to console
   std::cout << output;
}

void Cache::statistics_report() {
   if (this->level == L1) {
      L1_stats_report();
      next_level->statistics_report();
      return;
   }
   L2_stats_report();
}

void Cache::L1_stats_report() {
   std::string output = "===== Simulation results =====\n";
   output += "  a. number of L1 reads:                ";
   cat_padded(&output, this->reads);
   output += "  b. number of L1 read misses:          ";
   cat_padded(&output, this->read_misses);
   output += "  c. number of L1 writes:               ";
   cat_padded(&output, this->writes);
   output += "  d. number of L1 write misses:         ";
   cat_padded(&output, this->write_misses);
   output += "  e. number of swap requests:           ";
   cat_padded(&output, this->vc_swap_requests);
   output += "  f. swap request rate:                 ";
   cat_padded(&output, (double)std::round(10000*((double)this->vc_swap_requests)/((double)this->reads+(double)this->writes))/10000); // TODO swap request rate
   output += "  g. number of swaps:                   ";
   cat_padded(&output, this->vc_swaps);
   output += "  h. combined L1+VC miss rate:          ";
   cat_padded(&output, (double)std::round(10000*((double)(this->read_misses+this->write_misses-this->vc_swaps))/((double)(this->reads+this->writes)))/10000); // TODO l1+vc miss rate
   output += "  i. number writebacks from L1/VC:      ";
   cat_padded(&output, this->write_backs);

   std::cout << output;
}

void Cache::L2_stats_report() {
   std::string output = "";

   if (level == L2) {
      output += "  j. number of L2 reads:                ";
      cat_padded(&output, this->reads);
      output += "  k. number of L2 read misses:          ";
      cat_padded(&output, this->read_misses);
      output += "  l. number of L2 writes:               ";
      cat_padded(&output, this->writes);
      output += "  m. number of L2 write misses:         ";
      cat_padded(&output, this->write_misses);
      output += "  n. L2 miss rate:                      ";
      cat_padded(&output, (double)std::round(10000*(double)this->read_misses / (double)this->reads)/10000); //Todo miss rate
      output += "  o. number of writebacks from L2:      ";
      cat_padded(&output, this->write_backs);
      output += "  p. total memory traffic:              ";
      cat_padded(&output, (this->next_level->reads + this->next_level->writes));
   } else {
      output += "  j. number of L2 reads:                ";
      cat_padded(&output, (uint_fast32_t) 0);
      output += "  k. number of L2 read misses:          ";
      cat_padded(&output, (uint_fast32_t) 0);
      output += "  l. number of L2 writes:               ";
      cat_padded(&output, (uint_fast32_t) 0);
      output += "  m. number of L2 write misses:         ";
      cat_padded(&output, (uint_fast32_t) 0);
      output += "  n. L2 miss rate:                      ";
      cat_padded(&output, 0.0); //Todo miss rate
      output += "  o. number of writebacks from L2:      ";
      cat_padded(&output, (uint_fast32_t) 0);
      output += "  p. total memory traffic:              ";
      cat_padded(&output, (this->reads + this->writes));
   }
   std::cout << output;
}

// Attach a numeric value to the end of a string with space padding
void Cache::cat_padded(std::string *str, uint_fast32_t n) {
   std::string value = std::to_string(n);
   while (value.length() < 12)
      value = " " + value;
   value += "\n";
   *str += value;
}

// Attach a numeric value to the end of a string with space padding
void Cache::cat_padded(std::string *str, double n) {
   std::string value = std::to_string(n).substr(0, 6);
   while (value.length() < 12)
      value = " " + value;
   value += "\n";
   *str += value;
}

// Attach a numeric value to the end of a string with space padding
void Cache::cat_padded(std::string *str, std::string *cat) {
   std::string value = *cat;
   while (value.length() < 16)
      value = " " + value;
   value += "\n";
   *str += value;
}