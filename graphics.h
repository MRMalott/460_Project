/*
Handler for curse functions

*/

#include <unistd.h>
#include <curses.h>

using namespace std;

#define WIDTH 80
#define HEIGHT 25

// Define colors
init_pair(10, COLOR_BLACK, COLOR_WHITE); // Normal Background
init_pair(0, COLOR_RED, COLOR_WHITE); // 
init_pair(1, COLOR_BLUE, COLOR_WHITE); //
init_pair(2, COLOR_YELLOW, COLOR_WHITE); //
init_pair(11, COLOR_BLACK, COLOR_CYAN); // Selected Background
init_pair(3, COLOR_RED, COLOR_CYAN); //
init_pair(4, COLOR_BLUE, COLOR_CYAN); // 
init_pair(5, COLOR_YELLOW, COLOR_CYAN); //

// Thread function to Explosion



// Display card
// send in window and top left corner x, y
void draw_card(WINDOW* window, int x, int y, Card card, bool select) {
	// Determin color of shapes and background
	if (select) { 
		attron(COLOR_PAIR(3 + card.color));
	}
	else {
		attron(COLOR_PAIR(card.color));
	}

	// Draw Background
	for (int row = 0; row < 5; row++); {
		mvwhline(window, y + row, x, ' ', 13); 
	}

	// Draw Shapes
	int line_mover = 0; // Says how much farther right to put the shape

	// Draw one shape
	char shades[3] = {' ', '@', '#' };
	if (card.number == 0) {
		mvwaddch(window, y + 1, x + 6, shades[card.shading]);
		mvwhline(window, y + 2, x + 5, shades[card.shading], 3);
		mvwaddch(window, y + 3, x + 6, shades[card.shading]);
	}

	else if (card.number == 1) {
		for (int i = 0; i < 2; i++) { 
	        mvwaddch(window, y + 1, x + line_move + 3, shades[card.shading]);
    	    mvwhline(window, y + 2, x + line_mover + 2, shades[card.shading], 3);
        	mvwaddch(window, y + 3, x + line_mover + 3, shades[card.shading]);
			line_move += 6;
		}
	}

	else {
	        for (int i = 0; i < 3; i++) {
        	    mvwaddch(window, y + 1, x + line_move + 2, shades[card.shading]);
	            mvwhline(window, y + 2, x + line_mover + 1, shades[card.shading], 3);
        	    mvwaddch(window, y + 3, x + line_mover + 2, shades[card.shading]);
	            line_move += 4;
		}
	}

	attroff(card.color);
}

 
// Select


// Deselect


// Score update


// Countdown



