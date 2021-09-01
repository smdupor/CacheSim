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
   reads = 0, read_misses = 0, read_hits = 0, writes = 0, write_misses = 0, write_hits = 0, vc_swaps = 0, write_backs = 0, vc_swap_requests = 0;

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
   local_assoc = 0;

   for (size_t i = 0; i < num_blocks; ++i) {
      sets.emplace_back(Set(local_assoc));
   }
   index_length = log2(num_sets);
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

void Cache::read(unsigned long &addr) {
   if (this->main_memory) {
      ++this->reads;
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

      // Find Oldest block
      auto oldest_block = std::find_if(sets[index].blocks.begin(), sets[index].blocks.end(), [&](Block b) {
         return b.recency == local_assoc - 1;
      });

      if(victim_cache && victim_cache->vc_exists(addr)) {
         victim_cache->vc_swap(&*oldest_block, addr);
         ++vc_swaps;
         ++vc_swap_requests;

         for (Block &traversal_block : sets[index].blocks)
            if(traversal_block.recency < oldest_block->recency)
               ++traversal_block.recency;
         oldest_block->recency = 0;
         return;
      } else if (victim_cache && !victim_cache->vc_exists(addr)) {
         victim_cache->vc_replace(&*oldest_block);
         ++vc_swap_requests;
      }
         next_level->read(addr);
         ++read_misses;
      // If dirty, writeback
      if (oldest_block->dirty) {
         ++write_backs;
         next_level->write(addr);
      }

      // Emplace block into set
      oldest_block->valid = true;
      oldest_block->tag = tag;
      oldest_block->dirty = false;

      // Traverse and update recency

      for (Block &traversal_block : sets[index].blocks)
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
   uint_fast32_t tag = addr >> (index_length + block_length);
   uint_fast32_t index = addr - (tag << (index_length + block_length));
   index = index >> block_length;

   auto block = std::find_if(sets[index].blocks.begin(), sets[index].blocks.end(), [&](Block b) {
      return b.valid && b.tag == tag;
   });

   return block != sets[index].blocks.end();
}


inline void Cache::vc_swap(Block *b, unsigned long &addr) {
// Swap data WITHOUT changing recency of incoming block, but changing recency in VC
   uint_fast32_t tag = addr >> (index_length + block_length);
   uint_fast32_t index = addr - (tag << (index_length + block_length));
   index = index >> block_length;

   auto block = std::find_if(sets[index].blocks.begin(), sets[index].blocks.end(), [&](Block b) {
      return b.valid && b.tag == tag;
   });

   // STOPPING POINT: Problem: VC should, really, not change the returned block b/c it doesn't have tag info for the
   // L1 Cache. Perhaps we should just swap the dirty bits, then handle updating the tag in the caller function??????


   uint_fast32_t temp;
}

inline void Cache::vc_replace(Block *b) {
   // Emplace this block into the VC, in place of oldest block. If oldest block is invalid/available, ensure dirty
   //bit is cleared so no writeback is called.

}

void Cache::write(unsigned long &addr) {
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

      // Find Oldest block
      auto oldest_block = std::find_if(sets[index].blocks.begin(), sets[index].blocks.end(), [&](Block b) {
         return b.recency == local_assoc - 1;
      });

      if(victim_cache && vc_exists(addr)) {
         vc_swap(&*oldest_block, addr);
         ++vc_swaps;
         ++vc_swap_requests;

         for (Block &traversal_block : sets[index].blocks)
            if(traversal_block.recency < oldest_block->recency)
               ++traversal_block.recency;
         oldest_block->recency = 0;
         oldest_block->dirty = true;
         return;
      } else if (victim_cache && !vc_exists(addr)) {
         vc_replace(&*oldest_block);
         ++vc_swap_requests;
      }
      ++write_misses;



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

void Cache::contents_report() {
   if (this->main_memory)
      return;
   std::cout << "===== L" << std::to_string(this->level) << " contents =====\n";
   for (size_t i = 0; i < sets.size(); ++i)
      cache_line_report(i);
   std::cout << "\n";
   this->next_level->contents_report();
}

void Cache::cache_line_report(uint8_t set_num) {
   std::string output = "  set  ";
   if (set_num <= 9)
      output += " ";
   output += std::to_string(set_num);
   output += ": ";

   // Copy the set and sort it
   std::vector<Block> temp_set = sets[set_num].blocks;
   std::sort(temp_set.begin(), temp_set.end());

   // Traverse the set and Convert the contents to a string
   for (Block &b : temp_set) {
      output += "  ";
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
   output += "a. number of L1 reads:                ";
   cat_padded(&output, this->reads);
   output += "b. number of L1 read misses:          ";
   cat_padded(&output, this->read_misses);
   output += "c. number of L1 writes:               ";
   cat_padded(&output, this->writes);
   output += "d. number of L1 write misses:         ";
   cat_padded(&output, this->write_misses);
   output += "e. number of swap requests:           ";
   cat_padded(&output, this->vc_swap_requests);
   output += "f. swap request rate:                 ";
   cat_padded(&output, 0.0); // TODO swap request rate
   output += "g. number of swaps:                   ";
   cat_padded(&output, this->vc_swaps);
   output += "h. combined L1+VC miss rate:          ";
   cat_padded(&output, 0.0); // TODO l1+vc miss rate
   output += "i. number writebacks from L1/VC:      ";
   cat_padded(&output, this->write_backs);

   std::cout << output;
}

void Cache::L2_stats_report() {
   std::string output = "";

   if (level == 2) {
      output += "j. number of L2 reads:                ";
      cat_padded(&output, this->reads);
      output += "k. number of L2 read misses:          ";
      cat_padded(&output, this->read_misses);
      output += "l. number of L2 writes:               ";
      cat_padded(&output, this->writes);
      output += "m. number of L2 write misses:         ";
      cat_padded(&output, this->write_misses);
      output += "n. L2 miss rate:                      ";
      cat_padded(&output, 0.0); //Todo miss rate
      output += "o. number of writebacks from L2:      ";
      cat_padded(&output, this->write_backs);
      output += "p. total memory traffic:              ";
      cat_padded(&output, (this->next_level->reads + this->next_level->writes));
   } else {
      output += "j. number of L2 reads:                ";
      cat_padded(&output, (uint_fast32_t) 0);
      output += "k. number of L2 read misses:          ";
      cat_padded(&output, (uint_fast32_t) 0);
      output += "l. number of L2 writes:               ";
      cat_padded(&output, (uint_fast32_t) 0);
      output += "m. number of L2 write misses:         ";
      cat_padded(&output, (uint_fast32_t) 0);
      output += "n. L2 miss rate:                      ";
      cat_padded(&output, 0.0); //Todo miss rate
      output += "o. number of writebacks from L2:      ";
      cat_padded(&output, (uint_fast32_t) 0);
      output += "p. total memory traffic:              ";
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