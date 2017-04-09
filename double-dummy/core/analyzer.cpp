// This file is part of FreeFinesse, a double-dummy analyzer (c) Edward Lockhart, 2010
// It is made available under the GPL; see the file COPYING for details

//
//  Implementation of the analyzer
//

#include "types.h"
#include "analyzer.h"

#include <vector>
#include <map>
#include <cstdlib>

// In most cases, we can use <inttypes.h>
typedef unsigned int uint;
#ifndef WIN32
#include <inttypes.h> 
typedef uint8_t uint8;
typedef uint64_t uint64;
#endif

// But windows doesn't have it
#ifdef WIN32
typedef unsigned __int8 uint8;
typedef unsigned __int64 uint64;
#pragma warning(disable: 4146)			// signed manipulations
#endif

namespace {		

	char ranktext(rank_t r) { return "23456789TJQKA"[r]; }
	char suittext(suit_t s) { return "CDHSN"[s]; }
	char playertext(player_t pl) { return "NESW"[pl]; }	
	char csuittext(card_t c) { return suittext(suit(c)); }
	char cranktext(card_t c) { return ranktext(rank(c)); }

	// Bit twiddling operations for 64-bit integers
	inline uint64 bit(int n) { return uint64(1) << n; }
	inline uint64 lsb(uint64 x) { return x & (-x); }
	const int bitindextable[] = {-1, 0, 1, 39, 2, 15, 40, 23, 3, 12, 16, 59, 41, 19, 24, 54, 4,
		-1, 13, 10, 17, 62, 60, 28, 42, 30, 20, 51, 25, 44, 55, 47, 5, 32, -1, 38, 14, 22,
		11, 58, 18, 53, 63, 9, 61, 27, 29, 50, 43, 46, 31, 37, 21, 57, 52, 8, 26, 49, 45,
		36, 56, 7, 48, 35, 6, 34, 33};
	inline int bitindex(uint64 x) { return bitindextable[x % 67]; }
	const uint64 suitmask[] = {300239975158033LL, 600479950316066LL, 1200959900632132LL, 2401919801264264LL, 0};
	const uint64 sameRankOrHigher[] = 
	{300239975158033LL, 600479950316066LL, 1200959900632132LL, 2401919801264264LL, 
		300239975158032LL, 600479950316064LL, 1200959900632128LL, 2401919801264256LL, 
		300239975158016LL, 600479950316032LL, 1200959900632064LL, 2401919801264128LL, 
		300239975157760LL, 600479950315520LL, 1200959900631040LL, 2401919801262080LL, 
		300239975153664LL, 600479950307328LL, 1200959900614656LL, 2401919801229312LL, 
		300239975088128LL, 600479950176256LL, 1200959900352512LL, 2401919800705024LL, 
		300239974039552LL, 600479948079104LL, 1200959896158208LL, 2401919792316416LL, 
		300239957262336LL, 600479914524672LL, 1200959829049344LL, 2401919658098688LL, 
		300239688826880LL, 600479377653760LL, 1200958755307520LL, 2401917510615040LL, 
		300235393859584LL, 600470787719168LL, 1200941575438336LL, 2401883150876672LL, 
		300166674382848LL, 600333348765696LL, 1200666697531392LL, 2401333395062784LL, 
		299067162755072LL, 598134325510144LL, 1196268651020288LL, 2392537302040576LL, 
		281474976710656LL, 562949953421312LL, 1125899906842624LL, 2251799813685248LL};
	
	// Rank equivalence (kept dynamically up-to-date)
	struct rankequiv_t {
		card_t nexthigher[52];
		card_t nextlower[52];

		void init() {
			for (int s = 0; s < 4; s++) {
				nextlower[0 * 4 + s] = 0 * 4 + s;		// deuce
				nexthigher[0 * 4 + s] = 1 * 4 + s;		// deuce
				for (int r = 1; r < 12; r++) {
					nextlower[r * 4 + s] = (r-1) * 4 + s;
					nexthigher[r * 4 + s] = (r+1) * 4 + s;
				}
				nextlower[12 * 4 + s] = 11 * 4 + s;		// Ace
				nexthigher[12 * 4 + s] = 12 * 4 + s;	// Ace
			}
		}

		rankequiv_t() {
			init();
		}

		rankequiv_t(const play_t& p) {
			init();
			for (int i = 0; i < (p.nCardsPlayed&~3); ++i)	// don't include tricks in progress (since cards aren't yet equivalent)
				play(p.played[i]);
		}

		void play(card_t c) {
			card_t nh = nexthigher[c];
			card_t nl = nextlower[c];
			nexthigher[nl] = nh;
			nextlower[nh] = nl;
		}

		void unplay(card_t c) {				// can't unplay more than one card
			card_t nh = nexthigher[c];
			card_t nl = nextlower[c];
			if (nl != c) nexthigher[nl] = c;
			if (nh != c) nextlower[nh] = c;
		}
	};

	// Progress of current trick
	struct trickstate_t {
		player_t leader;
		player_t winner;
		card_t winningcard;
		bool ranktrick;
		suit_t ledsuit;
		suit_t winsuit;
		trickstate_t(player_t leader, card_t card) : leader(leader), winner(leader),
		winningcard(card), ranktrick(false), ledsuit(suit(card)), winsuit(suit(card)) { }
		trickstate_t() { }
		
		// Record a play
		void play(player_t pl, card_t card, suit_t trumps) {
			suit_t s = suit(card);
			if (s == winsuit) {
				ranktrick = true;
				if (card > winningcard) {
					winningcard = card;
					winner = pl;
				}
			} else if (s == trumps) {
				ranktrick = false;
				winningcard = card;
				winsuit = trumps;
				winner = pl;
			}
		}

		// Whether a play would win the trick for this side (although could be partner's winner)
		bool would_win(player_t pl, card_t card, suit_t trumps) {
			if (partnership(winner) == partnership(pl)) return true;
			suit_t s = suit(card);
			if (s == winsuit) {
				return card > winningcard;
			} else if (s == trumps) {
				return true;
			} else {
				return false;
			}
		}
	};

	// State of the game
	struct gamestate_t {
		
		// Construction
		gamestate_t(const deal_t&, const play_t&);

		// Facts about the deal
		int nCardsEach;					// number of cards per player in original deal
		suit_t trumps;
		
		// The play so far
		int nCardsPlayed;				// number of cards played so far in total
		card_t cardsPlayed[52];			// what they were
		player_t whoPlayed[52];			// who played each card
		
		// Derived info about the state of play
		uint tricksLeft() const { return nCardsEach-nCardsPlayed/4; }
		
		// Current state of play
		uint64 mCardsLeft;				// mask cards left in total (deck order)
		uint64 mPlayerHand[4];			// mask cards left per player (deck order)
		uint64 uSuitLengths;			// number of cards left per player per suit

		// Play a card
		void play(card_t c, player_t pl) {
			cardsPlayed[nCardsPlayed] = c;
			whoPlayed[nCardsPlayed] = pl;
			nCardsPlayed++;
			mPlayerHand[pl] ^= bit(c); 
			mCardsLeft ^= bit(c); 
			uSuitLengths -= uint64(1) << 4*(4*pl+suit(c));
		}
		
		// Take back a play
		void unplay() {
			nCardsPlayed--;
			card_t c = cardsPlayed[nCardsPlayed];
			player_t pl = whoPlayed[nCardsPlayed];
			mPlayerHand[pl] ^= bit(c);
			mCardsLeft ^= bit(c);
			uSuitLengths += uint64(1) << 4*(4*pl+suit(c));
		}
		
		// Generate a list of legal moves, with duplicates eliminated (but reported seperately)
		int generateMoves_pl0(player_t, const rankequiv_t&, card_t* moves, uint64* equivalents) const;
		int generateMoves_pl1(player_t, const trickstate_t&, const rankequiv_t&, card_t* moves, uint64* equivalents) const;
		int generateMoves_pl2(player_t, const trickstate_t&, const rankequiv_t&, card_t* moves, uint64* equivalents) const;
		int generateMoves_pl3(player_t, const trickstate_t&, const rankequiv_t&, card_t* moves, uint64* equivalents) const;
	};
}

// Cache for storing results to avoid repeat computation
struct cache_t {
public:
	// Check the cache for a given target. Returns -1 (miss), +1 (hit) or 0 (don't know)
	// If a hit is found, updates the rwmask parameter with the rwmask from the cache
	int check(const gamestate_t&, player_t, uint trickTarget, uint64& rwmask);
	
	// Update when successfully hit trick target
	void update_hit(const gamestate_t&, player_t, uint64 rwmask, uint trickTarget);
	
	// Update when miss trick target
	void update_miss(const gamestate_t&, player_t, uint64 rwmask, uint trickTarget);

	// Clear
	void clear();
	
private:
	
	// Implementation details
	struct result {
		uint64 cardsLeft;		// 1 if played, else 0 (in deck order)
		uint64 rwMask;			// 1 if takes a trick, else 0 (in deck order)
		uint8 upperbound;		// min failed number of tricks
		uint8 lowerbound;		// max succeeded number of tricks
	};
	
	typedef std::vector<result> cache_resl;
	typedef std::map<uint64, cache_resl> data_t;
	
	data_t data[14][4];			// [tricks played][player on lead]
};

// Check the cache for a given target. Returns -1 (miss), +1 (hit) or 0 (don't know)
int cache_t::check(const gamestate_t& state, player_t pl, uint trickTarget, uint64& rwmask)
{
	cache_resl& resl = data[state.nCardsPlayed/4][pl][state.uSuitLengths];
	for (cache_resl::reverse_iterator it = resl.rbegin(); it != resl.rend(); it++) {
		if ((it->rwMask & it->cardsLeft) == (it->rwMask & state.mCardsLeft)) {
			if (trickTarget <= it->lowerbound) { rwmask |= it->rwMask; return +1; }
			if (trickTarget >= it->upperbound) { rwmask |= it->rwMask; return -1; }
		}
	}
	return 0;
}

// Update when successfully hit trick target
void cache_t::update_hit(const gamestate_t& state, player_t pl, uint64 rwmask, uint trickTarget)
{
	cache_resl& resl = data[state.nCardsPlayed/4][pl][state.uSuitLengths];
	result res;
	res.cardsLeft = state.mCardsLeft;
	res.lowerbound = trickTarget;
	res.upperbound = 1 + state.tricksLeft();
	res.rwMask = rwmask;
	resl.push_back(res);
}


// Update when miss trick target
void cache_t::update_miss(const gamestate_t& state, player_t pl, uint64 rwmask, uint trickTarget) 
{
	cache_resl& resl = data[state.nCardsPlayed/4][pl][state.uSuitLengths];
	result res;
	res.cardsLeft = state.mCardsLeft;
	res.lowerbound = 0;
	res.upperbound = trickTarget;
	res.rwMask = rwmask;
	resl.push_back(res);
}

// Clear the cache; used if running low on memory
void cache_t::clear() {
	for (int i = 0; i < 14; ++i) {
		for (int j = 0; j < 4; ++j) {
			data[i][j].clear();
		}
	}
}

namespace {
	
	// This class is used for a single problem only. For analysing the next trick or alternative plays, a new
	// instance of the analyzer must be created.
	class analyzer {
	public:
		// Construction
		analyzer(const deal_t&, const play_t&, cache_t*);
		
		// Is it possible to make the target number of tricks from the current position?
		// The target can be specified in terms of tricks for either side
		bool make(partnership_t who, uint tricktarget);

		// Is it possible to make the target number of tricks from the current position
		// by playing the specified card? (includes the just-completed trick in the trick
		// count if relevant)
		bool make(partnership_t who, uint tricktarget, card_t move);		
		
		// What moves are legal from this position?
		int generate_moves(card_t* moves, uint64* equivalents, player_t &pl);

	private:
		// Internal methods
		bool search_pl0(uint tricktarget, player_t, uint64& rwmask, const rankequiv_t&);
		bool search_pl1(uint tricktarget, player_t, uint64& rwmask, const rankequiv_t&, const trickstate_t&);
		bool search_pl2(uint tricktarget, player_t, uint64& rwmask, const rankequiv_t&, const trickstate_t&);
		bool search_pl3(uint tricktarget, player_t, uint64& rwmask, const rankequiv_t&, const trickstate_t&);
		bool search_t13(player_t, uint64& rwmask);

		// Data
		const suit_t trumps;
		gamestate_t state;
		cache_t* const cache;
	
	public:
		// Current state; set at creation and kept track of during analysis
		rankequiv_t m_rankequiv;
		player_t m_player;
		trickstate_t m_trickstate;
	};

	// Generate moves, ordered according to our (admittedly rubbish) heuristics
	int analyzer::generate_moves(card_t* moves, uint64* equivalents, player_t &pl)
	{
		pl = m_player;
        switch (state.nCardsPlayed & 3) {
            case 0: return state.generateMoves_pl0(pl, m_rankequiv, moves, equivalents);
            case 1: return state.generateMoves_pl1(pl, m_trickstate, m_rankequiv, moves, equivalents);
            case 2: return state.generateMoves_pl2(pl, m_trickstate, m_rankequiv, moves, equivalents);
            default: return state.generateMoves_pl3(pl, m_trickstate, m_rankequiv, moves, equivalents);
        }
	}

	// This is the search function for the start of a trick
	bool analyzer::search_pl0(uint tricktarget, player_t pl, uint64& rwmask, const rankequiv_t& rankequiv)
	{
		// Check for trivialities
		if (tricktarget <= 0) return true; 
		if (tricktarget >= 1 + state.tricksLeft()) return false;
		
		// The last trick is dealt with specially (and not cached)
		if (state.tricksLeft() == 1) {
			return search_t13(pl, rwmask);
		}

		// Check the cache
		int cr = cache->check(state, pl, tricktarget, rwmask);
		if (cr != 0) return (cr > 0);
			
		// If none of those applied, we need to search. Start by enumerating possible moves
		card_t moves[13];
		uint64 equivalents[13];
		int movecount = state.generateMoves_pl0(pl, rankequiv, moves, equivalents);
			
		// Check each move in turn
		uint64 failmask = 0;					// winning cards in failing lines
		int oppotarget = 1 + state.tricksLeft() - tricktarget;
		for (int i = 0; i < movecount; i++) {
			bool thisPlayWorks = false;
			uint64 thismask = 0;
			state.play(moves[i], pl);
			trickstate_t trickstate(pl, moves[i]);
			thisPlayWorks = !search_pl1(oppotarget, nextpl(pl), thismask, rankequiv, trickstate);
			state.unplay();
			if ((thismask & equivalents[i]) != 0) thismask |= sameRankOrHigher[moves[i]];
			if (thisPlayWorks) {
				cache->update_hit(state, pl, thismask, tricktarget);
				rwmask |= thismask;
				return true;
			} else {
				failmask |= thismask;
			}
		}

		// If we get here, we didn't find a winning move
		cache->update_miss(state, pl, failmask, tricktarget);
		rwmask |= failmask;
		return false;
	}

	// This is the search function for trick 13; returns true if leader wins it
	bool analyzer::search_t13(player_t pl, uint64& rwmask)
	{
		trickstate_t trickstate(pl, bitindex(state.mPlayerHand[pl]));	   player_t thispl = nextpl(pl);
		trickstate.play(thispl, bitindex(state.mPlayerHand[thispl]), trumps);	thispl = nextpl(thispl);
		trickstate.play(thispl, bitindex(state.mPlayerHand[thispl]), trumps);	thispl = nextpl(thispl);
		trickstate.play(thispl, bitindex(state.mPlayerHand[thispl]), trumps);			
		if (trickstate.ranktrick) rwmask |= sameRankOrHigher[trickstate.winningcard];
		return (partnership(pl) == partnership(trickstate.winner));
	}

	// This is the search function for the second player to play to the trick
	bool analyzer::search_pl1(uint tricktarget, player_t pl, uint64& rwmask, const rankequiv_t& rankequiv, const trickstate_t& trickstate)
	{
		// Enumerate possible moves
		card_t moves[13];
		uint64 equivalents[13];
		int movecount = state.generateMoves_pl1(pl, trickstate, rankequiv, moves, equivalents);
		
		// Prepare for searching
		int oppotarget = 1 + state.tricksLeft() - tricktarget;	
		uint64 failmask = 0;					// winning cards in failing lines
		
		// Try each move in turn
		for (int i = 0; i < movecount; i++) {
			bool thisPlayWorks = false;
			uint64 thismask = 0;
			state.play(moves[i], pl);
			trickstate_t trickstate_next = trickstate;
			trickstate_next.play(pl, moves[i], trumps);
			thisPlayWorks = !search_pl2(oppotarget, nextpl(pl), thismask, rankequiv, trickstate_next);
			state.unplay();
			if ((thismask & equivalents[i]) != 0) thismask |= sameRankOrHigher[moves[i]];
			if (thisPlayWorks) {
				rwmask |= thismask;
				return true;
			} else {
				failmask |= thismask;
			}
		}
		
		// If we get here, we didn't find a winning move
		rwmask |= failmask;
		return false;
	}

	// This is the search function for the third player to play to the trick
	bool analyzer::search_pl2(uint tricktarget, player_t pl, uint64& rwmask, const rankequiv_t& rankequiv, const trickstate_t& trickstate)
	{
		// Enumerate possible moves
		card_t moves[13];
		uint64 equivalents[13];
		int movecount = state.generateMoves_pl2(pl, trickstate, rankequiv, moves, equivalents);
		
		// Prepare for searching
		int oppotarget = 1 + state.tricksLeft() - tricktarget;	
		uint64 failmask = 0;					// winning cards in failing lines
		
		// Try each move in turn
		for (int i = 0; i < movecount; i++) {
			bool thisPlayWorks = false;
			uint64 thismask = 0;
			state.play(moves[i], pl);
			trickstate_t trickstate_next = trickstate;
			trickstate_next.play(pl, moves[i], trumps);		
			thisPlayWorks = !search_pl3(oppotarget, nextpl(pl), thismask, rankequiv, trickstate_next);
			state.unplay();
			if ((thismask & equivalents[i]) != 0) thismask |= sameRankOrHigher[moves[i]];
			if (thisPlayWorks) {
				rwmask |= thismask;
				return true;
			} else {
				failmask |= thismask;
			}
		}
		
		// If we get here, we didn't find a winning move
		rwmask |= failmask;
		return false;
	}


	// This is the search function for the last player to play to the trick
	bool analyzer::search_pl3(uint tricktarget, player_t pl, uint64& rwmask, const rankequiv_t& rankequiv, const trickstate_t& trickstate)
	{
		// Enumerate possible moves
		card_t moves[13];
		uint64 equivalents[13];
		int movecount = state.generateMoves_pl3(pl, trickstate, rankequiv, moves, equivalents);
		
		// Prepare for searching
		int oppotarget = 1 + state.tricksLeft() - tricktarget;	
		uint64 failmask = 0;					// winning cards in failing lines
		
		// Update rank equivalence
		rankequiv_t rankequiv_next = rankequiv;
		for (int ci = state.nCardsPlayed - 4; ci < state.nCardsPlayed; ci++) rankequiv_next.play(state.cardsPlayed[ci]);
		
		// Try each move
		for (int i = 0; i < movecount; i++) {
			bool thisPlayWorks = false;
			uint64 thismask = 0;
			state.play(moves[i], pl);
			rankequiv_next.play(moves[i]);
			trickstate_t trickstate_this = trickstate;
			trickstate_this.play(pl, moves[i], trumps);
			if (partnership(pl) == partnership(trickstate_this.winner)) {
				thisPlayWorks = search_pl0(tricktarget - 1, trickstate_this.winner, thismask, rankequiv_next);
			} else {
				thisPlayWorks = !search_pl0(oppotarget - 1, trickstate_this.winner, thismask, rankequiv_next);
			}
			if (trickstate_this.ranktrick) thismask |= sameRankOrHigher[trickstate_this.winningcard];
			if ((thismask & equivalents[i]) != 0) thismask |= sameRankOrHigher[moves[i]];
			state.unplay();
			rankequiv_next.unplay(moves[i]);
			if (thisPlayWorks) {
				rwmask |= thismask;
				return true;
			} else {
				failmask |= thismask;
			}
		}
		
		// If we get here, we didn't find a winning move
		rwmask |= failmask;
		return false;
	}

	// Delegates to the appropriate player-specialised search method
	bool analyzer::make(partnership_t who, uint tricktarget)
	{
		if (tricktarget <= 0) return true;
		uint64 rwmask = 0;
		if (partnership(m_player) == who) {
			switch (state.nCardsPlayed % 4) {
				case 0: return search_pl0(tricktarget, m_player, rwmask, m_rankequiv);
				case 1: return search_pl1(tricktarget, m_player, rwmask, m_rankequiv, m_trickstate);
				case 2: return search_pl2(tricktarget, m_player, rwmask, m_rankequiv, m_trickstate);
				default: return search_pl3(tricktarget, m_player, rwmask, m_rankequiv, m_trickstate);
			}
		} else {
			int oppotarget = 1 + state.tricksLeft() - tricktarget;	
			switch (state.nCardsPlayed % 4) {
				case 0: return !search_pl0(oppotarget, m_player, rwmask, m_rankequiv);
				case 1: return !search_pl1(oppotarget, m_player, rwmask, m_rankequiv, m_trickstate);
				case 2: return !search_pl2(oppotarget, m_player, rwmask, m_rankequiv, m_trickstate);
				default: return !search_pl3(oppotarget, m_player, rwmask, m_rankequiv, m_trickstate);
			}
		}
	}

	// Can we make this many tricks having made this move?
	// Includes the just-completed trick in the trick count
	bool analyzer::make(partnership_t who, uint tricktarget, card_t move)
	{
		trickstate_t saved_trickstate = m_trickstate;
		player_t saved_pl = m_player;
		rankequiv_t saved_rankequiv = m_rankequiv;

		state.play(move, m_player);
		if (state.nCardsPlayed%4 == 1) {
			m_trickstate = trickstate_t(m_player, move);
			m_player = nextpl(m_player);
		} else {
			m_trickstate.play(m_player, move, trumps);
			m_player = nextpl(m_player);
			if (state.nCardsPlayed%4 == 0) {
				for (int ci = state.nCardsPlayed - 4; ci < state.nCardsPlayed; ci++) 
					m_rankequiv.play(state.cardsPlayed[ci]);
				if (partnership(m_trickstate.winner) == who) tricktarget--;
				m_player = m_trickstate.winner; 
			}
		}

		bool rv = make(who, tricktarget);
		
		m_trickstate = saved_trickstate;
		m_rankequiv = saved_rankequiv;
		state.unplay();
		m_player = saved_pl;
		return rv;
	}
	
	// Constructor
	analyzer::analyzer(const deal_t& deal, const play_t& play, cache_t* cache) : state(deal, play), trumps(deal.trumps), cache(cache), m_rankequiv(play)
	{
		m_player = nextpl(deal.declarer);
		for (int i = 0; i < play.nCardsPlayed; ++i) {
			card_t c = play.played[i];
			if (i % 4 == 0) m_trickstate = trickstate_t(m_player, c);
			else m_trickstate.play(m_player, c, trumps);
			m_player = (i % 4 == 3) ? m_trickstate.winner : nextpl(m_player);
		}
	}

	// Generate all possible unique moves for the first player to play to a trick
	int gamestate_t::generateMoves_pl0(player_t pl, const rankequiv_t& rankequiv, card_t* moves, uint64* equivalents) const
	{
		// Generate unique moves per suit, keeping track of equivalent alternatives
		card_t suitmoves[4][13];
		uint64 suitrankequivs[4][13];
		int suitmovecount[4];
		for (int s = 0; s < 4; s++) {
			suitmovecount[s] = 0;
			uint64 suit = mPlayerHand[pl] & suitmask[s];
			card_t lastmove = -1;
			while (suit) {
				uint64 mcard = lsb(suit);
				suit ^= mcard;
				card_t thismove = bitindex(mcard);
				if ((lastmove == -1) || (rankequiv.nexthigher[lastmove] != thismove)) {
					suitmoves[s][suitmovecount[s]] = thismove;
					suitrankequivs[s][suitmovecount[s]] = mcard;
					suitmovecount[s]++;
				} else {
					suitrankequivs[s][suitmovecount[s]-1] |= mcard;
				}
				lastmove = thismove;
			}
		}
		
		// Assemble moves in preferred order
		int movecount = 0;

		// Highest card in each suit
		for (int s = 0; s < 4; s++)	if (suitmovecount[s] > 0) {
			moves[movecount] = suitmoves[s][suitmovecount[s]-1];		
			equivalents[movecount] = suitrankequivs[s][suitmovecount[s]-1];
			movecount++;
		}
			
		// Lowest card in each suit	
		for (int s = 0; s < 4; s++)	if (suitmovecount[s] > 1) {
			moves[movecount] = suitmoves[s][0];							
			equivalents[movecount] = suitrankequivs[s][0];
			movecount++;
		}
			
		// Interior cards
		for (int s = 0; s < 4; s++)	if (suitmovecount[s] > 2) for (int i = 1; i < suitmovecount[s]-1; i++) {
			moves[movecount] = suitmoves[s][i];							
			equivalents[movecount] = suitrankequivs[s][i];
			movecount++;
		}

		// Done
		return movecount;
	}

	// Generate all possible unique moves for a player (not the first to play to the trick)
	int gamestate_t::generateMoves_pl1(player_t pl, const trickstate_t& trickstate, const rankequiv_t& rankequiv, card_t* moves, uint64* equivalents) const
	{
		// Kep count of the moves
		int movecount = 0;
		
		// If we can follow suit, then do that
		uint64 suit = mPlayerHand[pl] & suitmask[trickstate.ledsuit];
		if (suit) {
			
			// Generate unique moves (lowest card first)
			card_t suitmoves[13];
			uint64 suitequivalents[13];
			card_t lastmove = -1;
			while (suit) {
				uint64 mcard = lsb(suit);
				suit ^= mcard;
				card_t thismove = bitindex(mcard);
				if (rankequiv.nextlower[thismove] != lastmove) {
					suitmoves[movecount] = thismove;
					suitequivalents[movecount] = mcard;
					movecount++;
				} else {
					suitequivalents[movecount-1] |= mcard;
				}
				lastmove = thismove;
			}
			
			// Order moves - first highest...
			moves[0] = suitmoves[movecount-1];
			equivalents[0] = suitequivalents[movecount-1];
			
			// ...then lowest...
			if (movecount > 1) {
				moves[1] = suitmoves[0];
				equivalents[1] = suitequivalents[0];
			}
			
			// ..then top-down.
			for (int i = 2; i < movecount; i++) {
				moves[i] = suitmoves[movecount-i];
				equivalents[i] = suitequivalents[movecount-i];
			}
		}
		
		// Otherwise, consider other suits
		else {
			// Assemble unique moves in each suit (lowest first)
			card_t suitmoves[4][13];
			int suitmovecount[4];
			uint64 suitequivalents[4][13];
			for (int s = 0; s < 4; s++) {
				suitmovecount[s] = 0;
				card_t lastmove = -1;
				uint64 suit = mPlayerHand[pl] & suitmask[s];
				while (suit) {
					uint64 mcard = lsb(suit);
					suit ^= mcard;
					card_t thismove = bitindex(mcard);
					if (rankequiv.nextlower[thismove] != lastmove) {
						suitequivalents[s][suitmovecount[s]] = mcard;
						suitmoves[s][suitmovecount[s]] = thismove;
						suitmovecount[s]++;
					} else {
						suitequivalents[s][suitmovecount[s]-1] |= mcard;
					}
					lastmove = thismove;
				}
			}
			
			// First, a low ruff
			if ((trumps != nt) && (suitmovecount[trumps] > 0)) {
				moves[movecount] = suitmoves[trumps][0];
				equivalents[movecount] = suitequivalents[trumps][0];
				movecount++; 
			}
			
			// Then a low discard from each non-trump suit
			for (int s = 0; s < 4; s++) {
				if (s == trumps) continue;
				if (suitmovecount[s] > 0) {
					moves[movecount] = suitmoves[s][0];
					equivalents[movecount] = suitequivalents[s][0];
					movecount++;
				}
			}
			
			// Then higher ruffs
			if (trumps != nt)
				for (int i = 1; i < suitmovecount[trumps]; i++) {
					moves[movecount] = suitmoves[trumps][i];
					equivalents[movecount] = suitequivalents[trumps][i];
					movecount++;
				}
			
			// Then higher discards
			for (int s = 0; s < 4; s++) {
				if (s == trumps) continue;
				for (int i = 1; i < suitmovecount[s]; i++) {
					moves[movecount] = suitmoves[s][i];
					equivalents[movecount] = suitequivalents[s][i];
					movecount++;
				}
			}
		}
		
		// Return the number of moves
		return movecount;
	}

	// Generate all possible unique moves for third hand
	int gamestate_t::generateMoves_pl2(player_t pl, const trickstate_t& trickstate, const rankequiv_t& rankequiv, card_t* moves, uint64* equivalents) const
	{
		// Kep count of the moves
		int movecount = 0;
		
		// If we can follow suit, then do that
		uint64 suit = mPlayerHand[pl] & suitmask[trickstate.ledsuit];
		if (suit) {
			
			// Generate unique moves (lowest card first)
			card_t suitmoves[13];
			uint64 suitequivalents[13];
			card_t lastmove = -1;
			while (suit) {
				uint64 mcard = lsb(suit);
				suit ^= mcard;
				card_t thismove = bitindex(mcard);
				if (rankequiv.nextlower[thismove] != lastmove) {
					suitmoves[movecount] = thismove;
					suitequivalents[movecount] = mcard;
					movecount++;
				} else {
					suitequivalents[movecount-1] |= mcard;
				}
				lastmove = thismove;
			}
			
            // If we can't beat the highest card played so far, then just order cards from the bottom up
            if ((suitmoves[movecount-1] < trickstate.winningcard) || (trickstate.winsuit != trickstate.ledsuit)) {
            BOTTOM_UP:
                for (int i = 0; i < movecount; i++) {
                    moves[i] = suitmoves[i];
                    equivalents[i] = suitequivalents[i];
                }
                return movecount;
            }
            
            // Check the best card 4th hand has
            uint64 suit_pl3 = mPlayerHand[nextpl(pl)] & suitmask[trickstate.ledsuit];
            
            // If 4th hand can't follow suit, then
            if (!suit_pl3) {
                // If partner's winning, order from the bottom up
                if (trickstate.winner == partner(pl)) goto BOTTOM_UP;
                // Otherwise, find the cheapest winner we have
                int winner;
                for (winner = 0; (winner < movecount) && (trickstate.winningcard > suitmoves[winner]); ++winner);
                moves[0] = suitmoves[winner];
                equivalents[0] = suitequivalents[winner];
                // Then bottom up
                for (int i = 0; i < winner; i++) {
                    moves[i+1] = suitmoves[i];
                    equivalents[i+1] = suitequivalents[i];
                }
                for (int i = winner+1; i < movecount; i++) {
                    moves[i] = suitmoves[i];
                    equivalents[i] = suitequivalents[i];
                }
            
            } else {
                // If partner is winning and 4th hand can't beat him, then from the bottom up
                if (trickstate.winner == partner(pl) && bit(trickstate.winningcard) > suit_pl3) goto BOTTOM_UP;
                // If we can beat 4th hand
                if (bit(suitmoves[movecount-1]) > suit_pl3) {
                    // Find the cheapest winner
                    int winner;
                    for (winner = 0; (winner < movecount) && (trickstate.winningcard > suitmoves[winner]); ++winner);
                    // From cheap winner downwards
                    for (int i = 0; i <= winner; i++) {
                        moves[i] = suitmoves[winner-i];
                        equivalents[i] = suitequivalents[winner-i];
                    }
                    // From there up
                    for (int i = winner+1; i < movecount; i++) {
                        moves[i] = suitmoves[i];
                        equivalents[i] = suitequivalents[i];
                    }
                } else {
                    // Otherwise
                    // Top down to current highest card in the trick
                    int i;
                    for (i = 0; i < movecount; ++i)
                    {
                        if (suitmoves[movecount-1-i] < trickstate.winningcard) break;
                        moves[i] = suitmoves[movecount-1-i];
                        equivalents[i] = suitequivalents[movecount-1-i];
                    }
                    // Then bottom up
                    int j = 0;
                    while (i < movecount)
                    {
                        moves[i] = suitmoves[j];
                        equivalents[i] = suitequivalents[j];                        
                        i++;
                        j++;
                    }
                }
            }
		}
		
		// Otherwise, consider other suits
		else {
			// Assemble unique moves in each suit (lowest first)
			card_t suitmoves[4][13];
			int suitmovecount[4];
			uint64 suitequivalents[4][13];
			for (int s = 0; s < 4; s++) {
				suitmovecount[s] = 0;
				card_t lastmove = -1;
				uint64 suit = mPlayerHand[pl] & suitmask[s];
				while (suit) {
					uint64 mcard = lsb(suit);
					suit ^= mcard;
					card_t thismove = bitindex(mcard);
					if (rankequiv.nextlower[thismove] != lastmove) {
						suitequivalents[s][suitmovecount[s]] = mcard;
						suitmoves[s][suitmovecount[s]] = thismove;
						suitmovecount[s]++;
					} else {
						suitequivalents[s][suitmovecount[s]-1] |= mcard;
					}
					lastmove = thismove;
				}
			}
			
			// First, a low ruff
			if ((trumps != nt) && (suitmovecount[trumps] > 0)) {
				moves[movecount] = suitmoves[trumps][0];
				equivalents[movecount] = suitequivalents[trumps][0];
				movecount++; 
			}
			
			// Then a low discard from each non-trump suit
			for (int s = 0; s < 4; s++) {
				if (s == trumps) continue;
				if (suitmovecount[s] > 0) {
					moves[movecount] = suitmoves[s][0];
					equivalents[movecount] = suitequivalents[s][0];
					movecount++;
				}
			}
			
			// Then higher ruffs
			if (trumps != nt)
				for (int i = 1; i < suitmovecount[trumps]; i++) {
					moves[movecount] = suitmoves[trumps][i];
					equivalents[movecount] = suitequivalents[trumps][i];
					movecount++;
				}
			
			// Then higher discards
			for (int s = 0; s < 4; s++) {
				if (s == trumps) continue;
				for (int i = 1; i < suitmovecount[s]; i++) {
					moves[movecount] = suitmoves[s][i];
					equivalents[movecount] = suitequivalents[s][i];
					movecount++;
				}
			}
		}
		
		// Return the number of moves
		return movecount;
	}

    // Generate all possible unique moves for a player (last to play)
	int gamestate_t::generateMoves_pl3(player_t pl, const trickstate_t& trickstate, const rankequiv_t& rankequiv, card_t* moves, uint64* equivalents) const
	{
		// Kep count of the moves
		int movecount = 0;
		
		// If we can follow suit, then do that
		uint64 suit = mPlayerHand[pl] & suitmask[trickstate.ledsuit];
		if (suit) {
			
			// Generate unique moves (lowest card first)
			card_t suitmoves[13];
			uint64 suitequivalents[13];
			card_t lastmove = -1;
			while (suit) {
				uint64 mcard = lsb(suit);
				suit ^= mcard;
				card_t thismove = bitindex(mcard);
				if (rankequiv.nextlower[thismove] != lastmove) {
					suitmoves[movecount] = thismove;
					suitequivalents[movecount] = mcard;
					movecount++;
				} else {
					suitequivalents[movecount-1] |= mcard;
				}
				lastmove = thismove;
			}
			
            // Has the trick been ruffed? If so, follow low first
            if (trickstate.winsuit != trickstate.ledsuit) {
                for (int i = 0; i < movecount; ++i) {
                    moves[i] = suitmoves[i];
                    equivalents[i] = suitequivalents[i];
                }
            } else {
                // Find our cheapest winner (if any)
                int winner;
                for (winner = 0; (winner < movecount) && (trickstate.winningcard > suitmoves[winner]); ++winner);
                
                // Is partner winning the trick? If so, try our lowest card first (but next choice will be to overtake)
                int imove = 0;
                int start = 0;
                if ((trickstate.winner == partner(pl)) && (winner > 0)) {
                    moves[0] = suitmoves[0];
                    equivalents[0] = suitequivalents[0];
                    imove = 1;
                    start = 1;
                }
                
                // Can we win cheaply?
                if (winner < movecount) {
                    moves[imove] = suitmoves[winner];
                    equivalents[imove] = suitequivalents[winner];
                    ++imove;
                }
                
                // Then higher cards
                for (int i = winner+1; i < movecount; ++i) {
                    moves[imove] = suitmoves[i];
                    equivalents[imove] = suitequivalents[i];
                    ++imove;
                }
                
                // Finally lower cards (lowest first)
                for (int i = start; i < winner; ++i) {
                    moves[imove] = suitmoves[i];
                    equivalents[imove] = suitequivalents[i];
                    ++imove;
                }
            }
		}
		
		// Otherwise, consider other suits
		else {
			// Assemble unique moves in each suit (lowest first)
			card_t suitmoves[4][13];
			int suitmovecount[4];
			uint64 suitequivalents[4][13];
			for (int s = 0; s < 4; s++) {
				suitmovecount[s] = 0;
				card_t lastmove = -1;
				uint64 suit = mPlayerHand[pl] & suitmask[s];
				while (suit) {
					uint64 mcard = lsb(suit);
					suit ^= mcard;
					card_t thismove = bitindex(mcard);
					if (rankequiv.nextlower[thismove] != lastmove) {
						suitequivalents[s][suitmovecount[s]] = mcard;
						suitmoves[s][suitmovecount[s]] = thismove;
						suitmovecount[s]++;
					} else {
						suitequivalents[s][suitmovecount[s]-1] |= mcard;
					}
					lastmove = thismove;
				}
			}
			
			// First, a low ruff / overruff
            int ruffer = -1;
			if ((trumps != nt) && (suitmovecount[trumps] > 0)) {
                if (trickstate.winsuit == trumps) {
                    for (int i = 0; i < suitmovecount[trumps]; ++i) {
                        if (suitmoves[trumps][i] > trickstate.winningcard) {
                            moves[movecount] = suitmoves[trumps][i];
                            equivalents[movecount] = suitequivalents[trumps][i];
                            ruffer = i;
                            movecount++;
                            break;
                        }
                    }
                } else {
                    moves[movecount] = suitmoves[trumps][0];
                    equivalents[movecount] = suitequivalents[trumps][0];
                    movecount++; 
                    ruffer = 0;
                }
			}
			
			// Then a low discard from each non-trump suit
			for (int s = 0; s < 4; s++) {
				if (s == trumps) continue;
				if (suitmovecount[s] > 0) {
					moves[movecount] = suitmoves[s][0];
					equivalents[movecount] = suitequivalents[s][0];
					movecount++;
				}
			}
			
			// Then higher ruffs
			if (trumps != nt)
				for (int i = 0; i < suitmovecount[trumps]; i++) {
                    if (i != ruffer) {
                        moves[movecount] = suitmoves[trumps][i];
                        equivalents[movecount] = suitequivalents[trumps][i];
                        movecount++;
                    }
				}
			
			// Then higher discards
			for (int s = 0; s < 4; s++) {
				if (s == trumps) continue;
				for (int i = 1; i < suitmovecount[s]; i++) {
					moves[movecount] = suitmoves[s][i];
					equivalents[movecount] = suitequivalents[s][i];
					movecount++;
				}
			}
		}
		
		// Return the number of moves
		return movecount;
	}

    // Construct the game state
	gamestate_t::gamestate_t(const deal_t& deal, const play_t& playrecord) 
		: nCardsPlayed(0), mCardsLeft(0), uSuitLengths(0), trumps(deal.trumps)
	{
		for (int pl = 0; pl < 4; pl++) {
			mPlayerHand[pl] = 0;
		}
		int nCardsDealt = 0;
		for (int i = 0; i < 52; ++i) {
			suit_t s = suit(i);
			player_t pl = deal.holder[i];
			if (pl != plNone) {
				uSuitLengths += uint64(1) << 4*(4*pl+s);
				mCardsLeft |= bit(i);
				mPlayerHand[pl] |= bit(i);
				nCardsDealt++;
			}
		}	
		nCardsEach = nCardsDealt / 4;
		for (int i = 0; i < playrecord.nCardsPlayed; ++i) {
			card_t c = playrecord.played[i];
			play(c, deal.holder[c]);
		}
	}
}

void update_hit(card_t move, uint64 equivs, position_analysis_t* rv, int goal);
void update_hit(card_t move, uint64 equivs, position_analysis_t* rv, int goal)
{
	rv->play[move].low = goal;
	uint64 equiv = equivs;
	while (equiv) {
		uint64 e = lsb(equiv);
		rv->play[bitindex(e)].low = goal; 
		equiv ^= e;
	}
	if (rv->global.low < goal) rv->global.low = goal;
}

void update_miss(card_t move, uint64 equivs, position_analysis_t* rv, int goal);
void update_miss(card_t move, uint64 equivs, position_analysis_t* rv, int goal)
{
	rv->play[move].high = goal;
	uint64 equiv = equivs;
	while (equiv) {
		uint64 e = lsb(equiv);
		rv->play[bitindex(e)].high = goal; 
		equiv ^= e;
	}
}

// Analyze all moves from a position
void analyze(const deal_t* deal, const play_t* play, cache_t* cache, callback_t callback, position_analysis_t* rv, bool analyze_moves)
{
	// Assemble moves
	card_t moves[13];
	uint64 equivalents[13];
	player_t pl;
	analyzer a(*deal, *play, cache);
	int movecount = a.generate_moves(moves, equivalents, pl);
	partnership_t who = partnership(pl);

	// Initial bounds for each move
	for (int i = 0; i < movecount; ++i) {
		int wontricks = (play->nCardsPlayed%4==3) ? a.m_trickstate.would_win(pl, moves[i], deal->trumps) : 0;
		int maxtricks = rv->global.high;
		update_hit(moves[i], equivalents[i], rv, wontricks);
		update_miss(moves[i], equivalents[i], rv, maxtricks);
	}
		
	// First phase - find the best move
	while (rv->global.low+1 < rv->global.high) {
		int goal = (rv->global.low + rv->global.high) / 2;
		for (int i = 0; i < movecount; ++i) {
			card_t move = moves[i];
			if (goal < rv->play[move].high) {
				if (a.make(who, goal, move)) {
					update_hit(move, equivalents[i], rv, goal);
					goto NEXT;
				} else {
					update_miss(move, equivalents[i], rv, goal);
				}
				if (callback && !callback(rv)) return;
			}
		}
		rv->global.high = goal;
NEXT:	if (callback && !callback(rv)) return;
	}
	if (!analyze_moves) return;
	
	// Second phase - figure out results for all the other cards
	for (int i = 0; i < movecount; ++i) {
		card_t move = moves[i];
		while (rv->play[move].low+1 < rv->play[move].high) {
			int goal = (rv->play[move].low + rv->play[move].high) / 2;
			if (a.make(who, goal, move)) {
				update_hit(move, equivalents[i], rv, goal);
			} else {
				update_miss(move, equivalents[i], rv, goal);
			}
			if (callback && !callback(rv)) return;
		}
	}
}

// Create an empty cache
cache_t* new_cache() { return new cache_t; }

// Delete a cache
void free_cache(cache_t* p) { delete p; }

// Clear a cache (used when low on memory)
void clear_cache(cache_t* p) { p->clear(); }

// Copy a cache
cache_t* clone_cache(cache_t* p) { return new cache_t(*p); }

// Tells the user-interface what it nees to know about the play so far
void dealstate(const deal_t* deal, const play_t* play, dealstate_t* dealstate, int quitted)
{
	// Figure out what got dealt
	for (int c = 0; c < 52; ++c) {
		dealstate->cardstate[c] = (deal->holder[c] == plNone) ? notdealt : unplayed;
	}
	dealstate->trickswon[pNS] = 0;
	dealstate->trickswon[pEW] = 0;
	
	// Follow along with the play
	int startoftrick = (play->nCardsPlayed - (quitted ? 0 : 1)) & ~3;
	trickstate_t trickstate;
	dealstate->pl = nextpl(deal->declarer);
	for (int i = 0; i < play->nCardsPlayed; ++i) {
		card_t c = play->played[i];
		if (i % 4 == 0) trickstate = trickstate_t(dealstate->pl, c); else trickstate.play(dealstate->pl, c, deal->trumps);
		dealstate->cardstate[c] = (i >= startoftrick) ? playedthistrick : playedprevtrick;
		dealstate->pl = nextpl(dealstate->pl);
		if (i % 4 == 3) {
			dealstate->pl = trickstate.winner;
			dealstate->trickswon[partnership(trickstate.winner)]++;
		}
	}
	
	// Figure out which plays are legal
	bool anythinggoes = true;
	if (play->nCardsPlayed % 4 > 0) {
		for (int r = 0; r < 13; ++r) {
			card_t c = card(trickstate.ledsuit, r);
			if ((dealstate->cardstate[c] == unplayed) && (deal->holder[c] == dealstate->pl)) {
				dealstate->cardstate[c] = playable;
				anythinggoes = false;
			}
		}
	}
	if (anythinggoes) {
		for (int c = 0; c < 52; ++c) {
			if ((dealstate->cardstate[c] == unplayed) && (deal->holder[c] == dealstate->pl)) {
				dealstate->cardstate[c] = playable;
			}
		}
	}
}

// Generate a random deal
void randomdeal(deal_t* deal)
{
	card_t deck[52];
	for (int i = 0; i < 52; ++i)
		deck[i] = i;
	for (int i = 0; i < 51; ++i) {
		int j = rand() % (52-i);
		std::swap(deck[i], deck[i+j]);
	}
	for (int i = 0; i < 52; ++i)
		deal->holder[deck[i]] = player_t(i%4);
    deal->trumps = nt;
    deal->declarer = plS;
}

