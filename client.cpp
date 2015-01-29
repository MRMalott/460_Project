#include <iostream>
#include <cstdlib>
#include <cerrno>
#include <string>
#include <cstring>
#include <unistd.h>
#include <sys/utsname.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h> // asdlfkj
#include <fcntl.h> // asdfasdf
#include <curses.h>
#include <pthread.h>
#include <poll.h>
#include "deck.h" // Look in directory first
#include "network.h"

#define SERVER_IP "127.0.0.1"
#define WIDTH 80
#define HEIGHT 25

using namespace std;

// Declare connection talking
constexpr int READ_SIZE = 1024;
constexpr int WRITE_SIZE = 1024;
char input[WRITE_SIZE];
int client; // file descriptor to port
char player_name[NAME_LEN + 1]; // Current player name with null terminator
int port; // port operating from
char cards_selected[3]; // Array of selected cards
BOARD board; // Array of cards on the table
WINDOW* c_win; // Card - board window
WINDOW* s_win; // Score window
PLAYER players; // Player data
int num_players = 0;
int k; // Keyboard input

//	Declare Functions
void cleanup(int sig);
void fail(string message);
void rejected();
int read_bytes(int pos);
int receive_data();
void send_data();
void init_colors(); // Initialize curses colors
void clear_card_space(int x, int y); // black out a card
void disp_card(int x, int y, char c, int slot, bool select); // Disp a card
void disp_score(int x, int y, char name[], int score); // disp a player and score
void card_pos(int pos, int& x, int& y); // Find position of card on screen
void score_pos(int pos, int& x, int& y); // Find position of player on screen
void disp_options(); // Display keys to press (6, x, a-L)
void draw_board(); // Draw the cards to the screen
void init_screen(); // Show cards and names
void explosion(int card_slots[], int num); // exploding feature
void parse_input(); // Look at recieved input and do appropriate action
void timer_wait(); // Wait on timer
void init_board(); // Grab cards from server
void init_players(); // Displayes all players on player/score window
void accept_players(); // Grab players from server
void accept_username(char username[]); // Make sure username is under 15 chars
void init_username(char* username); // Assure no name collisions
void init_client(); // Initialize client server talk
void disp_last_input(char c); // Display current selected output

int main(int argc, char* argv[]) {
	// Check that the right ammount of args were sent
	if (argc < 2 or argc > 3) {
		cerr << "You didn't pass the right ammount of arguements";
		exit(EXIT_FAILURE);
	}

	// 	PLAYER DATA SETUP
	// Grab port from parameters
	port = atoi(argv[1]); 

    //  Grab name
	char username[NAME_LEN + 1]; // Name length plus null terminating byte
	clear_buffer(username); 
    
	// If user did not pass in name grab their login name
    if (argc != 3) {
		//username = getenv("LOGNAME");
		strncpy(username, getenv("LOGNAME"), strlen(getenv("LOGNAME")));
		//cout << "Original username is size " << strlen(getenv("LOGNAME")) << "." << endl;
    }
    // Otherwise use the name given
    else {
        //username = argv[2];
		strncpy(username, argv[2], strlen(argv[2]));
		//cout << "Original username is size " << strlen(argv[2]) << "." << endl;
    }
	//cout << "Original username is " << username << endl;

	// Make sure username is 14 chars or under
	accept_username(username);
	//cout << "Username after lessening is " << username << "." << endl;

	//	CONNECTION SETUP
	init_client();	

	// Finalize username
	init_username(username);
	cout << "Final username  is " << player_name << endl;
	
	//	COUNTDOWN TIMER
	// Send ready signal to client
	// Read global time variable and begin itimer
	// when it equals 0 the game has started. Start the game...
	timer_wait();
	//cout << "done with timer wait and initializatoins" << endl;
	
    //  INITIALIZE CURSES
    initscr();
    cbreak();
    noecho();
    if (!has_colors()) {
        endwin();
        fail("Your terminal does nots upport color");
    }

    if (start_color() != OK) {
        endwin();
        fail("Could not initialize colors");
    }
    init_colors();

    // Start windows
    c_win = newwin(25, 56, 0, 0);
    s_win = newwin(25, 24, 0, 57);

	// Display screen
	clear();
	init_screen();
	touchwin(c_win);
	wrefresh(c_win);
	touchwin(s_win);
	wrefresh(s_win);
	// Listen to server and keyboard
	struct pollfd pfds[2] = { {STDIN_FILENO, POLLIN, 0}, {client, POLLIN, 0} };
	bool done = false;
	bool found;
	int read_int;
	int x;
	int y;
	int pos;
	clear_buffer(cards_selected);
	for (;;) {
		switch(poll(pfds, 2, -1)) {
		case 0:
		case -1:
			fail("poll failed\n");
			// Poll found activity
		default:
			// If the the keyboard had activity
			if ((pfds[0].revents & POLLIN) != 0) {
				k = getch();
				touchwin(c_win);
				wmove(c_win, 24, 35);
				wclrtoeol(c_win);
				// If quit
				if (k == '6') {
					done = true;
				}
                else if (k == ' ') { // Entered potential set
                    // Check that there are three cards
					if (cards_selected[0] == '\0' or cards_selected[1] == '\0' or cards_selected[2] == '\0') {

					}

					// Otherwise cards are valid and ready to be sent
					else {
						//mvwprintw(c_win, 24, 40, "%c%c%c", cards_selected[0], cards_selected[1], cards_selected[2]);
						send_data();
						
						// redraw cards
						for (int i = 0; i < 3; i++) { 
							pos = (int)(cards_selected[i] - 'a');
							card_pos(pos, x, y);
							disp_card(x, y, cards_selected[i], pos, false);
						}
						clear_buffer(cards_selected);
						wattron(c_win, COLOR_PAIR(6));
						mvwprintw(c_win, 24, 41, "space entered");
						//refresh();
						
                    }
                }
				else if (k == 'x') {
					for (int i = 0; i < 3; i++) {
						if (cards_selected[i] != '\0') { 
							card_pos((int)(cards_selected[i] - 'a'), x, y);
							disp_card(x, y, cards_selected[i], cards_selected[i] - 'a', false);
						}
					}
					clear_buffer(cards_selected);
					cards_selected[0] = 'x';
					cards_selected[1] = 'x';
					cards_selected[2] = 'x';
					send_data();
					mvwprintw(c_win, 24, 41, "No-Set");
					clear_buffer(cards_selected);
				}
				else if (k > 'l' or k < 'a') { // If non valid number
					mvwprintw(c_win, 24, 41, "Invalid char");
				}
				else if (k <= 'l' and k >= 'a') { // valid card
					// Check that card position is still active
					pos = (int)(k - 'a');
					if (board[pos].color == 3) {
						break;
					}
					else { // Otherwise card position is still valid
						// Select or Deselect
						found = true;
						bool full = true;
						for (int i = 0; i < 3; i++) { 
							if (cards_selected[i] == '\0') { 
								full = false;
							}
						}

						for (int i = 0; i < 3; i++) {
							if (cards_selected[i] == k) {
								found = false; // inverse the selection
								cards_selected[i] = '\0';
								break;
							}
						}
						if (found) { 
							for (int i = 0; i < 3; i++ ) {
								if (cards_selected[i] == '\0') {
									cards_selected[i] = (char)(k);
									break;
								}
							}
						}
						if (full) {
							found = false;
						}
						// Display card
						card_pos(pos, x, y);
						disp_card(x, y, k, pos, found);
						//refresh();
					}
				} // end valid card
				//disp_last_input(k);
				touchwin(c_win);
				wattron(c_win, COLOR_PAIR(6));
				wmove(c_win, 23, 0);
				wclrtoeol(c_win);
				mvwprintw(c_win, 23, 0, "%c", k);
				mvwprintw(c_win, 24, 35, "%c%c%c", cards_selected[0], cards_selected[1], cards_selected[2]); 
				wrefresh(c_win);
			} // end if pfds[0]
			
			if ((pfds[1].revents & POLLIN) != 0) { // If server has
				read_int = receive_data(); // Fill input buffer
				//touchwin(c_win);
				//mvwprintw(c_win, 21, 24, "read %d data just now", read_int);
				wrefresh(c_win);
				// If sent no data
				if (read_int == 0) {
					done = true;
				}
				// If data read
				else {
					parse_input();
					//refresh();
				}
			} // end if server
		} // end poll switch
		touchwin(s_win);
		wrefresh(s_win);
		touchwin(c_win);
		wrefresh(c_win);
		
		// check game is done
		if (done == true) {
			break;

		}
	} // Done game for loop

	//	DISPLAY SCORES
	clear();
	// Recieve final scores
	
	// Assure scores are ordered
	int j;
	int value;	
	for (int i = 1; i < num_players; i++) {
		value = players[i].Score;
		char name[NAME_LEN + 1];
		clear_buffer(name);
		strncpy(name, players[i].Name, strlen(players[i].Name));
		pos = i;
		j = i - 1;
		while ((j >= 0) and (players[j].Score < value)) {
			clear_buffer(players[j+1].Name);
			strncpy(players[j+1].Name, players[j].Name, strlen(players[j].Name));
			players[j+1].Score = players[j].Score;
			j -= 1;
		}
		clear_buffer(players[j+1].Name);
		strncpy(players[j+1].Name, name, strlen(name));
		players[j+1].Score = value;
	}

	// Print out tallied scores
	int place = 1;
	mvprintw(5, 26, "%d) %s", place, players[0].Name); 
	mvprintw(5, 45, "%d", players[0].Score);
	
	for (int i = 1; i < num_players; i++) {
		if (players[i].Score == players[i - 1].Score) { 
    	    mvprintw(5 + i, 26, "%d) %s", place, players[i].Name);
	        mvprintw(5 + i, 45, "%d", players[i].Score);
		}
		else {
			mvprintw(5 + i, 26, "%d) %s", ++place, players[i].Name);
			mvprintw(5 + i, 45, "%d", players[i].Score);
		}
	}
	mvprintw(24, 35, "Press '6' to exit");
	refresh();

	// Wait for quit response
	for (;;) { 
		k = getch();
		if (k == '6') break; 
	}

	// Close out of server
	endwin();
	close(client);
}

// Assure te reading of x bytes to input buffer, returns 0 on success
int read_bytes(int pos, char* buf) {
	int bytes_read = 0;
	int result;
	// Read through, appending to buffer, until the correct number of bytes are read in
	clear_buffer(buf);
	///*	
	while (bytes_read < pos) {
		result = read(client, buf + bytes_read, pos - bytes_read);
		if (result < 1) fail("Could not read bytes from server");
		bytes_read += result;
		
	}//*/
	//if (read(client, buf, pos) == -1) fail("Couldn't read from server");
	//touchwin(c_win);
	//mvwprintw(c_win, 19, 10, "raw read_bytes %s", buf); 
	//wrefresh(c_win);
	return bytes_read;
}

int receive_data() { 
	char temp[] = {'1', '\0', '\0'}; // the ammount of sent bytes will be less than 999
	int bytes_to_be_read = 0;
	char temp_buf[4] = {'\0','\0', '\0', '\0'};
	// Read incoming  from server
	touchwin(c_win);
	int r = read_bytes(3, temp_buf);
	//mvwprintw(c_win, 21, 0, "rd %d bytes", r);
	// Parse code and bytes to be read {'c','1','1','1'...}
	// Read bytes to beo read
	// convert char to an integer
	bytes_to_be_read = atoi(temp_buf);
		
	// Send server 'ready' code
    if (write(client, &temp, 1) == -1) fail("Could not send code response to server");
	// Read incoming data until all bytes have ben read
	clear_buffer(input);
	read_bytes(bytes_to_be_read, input);
	//if (read(client, &aba, 120) == -1) fail("Couldn't read from server");

	// Send server 'all good' response
	int x = write(client, &temp, 1);
    if (x == -1) fail("Could not send code response to server");
    //mvwprintw(c_win, 18, 0, "sent %s with %d bytes to server", temp, x);

	//wattron(c_win, COLOR_PAIR(6));
	//mvwprintw(c_win, 22, 10, "read %d bytes of input", bytes_to_be_read);
	//mvwprintw(c_win, 23, 5, "input: %s", input);
	//wrefresh(c_win);
	return bytes_to_be_read;
}

void send_data() { 
	if (write(client, &cards_selected, 3) == -1) fail("Could not send cards");
	char temp_buf[] = {'\0', '\0'};
	if (read(client, &temp_buf, 1) == -1) fail("Couldn't recieve response");
}


// Reduce name to 14 chars
void accept_username(char username[]) {
	// Null terminate all bytes after the name
	//cout << "username is " << strlen(username) << " bytes" << endl;
	for (unsigned int c = NAME_LEN - 2; c < strlen(username); c++) {
		username[c] = '\0'; // This assumes username ends in null terminating bytes if it doesn't hit full length
	}
}


void init_username(char* username) {
	//cout << "initial username is " << username << "." << endl;
	// Write username to server
	if (write(client, username, NAME_LEN + 1) == -1) fail("Couldn't initialy write username");

	// Wait for a response
	if (read(client, &input, sizeof input) == -1) fail("Couldn't read username");

	//cout << "read " << input << " from client" << endl;

	// Check if client was accepted
	if (input[0] == '3') {
		rejected();
	}	
	else {
		// Put username as player name
		strncpy(player_name, input, strlen(input));
		//cout << "Final username is " << player_name << "." << endl;
	}
}

void init_client() {
    client = socket(AF_INET, SOCK_STREAM, 0);
    if (client == -1) fail("Couldnt create client");

    struct sockaddr_in address = {}; // must initialize to all zeros
    address.sin_family = AF_INET;
    address.sin_port = htons(port); // connect to specified port number
    if (inet_pton(AF_INET, SERVER_IP, &address.sin_addr) <= 0)
        fail("can't convert IP address");

    // Now connect our socket to the server's socket
    if (connect(client, reinterpret_cast<struct sockaddr*>(&address), sizeof address) == -1)
        fail("can't connect to server");
}

void init_colors() {
	// Define colors
	//init_color(20, 75, 75, 0) // Yellow

	// Define pairs
	// Normal Card
	init_pair(0, COLOR_WHITE, COLOR_WHITE);
	init_pair(1, COLOR_RED, COLOR_WHITE); //
	init_pair(2, COLOR_BLUE, COLOR_WHITE); //
	init_pair(3, COLOR_YELLOW, COLOR_WHITE); //
	// Normal colors
	init_pair(6, COLOR_WHITE, COLOR_BLACK);
	init_pair(7, COLOR_YELLOW, COLOR_YELLOW);
	// Selected Card
	init_pair(10, COLOR_CYAN, COLOR_CYAN);
	init_pair(11, COLOR_RED, COLOR_CYAN); //
	init_pair(12, COLOR_BLUE, COLOR_CYAN); //
	init_pair(13, COLOR_YELLOW, COLOR_CYAN); //
}


void clear_card_space(int x, int y) {
	touchwin(c_win);
	wattrset(c_win, COLOR_PAIR(6)); // black out
	for (int row = 0; row < 5; row++) {
		mvwhline(c_win, y + row, x, ' ', 13);
	}
	//refresh();
}

void disp_card(int x, int y, char c, int slot, bool select) {
	touchwin(c_win);
    // Grab card attributes
    int c_color = board[slot].color;
    int c_shape = board[slot].shape;
	int c_num = board[slot].number;
    int c_shade = board[slot].shading;
	wattron(c_win, COLOR_PAIR(6));
	//mvwprintw(c_win, 22, 0, "%d%d%d%d%d", c_color, c_shape, c_num, c_shade, select);
	//refresh();
	// If card is a blank card then clear the spot on the board
	if (c_color == 3) {
		clear_card_space(x, y);
	}
	else { // Otherwise card is valid and can be displayed
		touchwin(c_win);
		int color;
	    // Determin color of shapes and background : reverse so color shows up
	    if (select) {
	        color = 10;
	        wattron(c_win, COLOR_PAIR(color));
			wattron(c_win, A_REVERSE);
	    }
	    else {
	        color = 0;
	        wattron(c_win, COLOR_PAIR(color));
			wattron(c_win, A_REVERSE);
	    }
	
	    // Draw Background
	    for (int row = 0; row < 5; row++) {
	        mvwhline(c_win, y + row, x, ' ', 13);
	    }
	
	    // Draw Shapes
    	int line_mover = 0; // Says how much farther right to put the shape
    	color += c_color + 1;
		wattron(c_win, COLOR_PAIR(color));
		wattroff(c_win, A_REVERSE); // Turn off reversed colors
	
    	// Draw one shape
    	char shades[3] = {' ', 'o', '@' };
	    char shade = shades[c_shade];
	
	    // If the shade is blank then reverse colors so it shows up
	    if (c_shade == 0) { wattron(c_win, A_REVERSE); }
	
	    // Draw 1 shape
	    if (c_num == 0) {
	        // Shape is a CROSS
    	    if (c_shape == 0) {

	            mvwaddch(c_win, y + 1, x + 6, shade);
        	    mvwhline(c_win, y + 2, x + 5, shade, 3);
    	        mvwaddch(c_win, y + 3, x + 6, shade);
	        }

        	// Shape is a CIRCLE
    	    else if(c_shape == 1) {
	            mvwhline(c_win, y + 1, x + 5, shade, 3);
            	mvwaddch(c_win, y + 2, x + 5, shade);
        	    mvwaddch(c_win, y + 2, x + 7, shade);
    	        mvwhline(c_win, y + 3, x + 5, shade, 3);
	        }

        	// Shape is a SLASH
    	    else {
	            mvwaddch(c_win, y + 1, x + 7, shade);
            	mvwaddch(c_win, y + 2, x + 6, shade);
        	    mvwaddch(c_win, y + 3, x + 5, shade);
    	    }
	    } // end 1 shape

    	// Draw 2 shapes
	    else if (c_num == 1) {
    	    // Shade is a CROSS
	        if (c_shape == 0) {
	            for (int i = 0; i < 2; i++) {
            	    mvwaddch(c_win, y + 1, x + line_mover + 3, shade);
        	        mvwhline(c_win, y + 2, x + line_mover + 2, shade, 3);
    	            mvwaddch(c_win, y + 3, x + line_mover + 3, shade);
	                line_mover += 6;
            	}
        	}

    	    // Shape is a CIRCLE
	        else if (c_shape == 1) {
        	    for (int i = 0; i < 2; i++) {
    	            mvwhline(c_win, y + 1, x + line_mover + 2, shade, 3);
	                mvwaddch(c_win, y + 2, x + line_mover + 2, shade);
                	mvwaddch(c_win, y + 2, x + line_mover + 4, shade);
            	    mvwhline(c_win, y + 3, x + line_mover + 2, shade, 3);
        	        line_mover += 6;
    	        }
	        }

        	// Shape is a SLASH
    	    else {
	            for (int i = 0; i < 2; i++) {
            	    mvwaddch(c_win, y + 1, x + line_mover + 4, shade);
        	        mvwaddch(c_win, y + 2, x + line_mover + 3, shade);
    	            mvwaddch(c_win, y + 3, x + line_mover + 2, shade);
	                line_mover += 6;
    	        }
	        }

    	} // end if two shapes

	    else {
        // Shape is a CROSS
    	    if (c_shape == 0) {
	            for (int i = 0; i < 3; i++) {
            	    mvwaddch(c_win, y + 1, x + line_mover + 2, shade);
        	        mvwhline(c_win, y + 2, x + line_mover + 1, shade, 3);
    	            mvwaddch(c_win, y + 3, x + line_mover + 2, shade);
	                line_mover += 4;
        	    }
    	    }

	        // Shape is a CIRCLE
        	else if (c_shape == 1) {
    	        for (int i = 0; i < 3; i++) {
	                mvwhline(c_win, y + 1, x + line_mover + 1, shade, 3);
                	mvwaddch(c_win, y + 2, x + line_mover + 1, shade);
            	    mvwaddch(c_win, y + 2, x + line_mover + 3, shade);
        	        mvwhline(c_win, y + 3, x + line_mover + 1, shade, 3);
    	            line_mover += 4;
	            }
        	}

    	    // Shape is a SLASH
	        else {
            	for (int i = 0; i < 3; i++) {
        	        mvwaddch(c_win, y + 1, x + line_mover + 3, shade);
    	            mvwaddch(c_win, y + 2, x + line_mover + 2, shade);
	                mvwaddch(c_win, y + 3, x + line_mover + 1, shade);
            	    line_mover += 4;
        	    }
    	    }
	    } // end if 3 shapes

    // Turn off attributes
    if (c_shade == 0) wattroff(c_win, A_REVERSE); 
		
	}
	
	wattron(c_win, COLOR_PAIR(6));
	mvwprintw(c_win, y, x, "%c", c);
	//refresh();
} // end draw card


void card_pos(int pos, int& x, int& y) {
	// Print letter
	int col = pos % 4; // Attain col
	int row = pos / 4;

	// Find x y coords
	y = 1 + (row * 6);
	x = col * 14;
}


// Find x, y starting coords for player score
void score_pos (int pos, int& x, int& y) {
	x = 2; // start two spaces over or 58?
	y = pos * 2; // at y equivalent to player position * 2
}


void draw_board() { // Fills board with cards in global board struct
	int x;
	int y;
	char c = 'a'; // incremented to b, then c...
	touchwin(c_win);
	// Draw cards
	for (int i = 0; i < 12; i++) {
		// If slot is empty black out spot
		if (board[i].color == 3) {
			// do nothing
		}
		else {
			// Find card position
			card_pos(i, x, y);
			// Draw unselected card
			disp_card(x, y, c++, i, false);
			//refresh();
		}
	}	
}

void accept_players() {
	// Go through the sent names and find individual names
	int pos = 0;
	char buffer[NAME_LEN + 1];
	//cout << "the raw player buffer is " << input << endl;
	for (int i = 0; i < 12; i++ ) {
		// Find name
		clear_buffer(buffer);
		for (int c = 0; c < (NAME_LEN + 1); c++) { 
			// If input is a space then full name is found
			if (input[pos] == ' ') {
				pos++;
				//cout << "found " << buffer << endl;
				break;
			}
			// Otherwise append it to the name buffer
			else {
				buffer[c] = input[pos]; // copy character over to buffer
			}
			pos++; // increment position in input buffer
		}


		// Put name in struct
		strncpy(players[i].Name, buffer, strlen(buffer)); // Move found name into player data
		players[i].Score = 0; // Initialize score to 0
		num_players++; // Increment number of players
		//cout << "player " << i << " is " << players[i].Name << endl;
		// Check if this was the last name then stop making new names
		if (input[pos] == '<') {
			break;
		}
	}
	//cout << "done with accepting players" << endl;
	clear_buffer(input);
}


void init_players() { 
	touchwin(s_win);
	wattron(s_win, COLOR_PAIR(6));
	int x;
	int y;
	for (int i = 0; i < num_players; i++) {
		// Find card position
		score_pos(i, x, y);
		// Display player and score of 0
		disp_score(x, y, players[i].Name, 0);
	}
	wattron(s_win, A_REVERSE);
	mvwvline(s_win, 0, 0, ' ', 25);
	wattroff(s_win, A_REVERSE);
	//refresh();
}


void disp_score(int x, int y, char name[], int score) {
	touchwin(s_win);
	wattron(s_win, COLOR_PAIR(6));
	// Clear characters inline
	//wmove(s_win, y, x);
	//wclrtoeol(s_win);
	wmove(s_win, y + 1, x);
	wclrtoeol(s_win);
	//refresh();	
	// Print name and score
	mvwprintw(s_win, y, x, "%s", name);
	mvwprintw(s_win, y + 1, x + 1, "%d", score);
	wrefresh(s_win);
}


void draw_options() {
	touchwin(c_win);
	wattron(c_win, COLOR_PAIR(6));
	mvwprintw(c_win, 24, 0, "QUIT: 6 | NO-SET: X | LAST-INPUT: "); // stops at 34 inclusivei
	wrefresh(c_win);
}


void disp_last_input(char c) {
	touchwin(c_win);
	wattron(c_win, COLOR_PAIR(6));
	wmove(c_win, 24, 35);
	wclrtoeol(c_win);
	mvwprintw(c_win, 23, 0, "%c", c);
	// Have check for c not being between A - L
	// if x is selected it must be deselected
	// If c is a no-set character
	/*
	if (c == 'x') {
		if (cards_selected[0] == 'x') {
			clear_buffer(cards_selected);
		}
		else {
			clear_buffer(cards_selected);
			cards_selected[0] = 'x';
			cards_selected[1] = 'x';
			cards_selected[2] = 'x';
		}
	}
	*/
	if (c == '6') {
		if (cards_selected[0] == '6') {
			clear_buffer(cards_selected);
		}
		else {
			clear_buffer(cards_selected);
			cards_selected[0] = '6';
			cards_selected[1] = '6';
			cards_selected[2] = '6';
		}
	}

	// if C is a space char do nothing
	else if (c == ' ') {
		// Check if selected cards are valid
		if (cards_selected[0] == '\0' or cards_selected[1] == '\0' or cards_selected[2] == '\0') {
			mvwprintw(c_win, 24, 41, "Invalid");
		}
		else {
		// Clear selected cards 
		cards_selected[0] = '\0';
		cards_selected[1] = '\0';
		cards_selected[2] = '\0';

		// Display sent message
		mvwprintw(c_win, 24, 41, "Sent");
		}
	}

	// If c is not a valid card character
	else if (c > 'l' or c < 'a') {
		mvwprintw(c_win, 24, 41, "Invalid");
	}

	// Otherwise c is valid character
	else {
		// Check if valid card (if space is empty)
		if (board[(int)(k - 'a')].color == 3) {
			mvwprintw(c_win, 24, 41, "Invalid");
		}

		// Position is an actual card
		else {
			bool found = false;
			// See if character was duplicated
			for (int i = 0; i < 3; i++) {
				if (cards_selected[i] == c) {
					cards_selected[i] = '\0';
					found = true;
				}
			}

			// If it wasn't deselected then see if there is space for it
			if (!found) {
				for (int i = 0; i < 3; i++) {
					if (cards_selected[i] == '\0') {
						cards_selected[i] = c;
						break;
					}
				}
			}
		}
	} // end k is valid card position

	// Print currently selected cards
	mvwprintw(c_win, 24, 35, "%c%c%c", cards_selected[0], cards_selected[1], cards_selected[2]);
	wrefresh(c_win);
}


void init_screen() {
	// Draw cards
	draw_board();

	// Draw players and scores
	init_players();

	// Draw Control character options
	draw_options();
}

void explosion(int card_slots[], int num) {
	int phase = 0;
	int rows = 11;
	int x;
	int y;

	touchwin(c_win);
	// Implode
	for (int i = 0; i < 6; i++) { // For each phase
		for (int j = 0; j < num; j++) { // For each card
			card_pos(card_slots[j], x, y); // Find card position for card
			wattroff(c_win, A_REVERSE);
			clear_card_space(x, y);
			wattron(c_win, COLOR_PAIR(7));
			wattron(c_win, A_REVERSE);
			for (int r = 0; r < 5; r++) { // For each row
				mvwhline(c_win, y + r, (x + (i + 1)), ' ', rows);
			}
		}
		rows -= 2;
		wrefresh(c_win);
		usleep(50000); 
	}
	rows += 2;
    // Explode
    for (int i = 0; i < 5; i++) { // For each phase
        for (int j = 0; j < num; j++) { // For each card
            card_pos(card_slots[j], x, y); // Find card position for card
			wattroff(c_win, A_REVERSE);
			clear_card_space(x, y);
			wattron(c_win, COLOR_PAIR(7));
			wattron(c_win, A_REVERSE);
            for (int r = 0; r < 5; r++) { // For each row
                mvwhline(c_win, y + r, (x + (6 - i - 1)), ' ', rows);
            }
        }
        rows += 2;
		wrefresh(c_win);
		usleep(50000);
    }
}

void init_board() { // Fill board with cards from server
	// Initialize cards
	// Go through input buffer
	Color color[3] = {RED, BLUE, YELLOW};
	Shape shape[3] = {CROSS, CIRCLE, SLASH};
	Shading shade[3] = {LOW, MEDIUM, HIGH};
	Number num[3] = {ONE, TWO, THREE};
	
	int pars[4];

	cout << endl;

	char* c = &input[0]; // pointer to first char of input buffer
	for (int i = 0; i < 12; i++) {
		// Find integer values for card
		pars[0] = color[(int)((*c++) - '0')];
		pars[1] = shape[(int)((*c++) - '0')];
		pars[2] = num[(int)((*c++) - '0')];
		pars[3] = shade[(int)((*c++) - '0')];
		cout << pars[0] << pars[1] <<  pars[2] <<  pars[3]; 
		board[i] = Card(color[pars[0]], shape[pars[1]], num[pars[2]], shade[pars[3]]);
	}
	//cout << "\ndone with board init" << endl;
	// Clear buffer
	clear_buffer(input);
}

void parse_input() {
	char parsed[11]; // 10 digits available... even though screen can't show them currently...
	int pos = 0;
	int x;
	int y;
	char* c = &parsed[0]; // temporary will be overwritten
	
	// Already read input, now parse input
	for (unsigned int i = 2; i < strlen(input); i++) {
		// If current char is the < identifier then full score has been found
		if (input[i] == '<') {
			break;
		}
		else {
			parsed[pos++] = input[i];
		}
	}
	parsed[pos] = '\0'; // make sure its nul terminating
	//touchwin(c_win);
	//mvwprintw(c_win, 21, 20, "score: %d", atoi(parsed));
	//wrefresh(c_win);
	wmove(c_win, 24, 50);
	// Update player data
	// the first char of input buffer is position of player in array (a - 65 = 0)
	pos = (int)(input[0] - 65); // convert to int
	
	players[pos].Score = atoi(parsed); // Place new score in player score
	//touchwin(c_win);
	//mvwprintw(c_win, 20, 10, "New score is %d", atoi(parsed));
	//wrefresh(c_win);

	// Update the name and score on screen
	touchwin(s_win);
	score_pos(pos, x, y);
	disp_score(x, y, players[pos].Name, players[pos].Score);
	wrefresh(s_win);
	// Update cards if necessary
	for (pos = 2; pos < strlen(input); pos++) {
		if (input[pos] == '>') {
			break;
		}
	}
	// Increment forward once more
	pos++;

	Color color[4] = {RED, BLUE, YELLOW, NONE1};
	Shape shape[4] = {CROSS, CIRCLE, SLASH, NONE2};
	Number num[4] = {ONE, TWO, THREE, NONE3};
	Shading shade[4] = {LOW, MEDIUM, HIGH, NONE4};
	int position[4];
	// If the position is not filled then there are no cards to read, just player data
	if (input[pos] == '\0') {
		
	}
	
	// If the following cards are no set cards
	else if (input[pos] == 'x') {
		// Run through 4 ints per card
		c = &input[pos + 1];
		pos++; // increment to start of numbers
		for (int i = 0; i < 6; i++ ) {
			for (int j = 0; j < 4; j++) {
				position[j] = (int)(*c++ - '0');
			}
			board[i] = Card(color[position[0]], shape[position[1]], num[position[2]], shade[position[3]]);
		}

        // Display new cards                                !!! EXPLOSION !!!
		for (int i = 0; i < 6; i++) {
			position[i] = i;
		}
		explosion(position, 6);
        for (int i = 0; i < 6; i++) {
            card_pos(i, x, y);
			disp_card( x, y, (char)(i + 65), i, false); // Handles blank cards
        }	
	}
	
	// If there are only three new cards...
	else { //if (input[pos] > 65 and input[pos] < 77{ 
		// Put cards onto board
		c = &input[pos + 3]; // Pointer to first of the card integers
		for (int i = 0; i < 3; i++) {
			//x = 0;
			//y = pos + 3 + (i + 1) + i; // position of starting letter
			//board[(int)(input[pos+i]-65)] = Card((int)(input[y + (x++)]), (int)(input[y + (x++)]), 
			// Increment char pointer forward to next card integer
			for (int j = 0; j < 4; j++) {
				position[j] = (int)(*c++ - '0');
			}
			board[(int)(input[pos + i] - 'a')] = Card(color[position[0]], shape[position[1]], num[position[2]], shade[position[3]]);

		}

		// Display new cards								!!! EXPLOSION !!!
		for (int i = 0; i < 3; i++) { 
			position[i] = (int)(input[pos + i] - 'a');
		}
		explosion(position, 3);
		for (int i = 0; i < 3; i++) {
			card_pos((int)(input[pos + i] - 'a'), x, y);
			disp_card( x, y, input[pos + i], (int)(input[pos + i] - 'a'), false);
		}
	}
	//refresh();
}

// Countdown
void timer_wait() {
	// Simlply wait to recieve 'z' from server
	if (read(client, &input, sizeof input) == -1) fail("Couldn't read start game char");

	//cout << "timer_wait : " << input << endl;

	if (input[0] != 'z') fail("Read the wrong input in timer_wait");
	clear_buffer(input);

	char temp[3];
	clear_buffer(temp);
	temp[0] = '1';
	write(client, &temp, sizeof temp);
	
	read(client, &input, sizeof input);
	//cout << "board data: " << input << endl;
	write(client, &temp, sizeof temp);
	init_board();
	clear_buffer(input);
	
	read(client, &input, sizeof input);
	//cout << "player data: " << input << endl;
	accept_players();
	write(client, &temp, sizeof temp);
}


void rejected() {
	cout << "You are too late, try again next game." << endl;
	close(client);
	exit(EXIT_SUCCESS);
}


void cleanup(int sig) {
    endwin();
    close(client);
    exit(EXIT_SUCCESS);
}


void fail(string message) {
    cerr << strerror(errno) << ": " << message << endl;
    cleanup(0);
}

