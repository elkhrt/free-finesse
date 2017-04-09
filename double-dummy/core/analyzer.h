// This file is part of FreeFinesse, a double-dummy analyzer (c) Edward Lockhart, 2010
// It is made available under the GPL; see the file COPYING for details

//
//  The interface to the double-dummy solving code
//

#pragma once

#include "types.h"

// Bounds on makeable tricks: high > n >= low
typedef struct bound_t {
	int high, low;
} bound_t;

// Analysis of the current position
typedef struct position_analysis_t {
	bound_t global;		// upper and lower bound, based on best play
	bound_t play[52];	// bounds for given play
	void* context;		// context for the caller
} position_analysis_t;

// Data returned to caller
typedef int (*callback_t)(struct position_analysis_t*);

#ifdef __cplusplus
extern "C" {
#endif

// Cache - used by the analyzer
struct cache_t;
struct cache_t* new_cache();
void free_cache(struct cache_t*);
void clear_cache(struct cache_t*);
struct cache_t* clone_cache(struct cache_t*);
    
// Get the current state of play
void dealstate(const deal_t*, const play_t*, dealstate_t*, int quitted);

// Perform analysis
void analyze(const deal_t*, const play_t*, struct cache_t*, const callback_t, position_analysis_t*, bool analyze_moves);

// Generate a random deal
void randomdeal(deal_t*);

#ifdef __cplusplus
}
#endif
		
