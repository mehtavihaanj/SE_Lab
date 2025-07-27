/**************************************************************************
  * C S 429 system emulator
 * 
 * cache.c - A cache simulator that can replay traces from Valgrind
 *     and output statistics such as number of hits, misses, and
 *     evictions, both dirty and clean.  The replacement policy is LRU. 
 *     The cache is a writeback cache. 
 * 
 * Copyright (c) 2021, 2023, 2024, 2025. 
 * Authors: M. Hinton, Z. Leeper.
 * All rights reserved.
 * May not be used, modified, or copied without permission.
 **************************************************************************/ 
#include <getopt.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <assert.h>
#include <limits.h>
#include <string.h>
#include <errno.h>
#include "cache.h"




#define ADDRESS_LENGTH 64

/* Counters used to record cache statistics in printSummary().
   test-cache uses these numbers to verify correctness of the cache. */

//Increment when a miss occurs
int miss_count = 0;

//Increment when a hit occurs
int hit_count = 0;

//Increment when a dirty eviction occurs
int dirty_eviction_count = 0;

//Increment when a clean eviction occurs
int clean_eviction_count = 0;

/* STUDENT TO-DO: add more globals, structs, macros if necessary */
uword_t next_lru;

uword_t bitfield_u64(uword_t src, unsigned frompos, unsigned width);
static size_t _log(size_t x) {
  size_t result = 0;
  while(x>>=1)  {
    result++;
  }
  return result;
} 

/*
 * Initialize the cache according to specified arguments
 * Called by cache-runner so do not modify the function signature
 *
 * The code provided here shows you how to initialize a cache structure
 * defined above. It's not complete and feel free to modify/add code.
 */
cache_t *create_cache(int A_in, int B_in, int C_in, int d_in) {
    /* see cache-runner for the meaning of each argument */
    cache_t *cache = malloc(sizeof(cache_t));
    cache->A = A_in;
    cache->B = B_in;
    cache->C = C_in;
    cache->d = d_in;
    unsigned int S = cache->C / (cache->A * cache->B);

    cache->sets = (cache_set_t*) calloc(S, sizeof(cache_set_t));
    for (unsigned int i = 0; i < S; i++){
        cache->sets[i].lines = (cache_line_t*) calloc(cache->A, sizeof(cache_line_t));
        for (unsigned int j = 0; j < cache->A; j++){
            cache->sets[i].lines[j].valid = 0;
            cache->sets[i].lines[j].tag   = 0;
            cache->sets[i].lines[j].lru   = 0;
            cache->sets[i].lines[j].dirty = 0;
            cache->sets[i].lines[j].data  = calloc(cache->B, sizeof(byte_t));
        }
    }

    /* TODO: add more code for initialization */
    next_lru = 0;
    return cache;
}

cache_t *create_checkpoint(cache_t *cache) {
    unsigned int S = (unsigned int) cache->C / (cache->A * cache->B);
    cache_t *copy_cache = malloc(sizeof(cache_t));
    memcpy(copy_cache, cache, sizeof(cache_t));
    copy_cache->sets = (cache_set_t*) calloc(S, sizeof(cache_set_t));
    for (unsigned int i = 0; i < S; i++) {
        copy_cache->sets[i].lines = (cache_line_t*) calloc(cache->A, sizeof(cache_line_t));
        for (unsigned int j = 0; j < cache->A; j++) {
            memcpy(&copy_cache->sets[i].lines[j], &cache->sets[i].lines[j], sizeof(cache_line_t));
            copy_cache->sets[i].lines[j].data = calloc(cache->B, sizeof(byte_t));
            memcpy(copy_cache->sets[i].lines[j].data, cache->sets[i].lines[j].data, sizeof(byte_t));
        }
    }
    
    return copy_cache;
}

void display_set(cache_t *cache, unsigned int set_index) {
    unsigned int S = (unsigned int) cache->C / (cache->A * cache->B);
    if (set_index < S) {
        cache_set_t *set = &cache->sets[set_index];
        for (unsigned int i = 0; i < cache->A; i++) {
            printf ("Valid: %d Tag: %llx Lru: %lld Dirty: %d\n", set->lines[i].valid, 
                set->lines[i].tag, set->lines[i].lru, set->lines[i].dirty);
        }
    } else {
        printf ("Invalid Set %d. 0 <= Set < %d\n", set_index, S);
    }
}

/*
 * Free allocated memory. Feel free to modify it
 */
void free_cache(cache_t *cache) {
    unsigned int S = (unsigned int) cache->C / (cache->A * cache->B);
    for (unsigned int i = 0; i < S; i++){
        for (unsigned int j = 0; j < cache->A; j++) {
            free(cache->sets[i].lines[j].data);
        }
        free(cache->sets[i].lines);
    }
    free(cache->sets);
    free(cache);
}

/* STUDENT TO-DO:
 * Get the line for address contained in the cache
 * On hit, return the cache line holding the address
 * On miss, returns NULL
 */
cache_line_t *get_line(cache_t *cache, uword_t addr) {
    // Student TODO
  // unsigned int S = (unsigned int) cache->C / (cache->A * cache->B);
  // if (next_lru == __INT64_MAX__) {
  //     for (unsigned int i = 0; i < S; i++){
  //         for (unsigned int j = 0; j < cache->A; j++) {
  //             cache->sets[i].lines[j].lru = 0;
  //         }
  //         free(cache->sets[i].lines);
  //     }
  //     next_lru = 1;
  // }
    
    
    unsigned int memBlockSize_b = _log(cache->B);
    unsigned int setCount = (unsigned int) cache->C / (cache->A * cache->B);
    unsigned int numSetBits_s = _log(setCount);

    uword_t tag = bitfield_u64(addr, memBlockSize_b + numSetBits_s, ADDRESS_LENGTH - memBlockSize_b - numSetBits_s);
    uword_t setIndex = bitfield_u64(addr, memBlockSize_b, numSetBits_s);
  //  uword_t offset = bitfield_u64(addr, 0, memBlockSize_b);
   // cache_set_t currentSet = cache->sets[setIndex];

    for (unsigned int j = 0; j < cache->A; j++) {
        uword_t currentTag = cache->sets[setIndex].lines[j].tag;

        if (currentTag == tag && cache->sets[setIndex].lines[j].valid) {
            // Hit occurred
            next_lru++;
            return &cache->sets[setIndex].lines[j];
        }
    }

    // No hits = Miss, return NULL
    return NULL;
}

/* STUDENT TO-DO:
 * Select the line to fill with the new cache line
 * Return the cache line selected to filled in by addr
 */
cache_line_t *select_line(cache_t *cache, uword_t addr) {
    // Student TODO

    // Loop through the cache set for this address by getting the tag
    // If the tag is different (a new block) and the block is invalid (don't want to overwrite valid values yet) then replace
    // Else if we didn't find any open spots, need to replace based on LRU policy

    unsigned int memBlockSize_b = _log(cache->B);
    unsigned int setCount = (unsigned int) cache->C / (cache->A * cache->B);
    unsigned int numSetBits_s = _log(setCount);

  //  uword_t tag = bitfield_u64(addr, memBlockSize_b + numSetBits_s, ADDRESS_LENGTH - memBlockSize_b - numSetBits_s);
    uword_t setIndex = bitfield_u64(addr, memBlockSize_b, numSetBits_s);
  

    for (unsigned int j = 0; j < cache->A; j++) {
     //   uword_t currentTag = cache->sets[setIndex].lines[j].tag;
        bool validBit = cache->sets[setIndex].lines[j].valid;

        // if valid == false
        if (!validBit) {
            return &cache->sets[setIndex].lines[j];
        }
    }

    // Since we have reached here, need to find LRU block
    int min = INT_MAX;
    for (unsigned int j = 0; j < cache->A; j++) {
        uword_t lru = cache->sets[setIndex].lines[j].lru;
        if (lru <= min) {
            min = lru;
        }
    }

    for (unsigned int j = 0; j < cache->A; j++) {
        if (cache->sets[setIndex].lines[j].lru <= min) {
            return &cache->sets[setIndex].lines[j];
        }
    }
    return NULL;
}

/*  STUDENT TO-DO:
 *  Check if the address is hit in the cache, updating hit and miss data.
 *  Return true if pos hits in the cache.
 */
bool check_hit(cache_t *cache, uword_t addr, operation_t operation) {
    // Student TODO
    cache_line_t* cacheLineTemp = get_line(cache, addr);
    
    // if the address is a miss 
    if (!cacheLineTemp) {
        miss_count++;
        return false;
    }
    
    hit_count++;
    cacheLineTemp->lru = next_lru;
   if (operation == WRITE) {
       cacheLineTemp->dirty = true;
   }
   
    // Next: need to updates the LRU information and the "dirty" status depending on the operation_t operation
    return true;
}

/*  STUDENT TO-DO:
 *  Handles Misses, evicting from the cache if necessary.
 *  Fill out the evicted_line_t struct with info regarding the evicted line.
 */
evicted_line_t *handle_miss(cache_t *cache, uword_t addr, operation_t operation, byte_t *incoming_data) {
    
    evicted_line_t *evicted_line = malloc(sizeof(evicted_line_t));
    evicted_line->data = (byte_t *) calloc(cache->B, sizeof(byte_t));
    

    cache_line_t* selected = select_line(cache, addr);
    
    unsigned int memBlockSize_b = _log(cache->B);
    unsigned int setCount = (unsigned int) cache->C / (cache->A * cache->B);
    unsigned int numSetBits_s = _log(setCount);
    uword_t setIndex = bitfield_u64(addr, memBlockSize_b, numSetBits_s);
    
    
    if (selected->valid) {
        if (selected->dirty) {
            dirty_eviction_count++;
        }
        else {
            clean_eviction_count++;
        }
    }

    
    //uword_t offset = bitfield_u64(addr, 0, memBlockSize_b);
    evicted_line->addr = (selected->tag << (memBlockSize_b + numSetBits_s) | (setIndex << memBlockSize_b));
    if (selected->data) {
        memcpy(evicted_line->data, selected->data, cache->B);
    }
  //else {
  //    memset(evicted_line->data, 0, cache->B);
  //}
    
   
    evicted_line->dirty = selected->dirty;
    evicted_line->valid = selected->valid;
    
    if (incoming_data) {
        memcpy(selected->data, incoming_data, cache->B);
    }
    //else {
    //    memset(selected->data, 0, cache->B);
    //}
    
   // selected.
    selected->dirty = operation == WRITE;
    selected->valid = true;
    selected->tag = bitfield_u64(addr, memBlockSize_b + numSetBits_s, ADDRESS_LENGTH - memBlockSize_b - numSetBits_s);
    
    selected->lru = next_lru++;
    
  
    return evicted_line;
}

/* STUDENT TO-DO:
 * Get 8 bytes from the cache and write it to dest.
 * Preconditon: addr is contained within the cache.
 */
void get_word_cache(cache_t *cache, uword_t addr, word_t *dest) {
    // Student TODO

    unsigned int memBlockSize_b = _log(cache->B);
    uword_t offset = bitfield_u64(addr, 0, memBlockSize_b);
    cache_line_t *line_ptr = get_line(cache, addr);
    
    byte_t* bytePointer = (line_ptr->data) + offset;
  //  if (offset + sizeof(word_t) <= cache->B) {
    line_ptr->lru = next_lru;
    memcpy(dest, bytePointer, sizeof(word_t));
  //  }
}

/* STUDENT TO-DO:
 * Set 8 bytes in the cache to val at pos.
 * Preconditon: addr is contained within the cache.
 */
void set_word_cache(cache_t *cache, uword_t addr, word_t val) {

    unsigned int memBlockSize_b = _log(cache->B);
    uword_t offset = bitfield_u64(addr, 0, memBlockSize_b);
    
    cache_line_t *line_ptr = get_line(cache, addr);
    byte_t* bytePointer = (line_ptr->data) + offset;

    line_ptr->lru = next_lru;
    memcpy(bytePointer, &val, sizeof(word_t));  
    line_ptr->dirty = true;  
    
}

/*
 * Access data at memory address addr
 * If it is already in cache, increase hit_count
 * If it is not in cache, bring it in cache, increase miss count
 * Also increase eviction_count if a line is evicted
 *
 * Called by cache-runner; no need to modify it if you implement
 * check_hit() and handle_miss()
 */
void access_data(cache_t *cache, uword_t addr, operation_t operation)
{
    if(!check_hit(cache, addr, operation))
        free(handle_miss(cache, addr, operation, NULL));
}

uword_t bitfield_u64(uword_t src, unsigned frompos, unsigned width) {
    return (uword_t) ((src >> frompos) & ((1ULL << width) - 1));
} 
