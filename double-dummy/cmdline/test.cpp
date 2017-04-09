// This file is part of FreeFinesse, a double-dummy analyzer (c) Edward Lockhart, 2010
// It is made available under the GPL; see the file COPYING for details

#include "analyzer.h"
#include "par.h"
#include <iostream>
#include <mach/mach_time.h>

// Analysis for hand records
void test_main()
{
	// Initialise data structures
	play_t play;	
	play.nCardsPlayed = 0;
	position_analysis_t pos;
    deal_t deal;
    randomdeal(&deal);
    randomdeal(&deal);
    randomdeal(&deal);
	
    // Run the tests
    for (int i = 0; i < 10; ++i) 
    {
        const uint64_t startTime = mach_absolute_time();
        deal_analysis_t analysis;
        randomdeal(&deal);
        for (int s = 0; s <= 4; s++) {
            cache_t* cache = new_cache();
            deal.trumps = suit_t(s);
            for (int pl = 0; pl < 4; pl++) {
                deal.declarer = player_t(pl);
                pos.global.low = 0;
                pos.global.high = 1 + 13;
                pos.context = &(analysis.tricks[pl][s]);
                analyze(&deal, &play, cache, 0, &pos, false);
                analysis.tricks[pl][s] = 13-pos.global.low;
                std::cout << "0123456789abcd"[analysis.tricks[pl][s]];
            }
            free_cache(cache);
        }
        const uint64_t endTime = mach_absolute_time();
        const uint64_t elapsedMTU = endTime - startTime;            
        mach_timebase_info_data_t info;
        mach_timebase_info(&info);
        const double elapsedNS = (double)elapsedMTU * (double)info.numer / (double)info.denom;            
        std::cout << ' ' << elapsedNS/1000/1000/1000 << std::endl;
    }
}

