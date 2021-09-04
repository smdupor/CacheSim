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
   this->params = params;
   this->level = level;
   this->main_memory = false;
   reads = 0, read_misses = 0, read_hits = 0, writes = 0, write_misses = 0, write_hits = 0, vc_swaps = 0, write_backs = 0, vc_swap_requests = 0, count=0;

   switch (level) {
      case 0xff: // This is a main  memory
         this->main_memory = true;
         victim_cache = nullptr;
         return;
      case 0x01: // This is an L1 cache
         local_size = params.l1_size;
         local_assoc = params.l1_assoc;
         initialize_cache_memory();

         params.vc_num_blocks > 0 ? victim_cache = new Cache(params.vc_num_blocks, params.block_size) :
                 victim_cache = nullptr;

         ++level;
         if (params.l2_size == 0)
            level = 0xff; // There is no L2 cache so the next level is Main Memory
         next_level = new Cache(params, level);
         return;
      case 0x02: // This is an L2 cache
         local_size = params.l2_size;
         local_assoc = params.l2_assoc;
         victim_cache = nullptr;
         initialize_cache_memory();
         level = 0xff; // Force next level to main memory
         next_level = new Cache(params, level);
         return;
   }
}

// Create a victim cache
Cache::Cache(uint_fast32_t num_blocks, uint_fast32_t blocksize) {
   this->level = 0xfe;
   this->main_memory = false;
   reads = 0, read_misses = 0, read_hits = 0, writes = 0, write_misses = 0, write_hits = 0, vc_swaps = 0, write_backs = 0, vc_swap_requests = 0;
   block_size=blocksize;
   local_assoc = num_blocks;

   //for (size_t i = 0; i < num_blocks; ++i) {
      sets.emplace_back(Set(local_assoc));
   //}
   index_length = 0;
   block_length = log2(block_size);
   tag_length = address_length - index_length - block_length;

}

inline void Cache::initialize_cache_memory() {
   block_size = (uint8_t) params.block_size;
   size_t num_sets = local_size / (local_assoc * block_size);
   for (size_t i = 0; i < num_sets; ++i) {
      sets.emplace_back(Set(local_assoc));
   }
   index_length = log2(num_sets);
   block_length = log2(block_size);
   tag_length = address_length - index_length - block_length;
}

// Stub destructor
Cache::~Cache() = default;

void Cache::read(const unsigned long &addr) {
   if (this->main_memory) {
      ++this->reads;
      return;
   }

   // Separate tag, index, block offset
   uint_fast32_t tag = addr >> (index_length + block_length);
   uint_fast32_t index = addr - (((const uint_fast32_t) tag) << (index_length + block_length));
   index = index >> block_length;

   // Search the set at the calculated index for the requested block
   auto block = std::find_if(sets[index].blocks.begin(), sets[index].blocks.end(), [&](Block b) {
      return b.valid && b.tag == tag;
   });

   // Not found
   if (block == sets[index].blocks.end()) {
      ++read_misses;
      // Find Oldest block
      auto oldest_block = std::find_if(sets[index].blocks.begin(), sets[index].blocks.end(), [&](Block b) {
         return b.recency == local_assoc - 1;
      });

      if(attempt_vc_swap(addr, index, oldest_block)) {
         ++reads;

            for (Block &traversal_block : sets[index].blocks)
               if (traversal_block.recency < oldest_block->recency)
                  ++traversal_block.recency;
            oldest_block->recency = 0;

         return;
      }

      // If dirty, writeback
      if (oldest_block->dirty) {
         ++write_backs;
         next_level->write(addr);
      }
      next_level->read(addr);

      // Emplace block into set
      oldest_block->valid = true;
      oldest_block->tag = tag;
      oldest_block->dirty = false;

      // Traverse and update recency

      for (Block &traversal_block : sets[index].blocks)
         if (traversal_block.recency < oldest_block->recency)
            ++traversal_block.recency;
      oldest_block->recency = 0;
   } else {
      ++read_hits;

      //If the recency hierarchy has changed, traverse the set and update recencies
      if (block->recency != 0) {
         for (Block &traversal_block : sets[index].blocks)
            if (traversal_block.recency < block->recency)
               ++traversal_block.recency;
         block->recency = 0;
      }
   }
   ++reads;
}

inline bool Cache::vc_exists(uint_fast32_t addr) {
   uint_fast32_t tag = addr >> (block_length);
   uint_fast32_t index = 0;
   index = 0;

   auto block = std::find_if(sets[index].blocks.begin(), sets[index].blocks.end(), [&](Block b) {
      return b.valid && b.tag == tag;
   });

   Block debug = *block;

   if (block == sets[index].blocks.end())
      return false;
   return true;
}

inline void Cache::extract_tag_index(uint_fast32_t *tag, uint_fast32_t *index, const unsigned long *addr) {
   *tag = *addr >> (index_length + block_length);
   *index = *addr - (*tag << (index_length + block_length));
   *index = *index >> block_length;
}

inline void Cache::vc_swap(Block *incoming_block, const unsigned long &wanted_addr, const unsigned long &sent_addr) {
// Swap data WITHOUT changing recency of incoming block, but changing recency in VC
   uint_fast32_t wanted_tag=wanted_addr>>block_length, wanted_index=0;
   uint_fast32_t sent_tag = sent_addr>>block_length;
   //extract_tag_index(&wanted_tag, &wanted_index, &wanted_addr);

   Block *outgoing_block = &*std::find_if(sets[wanted_index].blocks.begin(), sets[wanted_index].blocks.end(), [&](Block b) {
      return b.valid && b.tag == wanted_tag;
   });

   // swap the dirty bits
   bool temp_dirty = outgoing_block->dirty;
   outgoing_block->dirty = incoming_block->dirty;
   incoming_block->dirty = temp_dirty;

   // Swap the tags. In the caller, we must right shift the wanted_index out

   incoming_block->tag = outgoing_block->tag;
   outgoing_block->tag = sent_tag;

   incoming_block->valid = outgoing_block->valid;
   outgoing_block->valid = true;


   //If the recency hierarchy has changed, traverse the set and update recencies
   if (outgoing_block->recency != 0) {
      for (Block &traversal_block : sets[wanted_index].blocks)
         if (traversal_block.recency < outgoing_block->recency)
            ++traversal_block.recency;
      outgoing_block->recency = 0;
   }
}

inline void Cache::vc_replace(Block *incoming_block, const unsigned long &sent_addr) {
   // Emplace this block into the VC, in place of oldest block. If oldest block is invalid/available, ensure dirty
   //bit is cleared so no writeback is called.
   uint_fast32_t sent_tag = sent_addr>>block_length, sent_index=0;
   //extract_tag_index(&sent_tag, &sent_index, &sent_addr);

   // Find Oldest block
   auto oldest_block = std::find_if(sets[sent_index].blocks.begin(), sets[sent_index].blocks.end(), [&](Block b) {
      return b.recency == local_assoc - 1;
   });

   Block debug = *oldest_block;
   // swap the dirty bits
   bool temp_dirty = oldest_block->dirty;
   oldest_block->dirty = incoming_block->dirty;
   incoming_block->dirty = temp_dirty;

   // Swap the tags. In the caller, we must right shift the sent_index out
   incoming_block->tag = oldest_block->tag;
   oldest_block->tag = sent_tag;

   bool temp_valid = oldest_block->valid;
   incoming_block->valid = oldest_block ->valid;
   oldest_block->valid = true;


   //If the recency hierarchy has changed, traverse the set and update recencies
   if (oldest_block->recency != 0) {
      for (Block &traversal_block : sets[sent_index].blocks)
         if (traversal_block.recency < oldest_block->recency)
            ++traversal_block.recency;
      oldest_block->recency = 0;
   }

}

void Cache::write(const unsigned long &addr) {
   if (this->main_memory) {
      ++this->writes;
      return;
   }

   // Separate tag, index, block offset
   uint_fast32_t tag = addr >> (index_length + block_length);
   uint_fast32_t index = addr - (tag << (index_length + block_length));
   index = index >> block_length;

   // Search the set at the calculated index for the requested block
   auto block = std::find_if(sets[index].blocks.begin(), sets[index].blocks.end(), [&](Block b) {
      return b.valid && b.tag == tag;
   });

   if (block == sets[index].blocks.end()) {
      //MISS
      ++write_misses;
      // Find Oldest block
      auto oldest_block = std::find_if(sets[index].blocks.begin(), sets[index].blocks.end(), [&](Block b) {
         return b.recency == local_assoc - 1;
      });
      if(attempt_vc_swap(addr, index, oldest_block)) {
         if (oldest_block->recency != 0) {
            for (Block &traversal_block : sets[index].blocks)
               if (traversal_block.recency < oldest_block->recency)
                  ++traversal_block.recency;
            oldest_block->recency = 0;
         }
         // Actually write to the swapped-in block
         oldest_block->dirty = true;
         ++writes;
         return;
      }





      // Assuming success at higher level, emplace into this cache


      // If dirty, writeback
      if (oldest_block->dirty) {
         ++write_backs;
         next_level->write(addr);
      }
      // Get this block from the next level
      next_level->read(addr);

      // Emplace block into set
      oldest_block->valid = true;
      oldest_block->tag = tag;

      // WRITE TO this block, and set dirty bit.
      oldest_block->dirty = true;

      // Traverse and update recency

      for (Block &traversal_block : sets[index].blocks)
         if (traversal_block.recency < oldest_block->recency)
         ++traversal_block.recency;
      oldest_block->recency = 0;
   } else //HIT
   {
      block->dirty = true;
      ++write_hits;

      //If the recency hierarchy has changed, traverse the set and update recencies
      if (block->recency != 0) {
         for (Block &traversal_block : sets[index].blocks)
            if (traversal_block.recency < block->recency)
               ++traversal_block.recency;
         block->recency = 0;
      }
   }
   ++writes;
}

bool Cache::attempt_vc_swap(const unsigned long &addr, uint_fast32_t index,
                            std::vector<cache_blocks>::iterator &oldest_block) {
   if(victim_cache && victim_cache->vc_exists(addr)) {
      // vc has it, swap
      victim_cache->vc_swap(&*oldest_block, addr, (((oldest_block->tag << index_length) + index) << block_length));
      oldest_block->tag = oldest_block->tag >> index_length;
      ++vc_swap_requests;
      ++vc_swaps;
      for (Block &traversal_block : sets[index].blocks)
         if(traversal_block.recency < oldest_block->recency)
            ++traversal_block.recency;
      oldest_block->recency = 0;
      return true;
   } else if (victim_cache && !victim_cache->vc_exists(addr) && oldest_block->valid) {
      //vs doesn't have it, push it into vc, writeback what we got out of vc, continue
      victim_cache->vc_replace(&*oldest_block, ((oldest_block->tag << index_length) + index) << block_length);


      //if what we got from VC is dirty, writeback
      if(oldest_block->dirty && oldest_block->valid) {
         next_level->write(oldest_block->tag << block_length);
         oldest_block->dirty = false;
         ++write_backs;
      }
      oldest_block->tag = oldest_block->tag >> index_length;

      ++vc_swap_requests;
   }
   return false;
}

void Cache::contents_report() {
   if (this->main_memory)
      return;
   else if(this->level==0xfe) {
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
      this->level == 0xfe ? output += " " : output += "  ";
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
   if (this->level == 1) {
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
   cat_padded(&output, 0.0); // TODO swap request rate
   output += "  g. number of swaps:                   ";
   cat_padded(&output, this->vc_swaps);
   output += "  h. combined L1+VC miss rate:          ";
   cat_padded(&output, 0.0); // TODO l1+vc miss rate
   output += "  i. number writebacks from L1/VC:      ";
   cat_padded(&output, this->write_backs);

   std::cout << output;
}

void Cache::L2_stats_report() {
   std::string output = "";

   if (level == 2) {
      output += "  j. number of L2 reads:                ";
      cat_padded(&output, this->reads);
      output += "  k. number of L2 read misses:          ";
      cat_padded(&output, this->read_misses);
      output += "  l. number of L2 writes:               ";
      cat_padded(&output, this->writes);
      output += "  m. number of L2 write misses:         ";
      cat_padded(&output, this->write_misses);
      output += "  n. L2 miss rate:                      ";
      cat_padded(&output, 0.0); //Todo miss rate
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