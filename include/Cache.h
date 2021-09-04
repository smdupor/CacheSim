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
#include <string>

typedef struct Block {
   explicit Block(uint_fast32_t init_recency) {
     tag = 0;
     valid = false;
     recency = init_recency;
     dirty = false;
   };
   uint_fast32_t tag;
   bool valid;
   uint_fast32_t recency;
   bool dirty;
   bool operator < (const Block i) const {
      return (recency < i.recency);
   }
} cache_blocks;

typedef struct Set{
   explicit Set(uint_fast32_t size) {
      for(size_t i =0; i< size; ++i){
         blocks.emplace_back(cache_blocks(i));
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
   uint_fast32_t reads, read_hits, read_misses, writes, write_hits, write_misses, vc_swaps, write_backs, vc_swap_requests;
   uint_fast32_t tag_length, index_length, block_length, block_size, local_assoc, level;
   uint_fast32_t num_sets, local_size;
   Cache *next_level;
   Cache *victim_cache;
   cache_params params;
   std::vector<Set> sets;

   // Current Access indices
   uint_fast32_t current_index, current_tag;
   inline void initialize_cache_memory();

   void cache_line_report(uint8_t set_num);
   void L1_stats_report();
   void L2_stats_report();
   void cat_padded(std::string *str, uint_fast32_t n);
   void cat_padded(std::string *str, double n);

   void extract_tag_index(uint_fast32_t *tag, uint_fast32_t *index, const uint_fast32_t *addr);

public:
   Cache(cache_params params, uint8_t level);
   Cache(uint_fast32_t num_blocks, uint_fast32_t blocksize);
   ~Cache();
   void read(const unsigned long &addr);
   void write(const unsigned long &addr);
   inline bool vc_exists(uint_fast32_t addr);
   inline void vc_replace(Block *b, const unsigned long &sent_addr);
   inline void vc_swap(Block *incoming_block, const unsigned long &wanted_addr, const unsigned long &sent_addr);
   void contents_report();
   void statistics_report();
};

#endif //CACHESIM_INCLUDE_CACHE_H
