// This file is part of FreeFinesse, a double-dummy analyzer (c) Edward Lockhart, 2010
// It is made available under the GPL; see the file COPYING for details

#include "types.h"
#include "analyzer.h"

#pragma once

// Annotations on a card 
typedef struct card_annotation_t { 
    bool has_label; 
    bound_t label;                  // What to display on the label
    bool playable;                  // Whether the card is available to be played 
    bool best;                      // Whether the card is (one of) the best available options 
} card_annotation_t;

typedef struct annotated_card_t { 
    bool has_card;
    card_t card; 
    card_annotation_t annotation; 
} annotated_card_t;

// A location in which a card can be displayed 
typedef enum card_loc_type_t { played, ontable, inhand } card_loc_type_t; 
typedef struct card_loc_t { 
    player_t pl; 
    card_loc_type_t where; 
    int suit;                       // For cards in hand, which suit it's in
    int index;                      // For cards in hand, index within the suit 
                                    // For cards that have been played, which trick they were played in
} card_loc_t;

// A change to the location or manner in which a card is displayed 
typedef enum action_t { none, add, remove_annotation, update } action_t; 
typedef struct card_change_t { 
    card_t card; 
    int move;                               // Whether to move the card 
    card_loc_t from; 
    card_loc_t to; 
    action_t annotation_action; 
    card_annotation_t annotation; 
} card_change_t;

// A set of card changes 
typedef struct card_changes_t { 
    int num_changes; 
    int pause_after;                        // an pause in the animation, e.g. before quitting a trick 
    card_change_t changes[52]; 
} card_changes_t;

// Summary of the view for one player 
typedef struct player_view_t { 
    annotated_card_t played[13]; 
    annotated_card_t hand[4][13]; 
    annotated_card_t inplay; 
} player_view_t;

// Trick summary for a partnership 
typedef struct trick_view_t { 
    int so_far; 
    bound_t to_go; 
} trick_view_t;

// Summary of the whole view 
typedef struct view_t { 
    player_view_t pl_view[4]; 
    trick_view_t summary[2];                                // Per partnership 
} view_t;

// Per-card information
typedef struct card_info_t
{
    cardstate_t state;
    card_loc_t location;
} card_info_t;

// Information used by the controller 
typedef struct controller_info_t { 
    void* context;
    card_info_t cardstate[52];                     // This is for the controller rather than for the view
    int playcontext[52];                           // A context identifier for each play; used so that a long-running
                                                   // analysis can figure out if its results are still useful
    int nextcontext;                               // The next unused context; to ensure uniqueness
    player_t nextpl;
    view_t view; 
    deal_t deal; 
    play_t play; 
    int redo_upto;                                  // How far forward we can do a re-do 
    position_analysis_t analysis[52];               // Per play, to allow for better undo 
} controller_info_t;

#ifdef __cplusplus 
extern "C" { 
#endif
    
    // Initialise view (and position analysis) based on deal and play so far 
    void initialise_view(controller_info_t* info, bool zap_position_analysis);
    
    // Provides updates based on new analysis 
    void update_for_analysis(controller_info_t* info, card_changes_t* changes);
    
    // Updates data structures for the play just made; returns 0 if the action is OK 
    // 1 if it is not (e.g. an invalid card, or backing up from the first trick) 
    int update_for_play(controller_info_t* info, card_changes_t* changes, card_t play);
    
    // Performs an undo. Returns 1 if undo is impossible (presumably because we're already 
    // at the first trick). 
    int update_for_undo(controller_info_t* info, card_changes_t* changes);
    
    // Performs a redo. Returns 1 if not possible (perhaps because we've got to the end of the re-do stack). 
    int update_for_redo(controller_info_t* info, card_changes_t* changes);
    
    // Makes the default play 
    // Returns 1 if not possible (presumably because there are no cards left) 
    int update_for_forward(controller_info_t* info, card_changes_t* changes);
    
#ifdef __cplusplus 
} 
#endif 
