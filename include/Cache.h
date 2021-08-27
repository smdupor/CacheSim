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

typedef struct Blocks {
   uint_fast32_t tag;
   bool valid;
   bool is_main_memory;
   uint8_t recency;
} cache_blocks;

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
   // Timing variables
   std::chrono::time_point<std::chrono::steady_clock> sim_start;
   bool main_memory;
   cache_blocks blocks[];

public:
   Cache(cache_params *params, bool main_mem);
   ~Cache();
   void read(unsigned long &addr);
   void write(unsigned long &addr, unsigned long &data);
};

#endif //CACHESIM_INCLUDE_CACHE_H
