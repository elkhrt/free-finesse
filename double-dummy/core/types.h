// This file is part of FreeFinesse, a double-dummy analyzer (c) Edward Lockhart, 2010
// It is made available under the GPL; see the file COPYING for details

//
//  Common type declarations and utility functions
//

#pragma once

// Player
typedef enum player_t { plN, plE, plS, plW, plNone } player_t;
static inline player_t nextpl(player_t pl) { return (player_t)((pl+1)%4); }
static inline player_t prevpl(player_t pl) { return (player_t)((pl+3)%4); }
static inline player_t partner(player_t pl) { return (player_t)(pl^2); }

// Partnership
typedef enum partnership_t { pNS, pEW } partnership_t;
static inline partnership_t partnership(player_t pl) { return (partnership_t)(pl&1); }

// Suits
typedef enum suit_t { cx, dx, hx, sx, nt } suit_t;

// Ranks
typedef int rank_t;

// Cards; construction
typedef int card_t;
static inline card_t card(suit_t suit, rank_t rank) { return (rank*4)+suit; }

// Cards; interpetation
static inline suit_t suit(card_t c) { return (suit_t)(c&3); }
static inline rank_t rank(card_t c) { return (rank_t)(c/4); }

// Definition of a deal
typedef struct deal_t {
	int board;
	player_t declarer;
	suit_t trumps;
	player_t holder[52];
} deal_t;

// The play so far
typedef struct play_t {
	int nCardsPlayed;
	card_t played[52];
} play_t;

// The current state of the deal, computed from the deal and the play
typedef enum cardstate_t { notdealt, playedthistrick, playedprevtrick, playable, unplayed } cardstate_t;
typedef struct dealstate_t {
	player_t pl;
	int trickswon[2];
	cardstate_t cardstate[52];
} dealstate_t;

// Serialisation
#ifdef __cplusplus
extern "C" {
#endif

#define DEAL_SIZE 128
void deserialise_deal(deal_t* deal, const char* buff);
void serialise_deal(const deal_t* deal, char* buff);

#define PLAY_SIZE 128
void deserialise_play(play_t* play, const char* buff);
void serialise_play(const play_t* play, char* buff);
	
#ifdef __cplusplus
}
#endif
