/**
 * Cache.h encapsulates headers for the Cache class, which simulates one level of a computer memory hierarchy.
 * The cache memory may be instantiated flexibly based on a series of parameters, and may include a Level 1 (L1),
 * an L1 cache with an accessory victim cache, a Level 2 (L2) cache, or a main memory. In general, the purpose of the
 * simulator is to allow an architecture designer to simulate various hierarchies of cache and measure hit/miss rates
 * across the levels, based on a memory trace from a given program execution. From this, the hierarchy design can be
 * optimized to minimize miss rate, and maximize speed of execution, while working within known chip-area parameters.
 *
 * Abstractions/Recursions: The highest-level (L1) cache contains a pointer to another Cache instance: in the presence
 * of an L2 cache, this next level points to the L2. If there is no L2, the L1's next_level points to the main memory
 * (another instance/abstraction of the Cache class). In turn, the L2 contains a pointer to the main memory, which is
 * another instance of the Cache class. Furthermore, in configurations where a level has a victim cache, the victim_cache
 * pointer references yet another instance of the Cache class.
 *
 * These levels are traversed recursively (in the same way that a computer memory hierarchy works) such that the CPU
 * only ever reads/writes to the interface of the L1 cache. Throughout this hierarchy, extensive use of the GoF strategy
 * pattern enables each recursive call to handle the call, and associated data, with no outside configuration or control
 * from the caller.
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

/**
 * Block contains one block of memory data. Blocks are held within a Set.
 */
struct Block {
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

   // Implement comparator less-than based on recency of this and another given Block
   bool operator < (const Block i) const {
      return (recency < i.recency);
   }
};

/**
 * Set contains a set of Blocks of memory.
 */
struct Set{
   explicit Set(uint_fast32_t size) {
      for(size_t i =0; i< size; ++i){
         blocks.emplace_back(Block(i));
      }
   };
   std::vector<Block> blocks;
};

/**
 * cache_params encapsulates the parameters used to construct the full memory hierarchy.
 */
typedef struct cache_params{
   unsigned long int block_size;
   unsigned long int l1_size;
   unsigned long int l1_assoc;
   unsigned long int vc_num_blocks;
   unsigned long int l2_size;
   unsigned long int l2_assoc;
} cache_params;

// Encapsulate human-readable reference for types/levels that a Cache memory can be.
enum levels{L1 = 0x01, L2=0x02, VC=0xfe, MAIN_MEM=0xff};

class Cache {
private:
   //Constants
   const uint8_t address_length = 32;

   // System-Level Vars
   bool main_memory;
   uint_fast32_t reads, read_hits, read_misses, writes, write_hits, write_misses, vc_swaps, write_backs,
   vc_swap_requests, index_length, block_length, block_size, local_assoc, level, local_size;

   // Pointers to the rest of the memory hierarchy
   Cache *next_level;
   Cache *victim_cache;

   // Hierarchy parameters, stored locally
   cache_params params;

   // Encapsulate multiple Sets of Blocks for an n-way set-associative cache
   std::vector<Set> sets;

   // Internal utility methods
   inline void initialize_cache_sets();
   static inline void update_set_recency(Set &set, Block &block);
   void extract_tag_index(uint_fast32_t *tag, uint_fast32_t *index, const uint_fast32_t *addr) const;

   // Internal statistics/contents reporting methods
   void cache_line_report(uint8_t set_num);
   void L1_stats_report();
   void L2_stats_report();
   void cat_padded(std::string *str, uint_fast32_t n);
   void cat_padded(std::string *str, double n);


public:
   // Construct an L1, L2, or Main Memory cache
   Cache(cache_params params, uint8_t level);

   // Construct a Victim Cache
   Cache(uint_fast32_t num_blocks, uint_fast32_t blocksize);

   //Destructor
   ~Cache();

   // CPU Interface read/write
   void read(const unsigned long &addr);
   void write(const unsigned long &addr);

   //Victim Cache interface
   inline bool vc_has_block(const uint_fast32_t &addr);
   inline void vc_insert_block(Block *incoming_block, const unsigned long &sent_addr);
   inline void vc_execute_swap(Block *incoming_block, const unsigned long &wanted_addr, const unsigned long &sent_addr);
   bool attempt_vc_swap(const unsigned long &addr, uint_fast32_t index, Block *oldest_block);

   // Contents and Statistics reporting interfaces
   void contents_report();
   void statistics_report();

   // External string-manipulation with whitespace padding utility method
   static void cat_padded(std::string *str, std::string *cat);
};

#endif //CACHESIM_INCLUDE_CACHE_H
