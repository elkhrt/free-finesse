// This file is part of FreeFinesse, a double-dummy analyzer (c) Edward Lockhart, 2010
// It is made available under the GPL; see the file COPYING for details

//
//  Opening lead analysis
//

#include "analyzer.h"
#include <iostream>
#include <string>

namespace {

	inline char suittext(suit_t suit) { return "CDHSN"[suit]; }
	inline char suittext(card_t c) { return suittext(suit(c)); }
	inline char ranktext(card_t c) { return "23456789TJQKA"[rank(c)]; }
	inline bool known(const bound_t& b) { return b.high == b.low+1; }

	int callback(position_analysis_t* pos)
	{
		dealstate_t* state = (dealstate_t*)pos->context;
		for (int i = 0; i < 52; ++i) {
			if (state->cardstate[i] == playable && known(pos->play[i])) {
				state->cardstate[i] = notdealt;	// to avoid outputting this again
				std::cout << suittext(i) << ranktext(i) << " " << pos->play[i].low << std::endl;
			}
		}
		return true;
	}

}

void leads(const deal_t& d) 
{
	// Set up data structures
	play_t play;
	play.nCardsPlayed = 0;
	cache_t* cache = new_cache();	
	std::string str;
	position_analysis_t pos;
	pos.global.low = 0;
	pos.global.high = 1+13;
	dealstate_t state;
	dealstate(&d, &play, &state, true);
	pos.context = &state;

	// Analysis
//	player_t pl = state.pl;
//	int tricksleft = 13-(play.nCardsPlayed/4);
	for (int i = 0; i < 52; ++i) {
		pos.play[i].low = 0;
		pos.play[i].high = pos.global.high;
	}
	analyze(&d, &play, cache, callback, &pos, true);
	std::cout << pos.global.low << std::endl;
}

