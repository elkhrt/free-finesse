// This file is part of FreeFinesse, a double-dummy analyzer (c) Edward Lockhart, 2010
// It is made available under the GPL; see the file COPYING for details

#include "types.h"
#include <iostream>
#include <string>
#include <vector>
#include <map>

// Different analysis modes; each is in their own source file
void leads(const struct deal_t& deal);
void par(struct deal_t deal);
void test_main();
void interactive(const struct deal_t& deal);

// Convert a string to a hand
std::vector<card_t> hand(const std::string& str) 
{
	int suit = 3;
	std::vector<card_t> rv;
	for (std::string::const_iterator it = str.begin(); it != str.end(); it++) {
		if (*it == '.') {
			suit--;
		} else {
			size_t p = std::string("23456789TJQKA").find(toupper(*it));
			rv.push_back(card_t(p * 4 + suit));
		}
	}
	return rv;
}

// Print a usage message and terminate
void usage()
{
	std::cout << "FreeFinesse by Edward Lockhart" << std::endl;
	std::cout << "Command line options:" << std::endl;
	std::cout << "\tSpecify hands via -n, -e, -s, -w" << std::endl;
	std::cout << "\tSpecify declarer via -d" << std::endl;
	std::cout << "\tSpecify trumpsuit via -t" << std::endl;
	std::cout << "\tSpecify board number via -b" << std::endl;
	std::cout << "\tSpecify a par analysis via -p" << std::endl;
	std::cout << "\tSpecify an opening-lead analysis via -l" << std::endl;
	std::cout << "\tRun tests with -T (other inputs ignored)" << std::endl;
	std::cout << "Any missing information will be requested" << std::endl;
	exit(0);
}

// Parse the command line
typedef std::map<char, std::string> opt_t;
opt_t parse_options(int argc, char* argv[])
{
	opt_t rv;
	for (int i = 1; i < argc; ++i) {
		if ((argv[i][0] != '-') || (strlen(argv[i]) < 2)) usage();
		rv[argv[i][1]] = std::string(argv[i]+2);
	}
	return rv;
}

// Get a string, either from the command-line or from the user
std::string get_option_prompt(char c, const std::string& prompt, const opt_t& opt)
{
	opt_t::const_iterator it = opt.find(c);
	if (it != opt.end()) return it->second;
	std::cout << prompt << ": ";
	std::string str;
	std::cin >> str;
	return str;
}

// Get a string, either from the command-line or with a default
std::string get_option_dflt(char c, const std::string& dflt, const opt_t& opt)
{
	opt_t::const_iterator it = opt.find(c);
	if (it != opt.end()) return it->second;
	return dflt;
}

// Char -> player / suit
inline player_t player(char c) { return player_t(std::string("NESW").find(toupper(c))); }
inline suit_t suit(char c) { return suit_t(std::string("CDHSN").find(toupper(c))); }

// Entry point
int main(int argc, char* argv[])
{
	// Parse options
	opt_t opt = parse_options(argc, argv);
    
    // Test mode
    if (opt.find('T') != opt.end()) {
        test_main();
        return 0;
	}
    
    // Assemble deal
	deal_t d;
	std::vector<card_t> cards[4];
	cards[plS] = hand(get_option_prompt('s', "South", opt));
	cards[plN] = hand(get_option_prompt('n', "North", opt));
	cards[plE] = hand(get_option_prompt('e', "East", opt));
	cards[plW] = hand(get_option_prompt('w', "West", opt));
	for (int pl = 0; pl < 4; ++pl) {
		if (cards[pl].size() != 13) usage();
		for (int i = 0; i < 13; ++i) {
			d.holder[cards[pl][i]] = player_t(pl);
		}
	}

	// Figure out what the user wants us to do
	if (opt.find('p') != opt.end()) {

		// Par analysis
		d.board = atoi(get_option_prompt('b', "Board Number", opt).c_str());
		par(d);

	} else if (opt.find('l') != opt.end()) {

		// Opening lead analysis
		d.trumps = suit(get_option_prompt('t', "Trumps", opt)[0]);
		d.declarer = player(get_option_prompt('d', "Declarer", opt)[0]);
		leads(d);

	} else {

		// Interactive double-dummy
		d.trumps = suit(get_option_prompt('t', "Trumps", opt)[0]);
		d.declarer = player(get_option_prompt('d', "Declarer", opt)[0]);
		interactive(d);

	}
}

