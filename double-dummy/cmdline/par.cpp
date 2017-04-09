// This file is part of FreeFinesse, a double-dummy analyzer (c) Edward Lockhart, 2010
// It is made available under the GPL; see the file COPYING for details

#include "analyzer.h"
#include "par.h"
#include <iostream>


namespace {
	
	// Output makeable contracts in DealMasterPro format
	void dmpForPlayer(std::ostream& out, const int* tricks)
	{
		for (int s = 0; s <= 4; ++s) {
			if (tricks[s] >= 7) out << (tricks[s]-6);
			else out << "-";
		}
	}
	std::ostream& operator<<(std::ostream& out, const deal_analysis_t& a)
	{
		out << "N "; dmpForPlayer(out, a.tricks[plN]); out << std::endl;
		out << "S "; dmpForPlayer(out, a.tricks[plS]); out << std::endl;
		out << "E "; dmpForPlayer(out, a.tricks[plE]); out << std::endl;
		out << "W "; dmpForPlayer(out, a.tricks[plW]);
		return out;
	}

	inline char suittext(suit_t suit) { return "CDHSN"[suit]; }
	inline char playertext(player_t pl) { return "NESW"[pl]; }

	// Output the par result
	std::ostream& operator<<(std::ostream& out, const result_t& res)
	{
		if (res.score == 0) {
			out << "Passed Out";
		} else {
			out << res.level << suittext(res.trumps);
			if (res.score < 0) out << "X";
			int target = res.level+6;
			if (res.tricks == target) out << "=";
			else if (res.tricks < target) out << "-" << (target-res.tricks);
			else out << "+" << (res.tricks-target);
			out << " " << playertext(res.declarer);
			out << ". ";
			const int NSscore = res.score * (partnership(res.declarer) == pNS ? +1 : -1);
			if (NSscore < 0) out << "E/W +" << -NSscore;
			else out << "N/S +" << NSscore;
		}
		return out;
	}
}				

// Analysis for hand records
void par(deal_t d)
{
	// Initialise data structures
	play_t play;	
	play.nCardsPlayed = 0;
	position_analysis_t pos;
	deal_analysis_t analysis;
	
	// Figure out how man tricks we can make for each suit, for each declarer
	for (int s = 0; s <= 4; s++) {
		cache_t* cache = new_cache();
		d.trumps = suit_t(s);
		for (int pl = 0; pl < 4; pl++) {
			d.declarer = player_t(pl);
			pos.global.low = 0;
			pos.global.high = 1 + 13;
			pos.context = &(analysis.tricks[pl][s]);
			analyze(&d, &play, cache, 0, &pos, false);
			analysis.tricks[pl][s] = 13-pos.global.low;
		}
		free_cache(cache);
	}
	
	// Find par result
	result_t result;
	analyze_par(d.board, &analysis, &result);

	// Output everything
	std::cout << analysis << std::endl;
	std::cout << "Par: " << result << std::endl;
}

