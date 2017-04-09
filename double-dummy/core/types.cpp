// This file is part of FreeFinesse, a double-dummy analyzer (c) Edward Lockhart, 2010
// It is made available under the GPL; see the file COPYING for details

//
//  Utility functions for dealing with common types
//

#include "types.h"
#include <string>

rank_t rank(char c);
rank_t rank(char c) { return rank_t(std::string("23456789TJQKA").find(toupper(c))); }
suit_t suit(char c);
suit_t suit(char c) { return suit_t(std::string("CDHSN").find(toupper(c))); }
player_t player(char c);
player_t player(char c) { return player_t(std::string("NESW").find(toupper(c))); }
char ranktext(rank_t r);
char ranktext(rank_t r) { return "23456789TJQKA"[r]; }
char suittext(suit_t s);
char suittext(suit_t s) { return "CDHSN"[s]; }
char playertext(player_t pl);
char playertext(player_t pl) { return "NESW"[pl]; }
char csuittext(card_t c);
char csuittext(card_t c) { return suittext(suit(c)); }
char cranktext(card_t c);
char cranktext(card_t c) { return ranktext(rank(c)); }

// The format for the play is just a sequence of cards
void deserialise_play(play_t* play, const char* buff)
{
	const char *p = buff;
	play->nCardsPlayed = 0;
	while (*p) {
		char csuit = *(p++);
		char crank = *(p++);
		play->played[play->nCardsPlayed] = card(suit(csuit), rank(crank));
		play->nCardsPlayed++;
	}
}

void serialise_play(const play_t* play, char* buff)
{
	char* p = buff;
	for (int i = 0; i < play->nCardsPlayed; ++i) {
		*(p++) = csuittext(play->played[i]);
		*(p++) = ranktext(play->played[i]);
	}
	*p = 0;
}

// The deal is in the format of the hand for each player (NESW), followed by the declarer and trump suit
void deserialise_deal(deal_t* deal, const char* buff)
{
	const char *p = buff;
	player_t pl = plN;
	suit_t s = sx;
	while ((*p) != ':') {
		if ((*p) == '.') s = suit_t(int(s)-1);
		else if ((*p) == ' ') { s = sx; pl = nextpl(pl); }
		else {
			rank_t r = rank(*p);
			deal->holder[card(s, r)] = pl;
		}
		p++;
	}
	deal->declarer = player(*(++p));
	deal->trumps = suit(*(++p));
}

void serialise_deal(const deal_t* deal, char* buff)
{
	char *p = buff;
	for (int pl = 0; pl < 4; ++pl) {
		for (int s = 0; s < 3; ++s) {
			if (s > 0) *(p++) = '.';
			for (int r = 0; r < 13; ++r) {
				card_t c = card(suit_t(s), rank_t(r));
				if (deal->holder[c] == pl) {
					*(p++) = ranktext(c);
				}
			}
		}
		*(p++) = ' ';
	}
	*(p++) = suittext(deal->trumps);
	*(p++) = playertext(deal->declarer);
	*p = 0;
}

