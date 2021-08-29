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

Cache::Cache(cache_params params, uint8_t level) {
   this->params = params;
   this->level = level;
   this->main_memory = false;
   reads=0, read_misses=0, read_hits=0, writes=0, write_misses=0, write_hits=0, vc_swaps=0;

   switch(level) {
      case 0xff: // This is a main  memory
         this->main_memory = true;
         return;
      case 0x01: // This is an L1 cache
         local_size =  params.l1_size;
         local_assoc = params.l1_assoc;
         initialize_cache_memory();
         ++level;
         if(params.l2_size == 0)
            level = 0xff; // There is no L2 cache so the next level is Main Memory
         next_level = new Cache(params, level);
         return;
      case 0x02: // This is an L2 cache
         local_size =  params.l2_size;
         local_assoc = params.l2_assoc;
         initialize_cache_memory();
         level = 0xff; // Force next level to main memory
         next_level = new Cache(params, level);
         return;
   }
}

inline void Cache::initialize_cache_memory() {
   block_size = (uint8_t) params.block_size;
   size_t num_sets = local_size / (local_assoc*block_size);
   for(size_t i =0; i<num_sets; ++i) {
      sets.emplace_back(Set(local_assoc));
   }
   index_length = log2(num_sets);
   block_length = log2(block_size);
   tag_length = address_length - index_length - block_length;
}

// Stub destructor
Cache::~Cache() = default;

void Cache::read(unsigned long &addr){
   if(this->main_memory) {
      ++read_hits;
      return;
   }

   uint_fast32_t tag = addr >> (index_length + block_length);
   uint_fast32_t index = addr - (tag << (index_length + block_length));
   index  = index >> block_length;

   auto b = std::find_if(sets[index].blocks.begin(), sets[index].blocks.end(), [&](Block b) {
      return b.valid && b.tag == tag;
   });
   if(b == sets[index].blocks.end()) {
      //MISS
      ++read_misses;

      next_level->read(addr);

      // Assuming success at higher level, emplace into this cache
      uint_fast32_t temp_recency = 0;
      uint8_t chosen_block = 0, counter = 0;

      // Traverse the set, looking for oldest block and updating age of blocks
      for(Block &bl : sets[index].blocks){
         if(!bl.valid || bl.recency > temp_recency)
            chosen_block = counter;
         ++bl.recency;
      }

      // Evict old data, and admit new data
      sets[index].blocks[chosen_block].recency = 0;
      sets[index].blocks[chosen_block].valid = true;
      sets[index].blocks[chosen_block].tag = tag;
      sets[index].blocks[chosen_block].dirty = false;
   }
   else {
      ++read_hits;
      //Traverse the set and update recency of access
      for(Block &bl : sets[index].blocks){
         if(bl.recency != 0 && bl.recency != 0xff)
            ++bl.recency;
      }

      b->recency = 0;


   }
   ++reads;
}

void Cache::write(unsigned long &addr) {

}

void Cache::report() {
   std::cout << "There were " << read_misses << " read misses and " << read_hits << " read hits.";
}
