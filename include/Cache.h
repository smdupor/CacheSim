/**
 * Cache.h encapsulates header functionality for the Cache class.
 *
 * Created on: August 27th, 2021
 * Author: Stevan Dupor
 * Copyright (C) 2021 Stevan Dupor - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited.
 */

#ifndef CACHESIM_INCLUDE_CACHE_H
#define CACHESIM_INCLUDE_CACHE_H

#include <chrono>
#include <vector>

typedef struct Block {
   explicit Block() {
     tag = 0;
     valid = false;
     recency = 0;
     dirty = false;
   };
   uint_fast32_t tag;
   bool valid;
   uint8_t recency;
   bool dirty;
} cache_blocks;

typedef struct Set{
   explicit Set(uint8_t size) {
      for(size_t i =0; i< size; ++i){
         blocks.emplace_back(cache_blocks());
      }
   };
   std::vector<cache_blocks> blocks;
} set;

typedef struct cache_params{
   unsigned long int block_size;
   unsigned long int l1_size;
   unsigned long int l1_assoc;
   unsigned long int vc_num_blocks;
   unsigned long int l2_size;
   unsigned long int l2_assoc;
} cache_params;

class Cache {

private:
   //Constants
   const uint8_t address_length = 32;
   // System-Level Vars
   std::chrono::time_point<std::chrono::steady_clock> sim_start;
   bool main_memory;
   uint_fast32_t reads, read_hits, read_misses, writes, write_hits, write_misses, vc_swaps;
   uint8_t tag_length, index_length, block_length, block_size, local_assoc, level;
   uint_fast32_t num_sets, local_size;
   Cache *next_level;
   cache_params params;
   std::vector<Set> sets;

   // Current Access indices
   uint_fast32_t current_index, current_tag;
   inline void initialize_cache_memory();


public:
   Cache(cache_params params, uint8_t level);
   ~Cache();
   void read(unsigned long &addr);
   void write(unsigned long &addr);
   void report();
};

#endif //CACHESIM_INCLUDE_CACHE_H
