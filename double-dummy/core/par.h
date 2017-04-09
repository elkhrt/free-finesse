// This file is part of FreeFinesse, a double-dummy analyzer (c) Edward Lockhart, 2010
// It is made available under the GPL; see the file COPYING for details

#pragma once

#include "types.h"

typedef struct deal_analysis_t {
	int tricks[4][5];	// [declarer][trumps]
} deal_analysis_t;

typedef struct result_t {
	int level;
	suit_t trumps;
	player_t declarer;
	int tricks;
	int score;			// for declarer
} result_t;

extern "C" void analyze_par(int board, const deal_analysis_t* analysis, result_t* result);
