/*
Purpose: Provide card and deck operations and constructs

shuffle, 
*/

#include <iostream>
#include <string>
#include <cstring>
#include <vector>
#include <cstdlib>
#include <cerrno>
#include <unistd.h>
#include <sys/types.h>
#include <algorithm>
#include <time.h>

using namespace std;


//  --	STRUCTURES  --


enum Color { RED, BLUE, YELLOW, NONE1 };
enum Shape { CROSS, CIRCLE, SLASH, NONE2 };
enum Number { ONE, TWO, THREE, NONE3 };
enum Shading { LOW, MEDIUM, HIGH, NONE4 };

struct Card {
	Color color;
	Shape shape;
	Number number;
	Shading shading;

	// Needs a parameterless constructor
	Card() {}

	// Constructor to save lines for initializing Deck
	Card(Color p1, Shape p2, Number  p3, Shading p4) :
		color(p1),
		shape(p2),
		number(p3),
		shading(p4)
	{ }
};

typedef Card BOARD[12];

typedef Card DECK[81]; // should this be an array or list?



//  --  FUNCTIONS  --

//	MAKE A DECK
void init_deck(Card deck[]) {
	int pos = 0; // Counter for position in deck
	
	Color color_arr[3] = {RED, BLUE, YELLOW};
	Shape shape_arr[3] = {CROSS, CIRCLE, SLASH};
	Number num_arr[3] = {ONE, TWO, THREE};
	Shading shade_arr[3] = {LOW, MEDIUM, HIGH};

	for (int c = 0; c < 3; c++) {
		for (int sp = 0; sp < 3; sp++) {
			for (int n = 0; n < 3; n++) {
				for (int sh = 0; sh < 3; sh++) {
					deck[pos++] = Card(color_arr[c], shape_arr[sp], num_arr[n], shade_arr[sh]);
				}
			}
		}
	}
}

//	SHUFFLE CARDS
void shuffle(Card deck[], int start, int end) {
	// Shuffle only the cards between start (usually 0), and end
	srand(time(NULL));
	random_shuffle(&deck[start], &deck[end]);
}

//	ORDER SCORES/PLAYERS
// use for final scores


//	CHECK FOR SET 

// Function prototypes
bool color_is_set(Color c1, Color c2, Color c3);
bool shape_is_set(Shape s1, Shape s2, Shape s3);
bool number_is_set(Number n1, Number n2, Number n3);
bool shade_is_set(Shading s1, Shading s2, Shading s3);
bool is_set(struct Card c1, struct Card c2, struct Card c3);
bool is_no_set(struct Card board[]);	// Card array passed in should be current state of board

// Function definitions
bool color_is_set(Color c1, Color c2, Color c3) {
	return (((c1 == c2) and (c1 == c3)) or (((c1 != c2) and (c2 != c3)) and (c1 != c3)));
}

bool shape_is_set(Shape s1, Shape s2, Shape s3) {
	return (((s1 == s2) and (s1 == s3)) or (((s1 != s2) and (s2 != s3)) and (s1 != s3)));
}

bool number_is_set(Number n1, Number n2, Number n3) {
	return (((n1 == n2) and (n1 == n3)) or (((n1 != n2) and (n2 != n3)) and (n1 != n3)));
}

bool shade_is_set(Shading s1, Shading s2, Shading s3) {
	return (((s1 == s2) and (s1 == s3)) or (((s1 != s2) and (s2 != s3)) and (s1 != s3)));
}

bool is_set(Card c1, Card c2, Card c3) {
	return (color_is_set(c1.color, c2.color, c3.color) and
		shape_is_set(c1.shape, c2.shape, c3.shape) and
		number_is_set(c1.number, c2.number, c3.number) and
		shade_is_set(c1.shading, c2.shading, c3.shading));
}

bool is_no_set(Card board[], bool print_set = false) {
	// not sure how many results there will be, so just made it large
	// ran a test in python, loop should run 1000 times
	int board_size = sizeof board;
	for (int i = 0; i < board_size - 2; i++) {
		for (int j = 1; j < board_size - 2; j++) {
			for (int k = 2; k < board_size - 2; k++) {
				// if 3 cards on the board can make a set, break from function
				if(is_set(board[i], board[i + j], board[i + k])) {
					//if (print_set)
					cout << "Set: " << (char)(i+65) << (char)(i+j+65) << (char)(i+k+65) << endl;
					return false;
				}
			}
		}
	}
	//if (print_set)
		cout << "NO SET" << endl;
	// If we get this far, a true value was not found on board
	return true;
}
