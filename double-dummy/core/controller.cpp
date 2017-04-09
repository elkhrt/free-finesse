// This file is part of FreeFinesse, a double-dummy analyzer (c) Edward Lockhart, 2010
// It is made available under the GPL; see the file COPYING for details

#include "controller.h"

namespace {
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
    };
    
    void init_position_analysis(position_analysis_t* pos, int tricksleft)
    {
        bound_t bound;
        bound.low = 0;
        bound.high = tricksleft+1;
        pos->global = bound;
        for (int i = 0; i < 52; ++i)
            pos->play[i] = bound;
    }
    
    card_annotation_t annotation(card_t c, position_analysis_t* analysis)
    {
        card_annotation_t rv;
        bound_t bound = analysis->play[c];
        rv.best = (bound.low == analysis->global.high-1);
        rv.has_label = true;
        rv.label = bound;
        rv.playable = true;
        return rv;
    }
    
    annotated_card_t playable_card(card_t c, position_analysis_t* analysis)
    {
        annotated_card_t rv;
        rv.has_card = true;
        rv.card = c;
		rv.annotation = annotation(c, analysis);
        return rv;
    }

    card_annotation_t annotation_unplayable()
    {
        card_annotation_t rv;
        rv.best = false;
        rv.has_label = false;
        rv.playable = false;  
        return rv;
    }
    
    annotated_card_t nonplayable_card(card_t c)
    {
        annotated_card_t rv;
        rv.has_card = true;
        rv.card = c;
        rv.annotation = annotation_unplayable();
        return rv;
    }

    annotated_card_t no_card()
    {
        annotated_card_t rv;
        rv.has_card = false;
        rv.card = 0;
        rv.annotation = annotation_unplayable();
        return rv;
    }
    
    void init_changes(card_changes_t* changes)
    {
        changes->num_changes = 0;
        changes->pause_after = -1;
    }
    
    void add_change(card_changes_t* p, card_change_t change)
    {
        p->changes[p->num_changes++] = change;
    }
    
    card_loc_t loc_played(player_t pl, int trick)
    {
        card_loc_t rv;
        rv.where = played;
        rv.pl = pl;
        rv.suit = -1;
        rv.index = trick;
        return rv;
    }

    card_loc_t loc_inhand(player_t pl, suit_t s, int index_in_suit)
    {
        card_loc_t rv;
        rv.where = inhand;
        rv.pl = pl;
        rv.suit = s;
        rv.index = index_in_suit;
        return rv;
    }

    card_loc_t loc_ontable(player_t pl)
    {
        card_loc_t rv;
        rv.where = ontable;
        rv.pl = pl;
        rv.suit = -1;
        rv.index = -1;
        return rv;
    }

    void play_card(card_changes_t* p, player_t pl, card_t c, int index)
    {
        card_change_t change;
        change.card = c;
        change.move = true;
        change.from = loc_inhand(pl, suit(c), index);
        change.to = loc_ontable(pl);
        change.annotation_action = remove_annotation;
        change.annotation = annotation_unplayable();
        add_change(p, change);
    }

    void unplay_card(card_changes_t* p, player_t pl, int index, card_t c, position_analysis_t* analysis)
    {
        card_change_t change;
        change.card = c;
        change.move = true;
        change.from = loc_ontable(pl);
        change.to = loc_inhand(pl, suit(c), index);
        change.annotation_action = add;
        change.annotation = annotation(c, analysis);
        add_change(p, change);
    }

    void move_card_along(card_changes_t* p, card_t c, player_t pl, int from, int to)
    {
        card_change_t change;
        change.card = c;
        change.move = true;
        change.from = loc_inhand(pl, suit(c), from);
        change.to = loc_inhand(pl, suit(c), to);
        change.annotation_action = none;
        add_change(p, change);
    }
    
    void quit_trick(card_changes_t* p, card_t c, player_t pl, int trick)
    {
        card_change_t change;
        change.card = c;
        change.move = true;
        change.from = loc_ontable(pl);
        change.to = loc_played(pl, trick);
        change.annotation_action = none;
        add_change(p, change);
    }
}

// Initialise view (and position analysis) based on deal and play so far
void initialise_view(controller_info_t* info, bool no_play_yet)
{
	// Figure out what got dealt
	for (int c = 0; c < 52; ++c) {
		info->cardstate[c].state = (info->deal.holder[c] == plNone) ? notdealt : unplayed;
        info->cardstate[c].location.pl = info->deal.holder[c];
	}
    
    // Zap the play so far
    if (no_play_yet) {
        for (int i = 0; i < 52; ++i) {
            info->play.played[i] = -1;
            info->playcontext[i] = -1;
        }
        info->playcontext[0] = info->nextcontext++;
    }
	
	// No cards played yet
	for (int pl = 0; pl < 4; ++pl) {
		info->view.pl_view[pl].inplay = no_card();
		for (int i = 0; i < 13; ++i)
			info->view.pl_view[pl].played[i] = no_card();
	}

	// Follow along with the play, logging played cards in the appropriate place
    info->view.summary[pNS].so_far = 0;
    info->view.summary[pEW].so_far = 0;
	int startoftrick = info->play.nCardsPlayed & ~3;
	trickstate_t trickstate;
	player_t pl = nextpl(info->deal.declarer);
	for (int i = 0; i < info->play.nCardsPlayed; ++i) {
		card_t c = info->play.played[i];
		if (i % 4 == 0) trickstate = trickstate_t(pl, c); else trickstate.play(pl, c, info->deal.trumps);
        const int trick = i/4;
        if (i >= startoftrick) {
            info->cardstate[c].state = playedthistrick;
            info->view.pl_view[pl].inplay = nonplayable_card(c);
            info->cardstate[c].location = loc_ontable(pl);
        } else {
            info->cardstate[c].state = playedprevtrick;
            info->view.pl_view[pl].played[trick] = nonplayable_card(c);
            info->cardstate[c].location = loc_played(pl, trick);
        }
		pl = nextpl(pl);
		if (i % 4 == 3) {
			pl = trickstate.winner;
			info->view.summary[partnership(trickstate.winner)].so_far++;
		}
	}
    info->nextpl = pl;
	
	// Figure out which plays are legal
	bool anythinggoes = true;
	if (info->play.nCardsPlayed % 4 > 0) {
		for (int r = 0; r < 13; ++r) {
			card_t c = card(trickstate.ledsuit, r);
			if ((info->cardstate[c].state == unplayed) && (info->deal.holder[c] == pl)) {
				info->cardstate[c].state = playable;
				anythinggoes = false;
			}
		}
	}
	if (anythinggoes) {
		for (int c = 0; c < 52; ++c) {
			if ((info->cardstate[c].state == unplayed) && (info->deal.holder[c] == pl)) {
				info->cardstate[c].state = playable;
			}
		}
	}
    
    // Now initialise the position analyses
	if (no_play_yet) {
	    for (int i = 0; i < 52; ++i)
		    init_position_analysis(&info->analysis[i], 13 - i/4);
	}
        
    // And finally the view of the cards in hands
    for (int isuit = 0; isuit < 4; isuit++) {
        int ncards[4];
        for (int ipl = 0; ipl < 4; ++ipl) {
            ncards[ipl] = 0;
            for (int icard = 0; icard < 13; ++icard) {
                info->view.pl_view[ipl].hand[isuit][icard] = no_card();
            }
        }
        for (int irank = 12; irank >= 0; irank--) {
            card_t c = card(suit_t(isuit), rank_t(irank));
            player_t pl = info->deal.holder[c];
            if (pl != plNone) {
                if (info->cardstate[c].state == unplayed) {
                    info->view.pl_view[pl].hand[isuit][ncards[pl]] = nonplayable_card(c);
					info->cardstate[c].location = loc_inhand(pl, suit_t(isuit), ncards[pl]++);
                } else if (info->cardstate[c].state == playable) {
                    info->view.pl_view[pl].hand[isuit][ncards[pl]] = playable_card(c, &info->analysis[info->play.nCardsPlayed]);
					info->cardstate[c].location = loc_inhand(pl, suit_t(isuit), ncards[pl]++);
                }
            }
        }
    }
    
    // And now the summaries
    const int tricksleft = 13 - info->play.nCardsPlayed/4;
    for (int p = 0; p < 2; ++p)
    {
        info->view.summary[p].to_go.low = 0;
        info->view.summary[p].to_go.high = 1+tricksleft;
    }
}

// Floor an int at zero
int floor0(int x);
int floor0(int x) 
{
    if (x < 0) return 0;
    else return x;
}

// Provides updates based on new analysis 
void update_for_analysis_ex(controller_info_t* info, card_changes_t* changes);
void update_for_analysis_ex(controller_info_t* info, card_changes_t* changes)
{
    for (int c = 0; c < 52; ++c) {
        if (info->cardstate[c].state == playable) {
            card_change_t change;
            change.card = c;
            change.move = false;
            change.from = info->cardstate[c].location;
            change.to = info->cardstate[c].location;
            change.annotation_action = update;
            change.annotation = annotation(c, &info->analysis[info->play.nCardsPlayed]);
            add_change(changes, change);
        }
    }
    int tricksleft = 13 - info->play.nCardsPlayed/4;
    bound_t bound = info->analysis[info->play.nCardsPlayed].global;
    info->view.summary[partnership(info->nextpl)].to_go = bound;
    info->view.summary[1-partnership(info->nextpl)].to_go.high = tricksleft+1 - bound.low;
    info->view.summary[1-partnership(info->nextpl)].to_go.low = tricksleft+1 - bound.high;
}

// Provides updates based on new analysis 
void update_for_analysis(controller_info_t* info, card_changes_t* changes)
{
    changes->num_changes = 0;    
    update_for_analysis_ex(info, changes);
}

// Updates data structures for the play just made; returns 0 if the action is OK
int update_for_play(controller_info_t* info, card_changes_t* changes, card_t c)
{
    changes->num_changes = 0;
    if (info->cardstate[c].state != playable) return 1;

    // Remove existing annotations
    player_t pl = info->deal.holder[c];
    for (int i = 0; i < 52; ++i) {
        if (info->cardstate[i].state == playable) {
            card_change_t change;
            change.card = i;
            change.move = false;
            change.from = info->cardstate[i].location;
            change.to = info->cardstate[i].location;
            change.annotation_action = remove_annotation;
			change.annotation = annotation_unplayable();
			add_change(changes, change);
        }
    }

    // Move the card
    play_card(changes, pl, c, info->cardstate[c].location.index);

	// And shuffle other cards along to close the gap
    for (int i = info->cardstate[c].location.index+1; i < 13; ++i) 
	{
		if (info->view.pl_view[pl].hand[suit(c)][i].has_card)
			move_card_along(changes, info->view.pl_view[pl].hand[suit(c)][i].card, pl, i, i-1);
	}
    
    // Update data structures for the play
    info->play.played[info->play.nCardsPlayed++] = c;
    info->playcontext[info->play.nCardsPlayed] = info->nextcontext++;
    initialise_view(info, false);
	if (info->play.nCardsPlayed < 52)
		init_position_analysis(&info->analysis[info->play.nCardsPlayed], 13 - info->play.nCardsPlayed/4);
    
    // If it's the end of the trick, quit it
    const int n = info->play.nCardsPlayed;
    if (n % 4 == 0) {
        changes->pause_after = changes->num_changes - 1;
        for (int i = n-4; i < n; ++i)
            quit_trick(changes, info->play.played[i], info->deal.holder[info->play.played[i]], n/4-1);
    }
    
    // Get next bound from the bound for this play
    bound_t play_bound = info->analysis[info->play.nCardsPlayed-1].play[c];
    if (partnership(pl) == partnership(info->nextpl)) {
        info->analysis[info->play.nCardsPlayed].global.low = floor0(play_bound.low-1);
        info->analysis[info->play.nCardsPlayed].global.high = floor0(play_bound.high-1);
    } else {
        int tricksleft = 13 - (info->play.nCardsPlayed-1)/4;
        info->analysis[info->play.nCardsPlayed].global.high = tricksleft+1 - play_bound.low;
        info->analysis[info->play.nCardsPlayed].global.low = tricksleft+1 - play_bound.high;
    }
    for (int i = 0; i < 52; ++i) {
        info->analysis[info->play.nCardsPlayed].play[i].high = info->analysis[info->play.nCardsPlayed].global.high;
        info->analysis[info->play.nCardsPlayed].play[i].low = 0;
    }
        
	// Add annotations
	update_for_analysis_ex(info, changes);

    return 0;
}

// Performs an undo. Returns 1 if undo is impossible (presumably because we're already 
// at the first trick). 
int update_for_undo(controller_info_t* info, card_changes_t* changes)
{
    // TODO
    changes->num_changes = 0;
    return 0;
}

// Performs a redo. Returns 1 if not possible (perhaps because we've got to the end of the re-do stack). 
int update_for_redo(controller_info_t* info, card_changes_t* changes)
{
    // TODO
    changes->num_changes = 0;    
    return 0;
}

// Makes the default play 
// Returns 1 if not possible (presumably because there are no cards left) 
int update_for_forward(controller_info_t* info, card_changes_t* changes)
{
    // TODO
    changes->num_changes = 0;
    return 0;
}

