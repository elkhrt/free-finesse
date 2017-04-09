// This file is part of FreeFinesse, a double-dummy analyzer (c) Edward Lockhart, 2010
// It is made available under the GPL; see the file COPYING for details

#include "par.h"
#include <algorithm>

namespace {

	// Board number, dealer and vulnerabiility
	struct board_t {
		int number;
		bool vul[2];
		player_t dealer;
		board_t(int board)
		{
			number = board;
			dealer = player_t((board-1)%4);
			vul[pEW] = (0x936c & (1<<((board-1)%16))) != 0; // 1001 0011 0110 1100 
			vul[pNS] = (0x5a5a & (1<<((board-1)%16))) != 0; // 0101 1010 0101 1010
		}
	};	

	// Scoring
	int score(suit_t trumps, int level, int tricks, bool vul)
	{
		// Undertricks, assumed doubled (for par computation)
		const int target = level+6;
		if (tricks < target) {
			int down = (target-tricks);
			if (vul) {
				if (down == 1) return -200;
				else return -500 - 300 * (down-2);
			} else {
				if (down == 1) return -100;
				if (down == 2) return -300;
				else return -500 - 300 * (down-3);
			}
		}
		
		// Trick score
		const int pertrick[] = {20, 20, 30, 30, 30};
		int score = pertrick[trumps] * level;
		if (trumps == nt) score += 10;
		
		// Bonuses
		if (score >= 100) score += (vul ? 500 : 300); else score += 50;
		if (level == 7) score += (vul ? 1500 : 1000);
		if (level == 6) score += (vul ? 750 : 500);
		
		// Overtricks
		if (tricks > target)
			score += pertrick[trumps] * (tricks - target);
		return score;
	}

	int contractIndex(int level, suit_t trumps)
	{
		return level*5 + trumps - 4;
	}

	// First and second to speak in a partnership
	player_t first(partnership_t p, player_t dealer)
	{
		if (p == pEW) {
			if ((dealer == plE) || (dealer == plN)) return plE;
			else return plW;
		} else {
			if ((dealer == plW) || (dealer == plN)) return plN;
			else return plS;
		}
	}

	// Order to speak of a player
	int order(player_t pl, player_t dealer)
	{
		if (pl == dealer) return 0;
		if ((pl+1)%4 == dealer) return 1;
		if ((pl+2)%4 == dealer) return 2;
		return 3;
	}

	// Stores info on a partnership's result in a contract
	struct partnershipResult_t
	{
		int score;
		player_t declarer;
	};

	// Compute result for a partnership
	partnershipResult_t partnershipResult(int level, suit_t trumps, const deal_analysis_t* analysis, partnership_t p, const board_t& board)
	{
		player_t p0 = first(p, board.dealer);
		player_t p1 = partner(p0);
		int t0 = analysis->tricks[p0][trumps],
			t1 = analysis->tricks[p1][trumps];
		partnershipResult_t rv;
		rv.score = score(trumps, level, std::max(t0, t1), board.vul[p]);
		rv.declarer = (t1 > t0) ? p1 : p0;
		return rv;
	}
}

// Par calculation
void analyze_par(int boardnumber, const deal_analysis_t* analysis, result_t* par)
{
	// Get dealer and vulnerability
	board_t board(boardnumber);
	
	// First assemble the scores for each contract for each partnership
	partnershipResult_t pres[2][39];
	for (int trumps = 0; trumps < 5; ++trumps) {
		for (int level = 1; level <= 7; ++level) {
			int i = contractIndex(level, suit_t(trumps));
			pres[pNS][i] = partnershipResult(level, suit_t(trumps), analysis, pNS, board);
			pres[pEW][i] = partnershipResult(level, suit_t(trumps), analysis, pEW, board);
		}
	}

	// Best so far is passed out
	int index = 0;
	player_t decl;
	bool improved = true;
	int bestScoreNS = 0;
	
	while (improved) {
		improved = false;
		
		// Can N/S do better?
		for (int i = 1+index; i <= contractIndex(7, nt); ++i) {
			if (pres[pNS][i].score > bestScoreNS) {
				index = i;
				decl = pres[pNS][i].declarer;
				bestScoreNS = pres[pNS][i].score;
				improved = true;
			}
		}
				
		// Can E/W do better?		
		for (int i = 1+index; i <= contractIndex(7, nt); ++i) {
			if (pres[pEW][i].score > -bestScoreNS) {
				index = i;
				decl = pres[pEW][i].declarer;
				bestScoreNS = -pres[pEW][i].score;
				improved = true;
			}
		}
		
		// Would both sides like to declare the same contract? If so, see who gets to do it first.
		partnership_t p = partnership(decl);	
		if (improved && pres[1-p][index].score > -pres[p][index].score) {
			if (order(pres[1-p][index].declarer, board.dealer) < order(pres[p][index].declarer, board.dealer)) {
				p = partnership_t(1-p);
				decl = pres[p][index].declarer;
				bestScoreNS = (p == pNS ? +1 : -1) * pres[p][index].score;
			}
		}
	}
	
	// Assemble the par contract
	par->level = ((index-1) / 5) + 1;
	par->trumps = suit_t((index-1) % 5);
	par->declarer = decl;
	par->tricks = analysis->tricks[decl][par->trumps];
	par->score = pres[partnership(decl)][index].score;
}
