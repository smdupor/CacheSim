/**
 * Cache.cpp Source code for the Cache class, which simulates the controller for one level of a computer memory hierarchy.
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

#include "Cache.h"
#include <iostream>
#include <cmath>
#include <algorithm>
#include <sstream>

/*************************************** CONSTRUCTION, INITIALIZATION, DESTRUCTION ***********************************/

/**
 * Constructor for a L1, L2, or Main Memory cache. Recursively calls constructors for Victim Caches or next-level
 * caches until the entire memory hierarchy is constructed.
 *
 * @param params of the entire memory hierarchy
 * @param level of this cache (Eg, 1, 2, 0xff (main memory))
 */
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

/**
 * Construct a fully-associative Victim Cache.
 *
 * @param num_blocks the number of blocks in the fully-associative cache.
 * @param blocksize the size of each block, in bytes.
 */
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

/**
 * Initialize each set within (this) cache object based on the local size and local associativity.
 */
inline void Cache::initialize_cache_sets() {
   block_size = (uint8_t) params.block_size;
   size_t qty_sets = local_size / (local_assoc * block_size);
   for (size_t i = 0; i < qty_sets; ++i) {
      sets.emplace_back(Set(local_assoc));
   }
   index_length = log2(qty_sets);
   block_length = log2(block_size);
}

/**
 * Stub destructor. Utilize C++11 garbage collection functionality to handle free()s when objects fall out of scope.
 */
Cache::~Cache() = default;


/******************************************* MAIN I/O INTERFACE ******************************************************/

/**
 * READS: Main IO interface for reads to this level of the memory hierarchy. Recursively reads/writes back on cache
 * misses and local evictions to the next level down the hierarchy.If a victim cache exists at this level, utilizes the
 * victim cache in the case of a miss.
 *
 * @param addr the address in memory requested by the caller (CPU or higher-level of hierarchy).
 */
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
      if(attempt_vc_swap(addr, index, &*oldest_block)) {
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

/**
 * WRITES: Main IO interface for writes to this level of the memory hierarchy. Recursively writes/read-allocates and
 * writes back on cache misses and local evictions to the next level down the hierarchy. If a victim cache exists at
 * this level, utilizes the victim cache in the case of a miss.
 *
 * @param addr the address in memory requested by the caller (CPU or higher-level of hierarchy).
 */
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
      if(attempt_vc_swap(addr, index, &*oldest_block)) {
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

/*********************************************** VICTIM CACHE METHODS ************************************************/

/**
 * Check whether the Victim cache possesses a valid copy of the requested block
 *
 * @param addr The requested address
 * @return TRUE if the block was found in the VC, FALSE if not found.
 */
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

/**
 * If it is possible to swap a provided block from the caller with a specified block in the Victim Cache, call
 * execute_vc_swap to perform the actual swap. In the case where this level does not have a victim cache, handle the
 * swap attempt as a failure. DATA DESTRUCTIVE: On a successful swap, incoming_block will be replaced with the outgoing
 * block from the VC (tag incorrect based on other-level associativity), and must be handled by the caller.
 *
 * @param addr The requested block we want to withdraw from the victim cache
 * @param index The integer index of the incoming block to be inserted (must be added back to the bit shifted tag to
 *              support the different associativity.
 * @param incoming_block The block we want to emplace into the victim cache, and where the outgoing block data will be passed to.
 * @return true if swap was a success (caller can read/write to incoming_block), false if swap was a failure
 *          (caller can freely evict/overwrite incoming_block)
 */
bool Cache::attempt_vc_swap(const unsigned long &addr, uint_fast32_t index, Block *incoming_block) {
   if(victim_cache && victim_cache->vc_has_block(addr)) {
      // Victim cache exists, and possesses the requested block. Swap it for the selected victim block.
      victim_cache->vc_execute_swap(incoming_block, addr,
                                    (((incoming_block->tag << index_length) + index) << block_length));
      incoming_block->tag = incoming_block->tag >> index_length;
      ++vc_swap_requests;
      ++vc_swaps;
      update_set_recency(sets[index], *incoming_block);

      // Swap was a success, return true.
      return true;
   } else if (victim_cache && !victim_cache->vc_has_block(addr) && incoming_block->valid) {
      //Victim cache exists and doesn't have requested block. Push selected victim block into VC
      victim_cache->vc_insert_block(&*incoming_block, ((incoming_block->tag << index_length) + index) << block_length);

      // If swapped block from VC to be evicted is dirty, writeback to next level
      if(incoming_block->dirty && incoming_block->valid) {
         next_level->write(incoming_block->tag  << block_length);
         incoming_block->dirty = false;
         ++write_backs;
      }
      // Remove index bits from tag to match this cache's set-associativity.
      incoming_block->tag = incoming_block->tag >> index_length;
      ++vc_swap_requests;
   }

   // Either VC exists and did not have the block, or VC does not exist; oldest_block is now free to be evicted
   // to the next level, return false.
   return false;
}

/**
 * Perform the actual swap between an incoming block, and a specified block within the victim cache.
 * DATA DESTRUCTIVE: On a successful swap, incoming_block will be replaced with the outgoing
 * block from the VC (tag incorrect based on other-level associativity), and must be handled by the caller.
 *
 * @param incoming_block the block to be emplaced into the VC, and where the outgoing block data is written
 * @param wanted_addr the full-length address of the block we want from the VC
 * @param sent_addr the full-length address of the block we are sending to the VC
 */
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

/**
 * Insert this block into the victim cache WITHOUT performing a swap with a specific block. Replaces the oldest block
 * in the victim cache.
 *
 * @param incoming_block the block we want to emplace into the VC. On return, contains a block that MAY need to be
 *                       written back to the next level.
 * @param sent_addr the full-length address of the block we are emplacing into the VC
 */
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

/********************************************* UTILITY METHODS *******************************************************/

/**
 * Given a full-length memory address, calculate the proper tag and index for this level of cache, and write those data
 * into tag and index.
 *
 * @param tag the calculated tag from addr
 * @param index the calculated index from addr
 * @param addr the full-length memory address
 */
inline void Cache::extract_tag_index(uint_fast32_t *tag, uint_fast32_t *index, const unsigned long *addr) const {
   *tag = *addr >> (index_length + block_length);
   *index = *addr - (*tag << (index_length + block_length));
   *index = *index >> block_length;
}

/**
 * Traverse a given set, and update the recency of the blocks within that set based on an access to the passed-in block.
 * The passed-in block becomes most recent (recency=0), the recency of older blocks than the passed-in block stays the
 * same, and the recency of newer blocks is incremented by 1.
 *
 * @param set reference to the full set of blocks needing recency updated
 * @param block the block to be updated as most-recently-accessed
 */
inline void Cache::update_set_recency(Set &set, Block &block) {
   if (block.recency != 0) {
      for (Block &traversal_block: set.blocks)
         if (traversal_block < block)
            ++traversal_block.recency;
      block.recency = 0;
   }
}

/******************************************** STATISTICS and REPORTING ***********************************************/

/**
 * Traverse the entire contents of this cache at the time of calling, and call cache_line_report for each set to
 * generate report to stdout. If this is a main memory, do not print contents.
 *
 * Recursively call for subsequent victim caches and levels until the entire hierarchy has been reported on.
 */
void Cache::contents_report() {
   if (this->main_memory) {
      return;
   }

   else if(this->level==VC) { // Print victim cache header, contents, and return if this is a VC.
      std::cout << "===== VC contents =====\n";
      cache_line_report(0);
      std::cout << std::endl;
      return;
   }
   else { // Print cache level header and continue
      std::cout << "===== L" << std::to_string(this->level) << " contents =====\n";
   }
   // Print the contents of each set to console
   for (size_t i = 0; i < sets.size(); ++i)
      cache_line_report(i);
   std::cout << "\n";

   //If this cache level has a victim cache attached, recursively run report on the VC
   if(victim_cache)
      victim_cache->contents_report();

   // Recursively run report on the next level of the memory hierarchy
   this->next_level->contents_report();
}

/**
 * For a given set, traverse set and print entire contents as well as dirty status.
 *
 * @param set_num
 */
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
      // Handle the fact that the example code uses an extra space for victim caches here.
      this->level == VC ? output += " " : output += "  ";

      if(b.valid) { // If this block is valid, concatenate contents
         // convert unsigned 32 long to hex string
         std::stringstream ss;
         ss << std::hex << b.tag;
         output += ss.str();
         output += " ";
         if (b.dirty)
            output += "D";
         else
            output += " ";
      }
      else
      { // If block is invalid/empty, simply concatenate a whitespace-padded dash
         output += "   -     ";
      }
   }
   output += "\n";

   // Print contents to console
   std::cout << output;
}

/**
 * Run the appropriate statistics report for this level of hierarchy. Recursively call until all statistics have been
 * reported for the entire memory.
 */
void Cache::statistics_report() {
   if (this->level == L1) {
      L1_stats_report();
      next_level->statistics_report();
      return;
   }
   L2_stats_report();
}

/**
 * Report statistics for Level-1 caches to the console.
 */
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

/**
 * Report statistics for level-2 caches to the console
 */
void Cache::L2_stats_report() {
   std::string output = "";

   if (level == L2) { // This hierarchy has a level-2 cache
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
   } else { // This hierarchy DOES NOT HAVE a level-2 cache, and this method is being called on the main memory.
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

/************************************** STRING MANIPULATION METHODS **************************************************/

/**
 * Concatenate an integer to the end of a string, and add whitespace padding up to a predefined amount before the
 * integer.
 *
 * @param str the string to be concatenated to
 * @param n the integer to emplace at the end of the string.
 */
void Cache::cat_padded(std::string *str, uint_fast32_t n) {
   std::string value = std::to_string(n);
   while (value.length() < 12)
      value = " " + value;
   value += "\n";
   *str += value;
}

/**
 * Concatenate a Double to the end of a string, and add whitespace padding up to a predefined amount before the
 * double.
 *
 * @param str the string to be concatenated to
 * @param n the double to emplace at the end of the string.
 */
void Cache::cat_padded(std::string *str, double n) {
   std::string value = std::to_string(n).substr(0, 6);
   while (value.length() < 12)
      value = " " + value;
   value += "\n";
   *str += value;
}

/**
 * Concatenate a String to the end of a string, and add whitespace padding up to a predefined amount before the
 * String to be added.
 *
 * @param head the string to be concatenated to
 * @param tail the string to emplace at the end of "head" with up to 16 spaces in the middle.
 */
void Cache::cat_padded(std::string *head, std::string *tail) {
   std::string value = *tail;
   while (value.length() < 16)
      value = " " + value;
   value += "\n";
   *head += value;
}