// This file is part of FreeFinesse, a double-dummy analyzer (c) Edward Lockhart, 2010
// It is made available under the GPL; see the file COPYING for details

//
//  Interactive analysis at the command-line; this is for demonstration purposes really
//

#include "analyzer.h"
#include "controller.h"
#include <iostream>
#include <sstream>
#include <iomanip>

namespace {

    inline char suittext(suit_t suit) { return "CDHSN"[suit]; }
	inline char suittext(card_t c) { return suittext(suit(c)); }
	inline char ranktext(card_t c) { return "23456789TJQKA"[rank(c)]; }
	inline char playertext(player_t pl) { return "NESW"[pl]; }
    inline std::string smalldigit(int d) {
        switch (d) {
            case 0: return "₀";
            case 1: return "₁";
            case 2: return "₂";
            case 3: return "₃";
            case 4: return "₄";
            case 5: return "₅";
            case 6: return "₆";
            case 7: return "₇";
            case 8: return "₈";
            case 9: return "₉";      
            default: return "?";
        }
    }
    
    struct gui_t : public view_t
    {
        std::string buff[14][120];				// each std::string is a single character; we do this so that we can use unicode characters

        gui_t() 
        {
            for (int x = 0; x < 120; ++x)
                for (int y = 0; y < 14; ++y)
                    buff[y][x] = " ";
        }
        
        annotated_card_t& card(const card_loc_t& loc)
        {
            if (loc.where == inhand) return pl_view[loc.pl].hand[loc.suit][loc.index];
            if (loc.where == ontable) return pl_view[loc.pl].inplay;
            if (loc.where == played) return pl_view[loc.pl].played[loc.index];
            throw "invalid card location";
        }
        
        std::string* location(const card_loc_t& loc)
        {
            int plx[4] = {55, 90, 55, 20};
            int ply[4] = {0, 5, 10, 5};
            int pltx[4] = {65, 70, 65, 60};
            int plty[4] = {6, 7, 8, 7};
            int x, y;
            switch (loc.where) {
                case inhand:
                    x = plx[loc.pl] + loc.index*3;
                    y = ply[loc.pl] + (3-loc.suit);
                    break;
                case ontable:
                    x = pltx[loc.pl];
                    y = plty[loc.pl];
                    break;
                case played:
                    x = loc.pl*3;
                    y = 1+loc.index;
                    break;
                default:
                    throw "invalid card location";
            }
            return buff[y] + x;
        }
        
        void draw_card_inhand(std::string* p, annotated_card_t c)
        {
            if (c.has_card) {
                *p++ = ranktext(c.card);
            } else {
                *p++ = " ";
            }
        }
        
        void draw_card_ontable(std::string* p, annotated_card_t c)
        {
            if (c.has_card) {
                *p++ = suittext(c.card);
                *p++ = ranktext(c.card);
            } else {
                *p++ = " ";
                *p++ = " ";
            }
        }
        
        void draw_card_played(std::string* p, annotated_card_t c)
        {
            if (c.has_card) {
                *p++ = suittext(c.card);
                *p++ = ranktext(c.card);
            } else {
                *p++ = " ";
                *p++ = " ";
            }
        }
        
        void draw_void(std::string* p)
        {
            *p++ = "-";
            *p++ = "-";
            *p++ = "-";
        }

        void draw_card(const card_loc_t& loc)
        {
            annotated_card_t c = card(loc);
            std::string* p = location(loc);
            if (loc.where == inhand) {
                if (loc.index == 0 && !c.has_card) draw_void(p);
                else if (loc.index < 7) draw_card_inhand(p, c);
            } else if (loc.where == played) {
                draw_card_played(p, c);
            } else if (loc.where == ontable) {
                draw_card_ontable(p, c);  
            } else throw "Invalid location";
        }   

        void draw_annotation(const card_loc_t& loc)
        {
            annotated_card_t c = card(loc);
            std::string* p = location(loc) + 1;
            if (loc.where == inhand && loc.index < 7) {
                if (c.annotation.has_label) {
                    if (c.annotation.label.low+1 < c.annotation.label.high) {
                        *p++ = "*";                                            
                        *p++ = " ";                                            
                    } else {
#ifdef _WIN32
						*p++ = ".";
						*p++ = ".";
#else
                        if (c.annotation.label.low > 9) {
                            *p++ = smalldigit(1);
                            *p++ = smalldigit(c.annotation.label.low-10);                    
                        } else {
                            *p++ = smalldigit(c.annotation.label.low);
                            *p++ = " ";                    
                        }
#endif
                    }
				} else {
					*p++ = " ";
					*p++ = " ";
				}
            }
        }  

		void draw_annotated_card(const card_loc_t& loc)
		{
			draw_annotation(loc);
			draw_card(loc);
		}

        void write(int x, int y, const std::string& str)
        {
            for (std::string::const_iterator it = str.begin(); it != str.end(); ++it)
            {
                buff[y][x++] = *it;
            }   
        }
        
        std::string itoa(int n) 
        {
            std::ostringstream out;
            out << std::setw(2) << n;
            return out.str();
        }
        
        std::string itoa(bound_t bound, int n) 
        {
            std::ostringstream out;
            if (bound.low + 1 == bound.high)
                out << " " << std::setw(2) << bound.low+n << "  ";
            else
                out << std::setw(2) << bound.low+n << "-" << std::setw(2) << bound.high-1+n;
            return out.str();
        }

        void draw_summary()
        {
            int x = 90, y = 0;
            buff[y++][x] = "        NS    EW  ";
            buff[y++][x] = "So Far  " + itoa(summary[pNS].so_far) + "    " + itoa(summary[pEW].so_far) + "  ";
            buff[y++][x] = "To Go  " + itoa(summary[pNS].to_go, 0) + " " + itoa(summary[pEW].to_go, 0);
            buff[y++][x] = "Total  " + itoa(summary[pNS].to_go, summary[pNS].so_far) + " " + itoa(summary[pEW].to_go, summary[pEW].so_far);
        }

        void draw_all()
        {
            card_loc_t loc;
            for (int pl = 0; pl < 4; ++pl)
            {
                loc.pl = player_t(pl);
                loc.where = ontable;
                draw_card(loc);
                for (int i = 0; i < 13; ++i)
                {
                    loc.index = i;
                    loc.where = played;
                    draw_card(loc);
                    loc.where = inhand;
                    for (int s = 0; s < 4; ++s) {
                        loc.suit = -(s);
                        draw_annotated_card(loc);
                    }
                }
            }
        }
        
        void initialise(const view_t& view)
        {
            summary[pNS] = view.summary[pNS];
            summary[pEW] = view.summary[pEW];
            draw_summary();
            for (int pl = 0; pl < 4; ++pl)
            {
                const player_view_t& src = view.pl_view[pl];
                player_view_t& dst = pl_view[pl];
                dst.inplay = src.inplay;
                for (int i = 0; i < 13; ++i)
                {
                    dst.played[i] = src.played[i];
                    for (int s = 0; s < 4; ++s)
                    {
                        dst.hand[s][i] = src.hand[s][i];
                    }
                }
            }
            draw_all();
        }
        
        void display() const
        {
            std::cout << std::endl;
            for (int y = 0; y < 14; ++y)
            {
                for (int x = 0; x < 120; ++x)
                {
                    std::cout << buff[y][x];
                }
                std::cout << std::endl;
            }
        }
    };    
    
    void process_changes(gui_t* gui, card_changes_t* changes)
    {
        for (int i = 0; i < changes->num_changes; ++i) {
            const card_change_t& ch = changes->changes[i];
            if (ch.move) {
                gui->card(ch.to) = gui->card(ch.from);
                gui->card(ch.from).has_card = false;
				gui->card(ch.from).annotation.has_label = false;
				gui->draw_annotated_card(ch.from);
                gui->draw_annotated_card(ch.to);
            }
            if (ch.annotation_action != none) {
                if (ch.annotation_action == remove_annotation) {
                    gui->card(ch.to).annotation.has_label = false;
					gui->draw_annotation(ch.to);
                } else {
                    gui->card(ch.to).annotation = ch.annotation;
                    gui->draw_annotation(ch.to);
                }
            }
            if (i == changes->pause_after) 
				gui->display();
        }
    }
    
	inline card_t card(const std::string& str) {
		suit_t s = suit_t(std::string("CDHSN").find(toupper(str[0])));
		rank_t r = rank_t(std::string("23456789TJQKA").find(toupper(str[1])));
		return card(s, r);
	}

	std::ostream& operator<<(std::ostream& out, bound_t& bound) {
		if (bound.high == bound.low+1) {
			out << bound.low;
		} else {
			out << bound.low << "-" << (bound.high-1);
		}
		return out;
	}

	int callback(position_analysis_t* pos)
	{
        controller_info_t* info = (controller_info_t*)pos->context;
        gui_t* gui = (gui_t*)info->context;
        card_changes_t changes;
        changes.num_changes = 0;
        changes.pause_after = -1;    
        update_for_analysis(info, &changes);
        gui->summary[pNS] = info->view.summary[pNS];
        gui->summary[pEW] = info->view.summary[pEW];
        gui->draw_summary();
        process_changes(gui, &changes);
        gui->display();
		return true;
	}

}

// Interactive mode
void interactive(const deal_t& d) 
{
	// Set up data structures
	cache_t* cache = new_cache();	
    controller_info_t info;
    gui_t gui;
    info.context = &gui;
    info.deal = d;
    info.play.nCardsPlayed = 0;
    initialise_view(&info, true);
    
    // Initialise GUI
    gui.initialise(info.view);
    std::string str;
    
	// Main loop
    card_changes_t changes;
	while (true) {
        info.analysis[info.play.nCardsPlayed].context = &info;
		analyze(&d, &info.play, cache, callback, &info.analysis[info.play.nCardsPlayed], true);
	    gui.display();
        changes.num_changes = 0;
        changes.pause_after = -1;    
    ASK_AGAIN:        
        std::cout << "Play: ";
        std::cin >> str;
        card_t c = card(str);        
        const int rv = update_for_play(&info, &changes, c);           
        if (rv != 0) goto ASK_AGAIN;
        process_changes(&gui, &changes);
	}
}
