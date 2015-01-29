#include <iostream>
#include <cstdlib>
#include <cerrno>
#include <string>
#include <cstring>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <fcntl.h>
#include <semaphore.h>
#include <pthread.h>
#include <poll.h>
#include "deck.h"
#include "network.h"

using namespace std;

#define SERVER_IP  "127.0.0.1"

int listening_server;	// Global FileDescriptor for server process
//struct pollfd pfds[12]; // array of file descriptors for 
int num_players = 0;
const int MAX_PLAYERS = 12;
constexpr int READ_SIZE = 1024;
constexpr int WRITE_SIZE = 1024;
char input[WRITE_SIZE];
//char return_val[WRITE_SIZE];
int fd_return;
BOARD board; // card.h typedef CARD BOARD[12]
DECK deck; // card.h typedef CARD DECK[81]
PLAYER players;
int current_deck_pos = 0; 

pthread_t threads[12];
int thread_count = 0;

pthread_t start_thread; // handles accept/rejection of clients
bool accepting_clients = true; // Global so start_thread doesn't have to check timer

pthread_t game_timer_thread; // decrements time_left every second
int duration; // Given duratoin to wait to start game, in seconds

int port;
int client;

int streak = 3;
int last_player = 13;

sem_t connect_lock;
sem_t pregame_lock;
sem_t server_busy;
sem_t player_update;
sem_t check_cards;
sem_t begin_game;
sem_t end_game;

int fib(int n) {
	if (n == 1)
		return 1;
	else if (n == 2)
		return 1;
	else
		return fib(n-1) + fib(n-2);
}

void wakeup (int sig) {
	// curently doesn't do anythings, just catches the SIGALRM signall
}

void cleanup(int sig) {
	close(listening_server);
	exit(EXIT_SUCCESS);
}

void fail(string message) {
	cerr << strerror(errno) << ": " << message << endl;
	cleanup(0);
}

// Make sure player score data isn't lossed
void update_score(int p, int n) {
	// lock semaphore on players
	players[p].Score += n; // add pos/neg score to player's score
	// unlock semaphor on players
}

int init_connection() { // will return File Descriptot for client to read/write from
	struct sockaddr_in client_addr;
    socklen_t client_len = sizeof client_addr;
    
	cout << "Waiting to accept clients...." << endl;
	int result = accept(listening_server, reinterpret_cast<struct sockaddr*>(&client_addr), &client_len);
	if (result == -1) fail("Bad client/server connection");

	return result;	
}

void init_player(char name[], int port_num, int c_fd) {
	Player player;
	strncpy(player.Name, name, strlen(name));
	player.Port = port_num;
	player.Client_FD = c_fd;
	player.Score = 0;
	// add to list of players
	players[num_players++] = player;
}

void init_board() {
	for (int i = 0; i < 12; i++) {
		board[i] = deck[i];
	}
	current_deck_pos = 12;	// use for next access into the deck
}

void send_board(int fd) {   // have to pass which socket fd to write to
	int pos = 0;
	for (unsigned int i = 0; i < 12; i++) {
		input[pos++] = static_cast<char>(board[i].color + '0');
		input[pos++] = static_cast<char>(board[i].shape + '0');
		input[pos++] = static_cast<char>(board[i].number + '0');
		input[pos++] = static_cast<char>(board[i].shading + '0');
	}
	input[pos++] = '\0'; //add a terminating zero byte to make sure strlen works right
	// send data to all players
	//char temp[] = {'\0', '\0', '\0'};
	write(fd, &input, strlen(input));

	clear_buffer(input);
}

void send_players(int fd) {
	clear_buffer(input); // make sure no data is in input
	int pos = 0;
	for (int i = 0; i < num_players; i++) {
		Player player = players[i];
		for (unsigned int j = 0; j < strlen(player.Name); j++) {
			input[pos++] = player.Name[j];
		}
		input[pos++] = ' '; // put spcae between names
	}
	input[pos++] = '<';
	input[pos++] = '>';
	input[pos++] = '\0';  // making sure strlen works by adding terminating zero byte
	// send to all players
	if (write(fd, &input, strlen(input)) == -1) fail("Failed writing player names to clients");
	clear_buffer(input);
}

int read_bytes(int fd, int pos, char* buf) {
	int bytes_read = 0;
	int result;
	// Read through, appending to buffer, until the correct number of bytes are read in
	while (bytes_read < pos) {
		result = read(fd, buf + bytes_read, pos - bytes_read);
		if (result < 1) fail("Failed to read bytes from server");
		bytes_read += result;
	}
	return bytes_read;
}

void send_data(int fd, char data[]) {
	//Send the code for what is being sent and the ammount of data to be read by client
	int bytes_sent = strlen(data);
	char bytes_sent_c[3] = {'\0','\0', '\0'}; // shouldn't be any more than 4 chars
	sprintf(bytes_sent_c, "%d", bytes_sent); // put integer into the array
	char temp_buf[3] = {'\0', '\0', '\0'};
	strncat(temp_buf, bytes_sent_c, 3); // put bytes to be read onto input buffer
	for (int i = 0; i < 3; i++) { // make sure thier are always 5 bytes being sent
		if (temp_buf[i] == '\0') {
			temp_buf[i] = '*';
		}
	}
	if (write(fd, &temp_buf, 3) == -1) fail("Failed to send code to client");
	//cout << "Wrote " << temp_buf << " to client." << endl;

	// Wait for response before continuing
	clear_buffer(temp_buf);
	read_bytes(fd, 1, temp_buf);
	//cout << "read " << temp_buf << " from client" << endl;
	// Send data
	int data_written = write(fd, data, strlen(data));
	cout << "Sent data " << data << " to client with " << data_written << " bytes" << endl;
	clear_buffer(temp_buf);
	read_bytes(fd, 1, temp_buf);
	
	//cout << "Read response \"" << temp_buf << "\"" << endl;
}

int receive_data(int fd, char buf[]) {
	// Will always recieve 3 chars
	clear_buffer(buf);
	int result = read_bytes(fd, 3, buf);
	cout << "Read in " << buf << " from client" << endl;
	char temp_buf[] = {'1'};
	if (write(fd, &temp_buf, 1) == -1) fail("Failed to write response to client");
	return result;
}

bool name_exists(char name[]) {
	char temp[NAME_LEN+1];
	strncpy(temp, name, strlen(name));
	for (int i = 0; i < num_players; i++) {
		// If the two names are the same return true
		if (strcmp(temp, players[i].Name) == 0) return true;
	}
	return false;
}

void* handle_player(void* arg) {
	// Current player's connection is set, play game
	int player_index = *reinterpret_cast<int*>(arg);
	int fd = players[player_index].Client_FD;
	// added for simplicity's sake when printing descriptive messages
	char player_name[NAME_LEN+1];
	strncpy(player_name, players[player_index].Name, strlen(players[player_index].Name));
	
	cout << "player " << player_name << " is waiting for response from 'z'" << endl;
	
	char temp[] = {'\0', '\0', '\0'};
	read(fd, &temp, sizeof temp);
	cout << "Received " << temp << " after sending 'z'" << endl;
	
	sem_wait(&server_busy);
	cout << "Sending board in thread" << endl;
	send_board(fd);
	clear_buffer(temp);
	
	cout << "Waiting for board-accepting response from client" << endl;
	read(fd, &temp, sizeof temp);
	cout << "Received " << temp << " after sending board" << endl;

	clear_buffer(input);
	cout << "Sending player names in thread" << endl;
	send_players(fd);
	
	clear_buffer(temp);
	cout << "Waiting for player-accepting response from client" << endl;
	read(fd, &temp, sizeof temp);
	cout << "Received " << temp << " after sending players" << endl;
	sem_post(&server_busy);
	sem_wait(&begin_game);
	cout << "player " << player_name << " is running" << endl;

	// Poll on filedescriptor to client: set, no-set, quit(0)
	struct pollfd pollfds[] = { {fd, POLLIN, 0} };
	bool done = false;
	char data[3]; // each thread gets its own read-in buffer

	while(!done) {
	switch (poll(pollfds, 1, -1)) {
	// if negative value passed to poll fct, no timeout
	case 0:
	case -1:
		fail("Poll fail");
	default:
		if ((pollfds[0].revents & POLLIN) != 0) { // received data from client
			sem_wait(&server_busy); // lock server to handle one client at a time
			
			cout << "Server_busy is locked" << endl;
			//clear_buffer(data); 
			int r = receive_data(fd, data);
			//int r = read(fd, &data, sizeof data);
			//if (r == -1) fail("Couldn't read from client in thread");
			if (r == 0) { // Player closed
				done = true;
				break; //		!!!! Does this break from default? !!!!
			}
			cout << "Player " << player_name << " sent \"" << data << "\"" << endl;
			bool was_set = false;
			bool was_no_set = false;
			char char1 = data[0];
			char char2 = data[1];
			char char3 = data[2];

			// Player quit
			if (char1 == '6') {
				// player wants to quit
				cout << "Player " << player_name << " has quit" << endl;
				done = true;
				//num_players--;
				break; //		!!!! Does this break from default? !!!
			}
			// Player says there is no set
			else if (char1 == 'x') {
				cout << "Player guessed NO SET" << endl;
				// If board has no set
				if (is_no_set(board, true)) {
					// add to player's score
					if (player_index == last_player) {
						players[player_index].Score += 10 + fib(streak);
						streak++;
					}
					else {
						players[player_index].Score += 10;
						last_player = player_index;
						streak = 3;
					}
					last_player = player_index;
					was_no_set = true;
				}
				// Otherwise player was wrong, there is a set
				else {
					// decrement player's score
					if (player_index == last_player) {
						players[player_index].Score -= 5;
						streak = 3;
						last_player = 13;  // some invalid index
					}
					else
						players[player_index].Score -= 5;
				}
			}
			// Player sent a set
			else {
				// parse input for cards to check for set
				if (is_set(board[(int)char1 - 'a'], board[(int)char2 - 'a'], board[(int)char3 - 'a'])) {
					// increment player score
					if (player_index == last_player) {
						players[player_index].Score += 5 + fib(streak);
						streak++;
					}
					else {
						players[player_index].Score += 5;
						streak = 3;
						last_player = player_index;
					}
					was_set = true;
				}
				else {
					// decrement score
					if (player_index == last_player) {
						last_player = 13;
						streak = 3;
					}
					players[player_index].Score -= 3;
				}
			}
			
			//		SEND BACK RESPONSE
			clear_buffer(input);
			input[0] = (char) player_index + 65;
			input[1] = ' ';
			char tempBuff[8];
			clear_buffer(tempBuff);	// make sure no strange chars in allocated space
			sprintf(tempBuff, "%d", players[player_index].Score);
			strncat(input, tempBuff, strlen(tempBuff));	// add score to string to be sent to clients			
			size_t buf_index = strlen(input);	// find next index for input
			input[buf_index++] = '<';
			input[buf_index++] = '>';
			
			Card tempCard;
			if (was_set) {
				cout << "Server says it is SET" << endl;

				buf_index = strlen(input);
				input[buf_index++] = char1;
				input[buf_index++] = char2;
				input[buf_index++] = char3;
				
				if (current_deck_pos < 79) {
					tempCard = deck[current_deck_pos++];
					board[(int)char1 - 'a'] = tempCard;
					input[buf_index++] = static_cast<char>(tempCard.color + '0');
					input[buf_index++] = static_cast<char>(tempCard.shape + '0');
					input[buf_index++] = static_cast<char>(tempCard.number + '0');
					input[buf_index++] = static_cast<char>(tempCard.shading + '0');

					tempCard = deck[current_deck_pos++];
					board[(int)char2 - 'a'] = tempCard;
					input[buf_index++] = static_cast<char>(tempCard.color + '0');
                	input[buf_index++] = static_cast<char>(tempCard.shape + '0');
                	input[buf_index++] = static_cast<char>(tempCard.number + '0');
                	input[buf_index++] = static_cast<char>(tempCard.shading + '0');
	
					tempCard = deck[current_deck_pos++];
					board[(int)char3 - 'a'] = tempCard;
					input[buf_index++] = static_cast<char>(tempCard.color + '0');
                	input[buf_index++] = static_cast<char>(tempCard.shape + '0');
                	input[buf_index++] = static_cast<char>(tempCard.number + '0');
                	input[buf_index++] = static_cast<char>(tempCard.shading + '0');
				}
				else {		// deck is out of new cards
					cout << "No more cards left in deck" << endl;

					for (int i = 0; i < 12; i++) {	// send invalid nums for client
						input[buf_index++] = '3';	// client will black out area if it sees a '3'
					}
					// replace spots on board with "blank" cards
					tempCard.color = NONE1;
					tempCard.shape = NONE2;
					tempCard.number = NONE3;
					tempCard.shading = NONE4;
					board[(int) char1 - 'a'] = tempCard;
					board[(int) char2 - 'a'] = tempCard;
					board[(int) char2 - 'a'] = tempCard;
				}
			}
			else if (was_no_set) {	// replace 6 cards on the board
				cout << "Server says it was NO SET" << endl;
				
				if (current_deck_pos < 79) {
					input[buf_index++] = 'x';	// signal to client that it was a no-set
					// add board[a - f] cards' data to input string to be sent
					int temp_pos = current_deck_pos; // remember deck position
					Card card_buf[6];
					for (int i = 0; i < 6; i++) {  // put siz new cards from deck into temp card buffer
						card_buf[i] = deck[current_deck_pos++];
					}

					for (int i = 0; i < 6; i++) {	// put new card data into string to send to client
						tempCard = card_buf[i];
						input[buf_index++] = static_cast<char>(tempCard.color + '0');
						input[buf_index++] = static_cast<char>(tempCard.shape + '0');
						input[buf_index++] = static_cast<char>(tempCard.number + '0');
						input[buf_index++] = static_cast<char>(tempCard.shading + '0');
					}
					
					current_deck_pos = temp_pos;
					for (int i = 0; i < 6; i++) {  // put 6 cards from board back into deck
						deck[current_deck_pos++] = board[i];
					}
					current_deck_pos = temp_pos;
	
					for (int i = 0; i < 6; i++) {  // put new cards on board from card buffer
						board[i] = card_buf[i];
					}
					shuffle(deck, current_deck_pos, 80); // shuffle cards left in deck
				}
				else { // there is no set on board and no cards left on deck to replace, game is over
					// send last score update to clients
					cout << "No more cards left in deck" << endl;

					for (int i = 0; i < num_players; i++) {
    		            send_data(players[i].Client_FD, input);
	        	    }

					cout << "A no-set occured with no cards left in deck, ending game..." << endl;

					sem_post(&end_game);  // unlocking end_game will allow server to close
				}
			}
			else {
				cout << "Guess was INCORRECT" << endl;
			}
			input[buf_index] = '\0';
			// send input to all clients
			for (int i = 0; i < num_players; i++) {
				send_data(players[i].Client_FD, input);
			}
			cout << "Number of cards left in deck: " << 80 - current_deck_pos << endl;
			cout << "Unlocking server_busy..." << endl;

			sem_post(&server_busy);
		}	// emd default
	} // end switch
	} // end for switch loop
}

void close_clients() {
	cout << "Closing all client fd's" << endl;

	for (int i = 0; i < num_players; i++) {
		close(players[i].Client_FD);
	}
}

void destroy_threads() {
	cout << "Destroying threads" << endl;
	for (int i = 0; i < 12; i++) {
		pthread_join(threads[i], nullptr);
	}
}

// Handles timer
void time_handler() {
	struct sigaction action = {};
	action.sa_handler = wakeup;
	sigaction(SIGALRM, &action, nullptr);
	// Set timer going
	cout << "Starting timer..." << endl;
	cout << "Duration: " << duration << endl;
	struct itimerval itimer = { {0, 0}, {duration, 0} };
	if (setitimer(ITIMER_REAL, &itimer, nullptr) == -1) fail("Couldn't start timer");

	pause(); // Wait until timer goes off in one second
	cout << "Time to connect is up!" << endl;
	accepting_clients = false; // No longer accept clients

	char temp[] = {'z', '\0'}; // z is game start
	// Send game start to all clients
	sem_wait(&connect_lock); // Don't start until all connections in are accepted
	for (int i = 0; i < num_players; i++) {
		if (write(players[i].Client_FD, &temp, sizeof temp) == -1) fail("Could not send start signal to client(s)");
	}
	
	cout << "Sent '" << temp << "' to all clients" << endl;
	clear_buffer(temp);

	sem_post(&connect_lock);
}

// Handles acceptance/rejection of cleints
void* pregame(void* arg) {
	// accept clients putting connecting and finding names and giving them their separate threads and checking to make sure accepting_clients is true

	while (true) {
	    sem_wait(&pregame_lock);    // make sure only one person can connect at a time
		// init fucntions access global data
	    char temp_name[NAME_LEN + 1];
		client = init_connection();
		cout << "Client attempted connection..." << endl;

		if (accepting_clients and (num_players < 12)) {
			cout << "Client accepted" << endl;
	    	
			clear_buffer(input); // make sure buffer is empty
		    if (read(client, &input, NAME_LEN+1) <= 0) fail("Failed reading from client");
		    // Check for name collision
		    if (name_exists(input)) {
    		    // Apend digits to name and check for collisions until accepted
    	    	char c_int[3];
		        clear_buffer(temp_name);
    		    cout << "Original size of name " << input << " is " << strlen(input) << endl;
       		 	for (int i = 1; i < 99; i++) {
	   	    	    clear_buffer(temp_name);
    	        	clear_buffer(c_int);

        		    // Put name into temp buffer
        	    	strncpy(temp_name, input, strlen(input));
	    	        cout << "temp_name is " << temp_name << " and i is " << i << endl;
    	   		    // Make digit a char
        		    sprintf(c_int, "%d", i);

	 	           // Put digit on end of name
    		        strncat(temp_name, c_int, strlen(c_int));
    	    	    cout << "the new temp_name is " << temp_name << endl;

        	    	// Check if the new name is accepted
	       		    if (name_exists(temp_name)) {
	      	          // continue
   		 	        }
    	    	    else {
    	        	    // Put name as the new name
	   		            cout << "Writing " << temp_name << " of size " << strlen(temp_name) << endl;
    	   		        write(client, &temp_name, strlen(temp_name));
        	   		    break;
           			}
		        } // End of append name loop
				
				init_player(temp_name, port, client); // Send changed name
   		 	}
	    	// Name as given was ok
		    else {
				// Write original name to client
   		     	write(client, &input, strlen(input));
	   		 	// init Player info
			    init_player(input, port, client); // Send original name
		    }

			cout << "Players: ";
	   		for (int i = 0; i < num_players; i++) {
		   		cout << players[i].Name << " ";
			}
			cout << endl;
	    	// clear input buffer for next entry
			clear_buffer(input); // declared in network.h

			int temp = num_players - 1;
			if (pthread_create(&threads[thread_count++], nullptr, handle_player, &temp) == -1) fail("Could not init thread");
			cout << "Player has been sent to thread" << endl;
		}
		else {
			read(client, &input, NAME_LEN+1);
			char response[] = "3\0";
			write(client, &response, strlen(response));

			cout << "Rejected client...sent '" << response << "' back" << endl;
		}
		// free up for next user to connect
	    
		sem_post(&pregame_lock);

		// Run thread to handle client
		// DOES THIS NEED TO BE MOVED INSIDE 'IF ACCEPTING CLIENTS' STATEMENT??????????
		//int temp = num_players - 1;
    	//if (pthread_create(&threads[thread_count++], nullptr, handle_player, &temp) == -1) fail("Could not init thread");
		//cout << "Player has been sent to thread..." << endl;
	} // end of infinite loop

	// Send board and players
	pthread_exit(nullptr);
}

int main(int argc, char* argv[]) {
	// If there are too many inputs	
	if (argc > 3 or argc < 2) fail("Given wrong number of inputs");
	// If all the inputs are given
	else if (argc == 3) {
		duration = atoi(argv[2]);
	}
	//given only the port
	else {
		duration = 15;
	}
	// Set port number
	port = atoi(argv[1]);
	cout << "Initial duration: " << duration << endl;
	// Initialize any semaphores needed
	if (sem_init(&connect_lock, 0, 1) == -1 or
		sem_init(&pregame_lock, 0, 1) == -1 or
		sem_init(&server_busy, 0, 1) == -1 or
		sem_init(&player_update, 0, 1) == -1 or
		sem_init(&check_cards, 0, 1) == -1 or
		sem_init(&begin_game, 0, 0) == -1 or
		sem_init(&end_game, 0, 0) == -1) fail("Could not init semaphore");

	//	CONNECT CLIENTS
	// Create a server with a socket as a file descriptor
	listening_server = socket(AF_INET, SOCK_STREAM, 0);
	if (listening_server == -1) fail("Bad server init");

	// Create server address object
	struct sockaddr_in server_addr = {};
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port); // port number is input from user
	
	if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0)	// SERVER_IP = 127.0.0.1 from server.h
		fail("Failed converting IP address");

	if (bind(listening_server, reinterpret_cast<struct sockaddr*>(&server_addr), sizeof server_addr) == -1)
		fail("Cannot bind server socket");

	// initiates 'soft' listening on the file descriptor, making sure it will work
	if (listen(listening_server, SOMAXCONN) == -1)
		fail("Cannot listen on server");

	// 	
	// Accept in clients waiting only argv[1] time for a connection before quiting
	// This function will accept up to 12 clients but makes the clients then wait
	// until the game is ready at which point accept_clients will return back

	init_deck(deck);
	shuffle(deck, 0, 80);
	shuffle(deck, 0, 80); // twice for good measure
	init_board();

	//		START THREADS
	//if (pthread_create(&game_timer_thread, nullptr, time_handler, nullptr) == -1) fail("could not initialize game_timer_thread");

	if (pthread_create(&start_thread, nullptr, pregame, nullptr) == -1) fail("Could not initialize start_thread");
	
	time_handler();

	// send inital board data to all players
	//sem_wait(&begin_game);
	//send_board();
	//send_players();
	
	for (int i = 0; i < num_players; i++) {
		sem_post(&begin_game);
	}

	//cout << "Sent board and player names...Game has begun" << endl;

	// wait for thread to let main know game is over
	sem_wait(&end_game);

	//	CLOSING
	close_clients();
	destroy_threads();
	// Close clients
	sem_destroy(&check_cards);
	sem_destroy(&connect_lock);
	sem_destroy(&player_update);
	sem_destroy(&server_busy);
	sem_destroy(&begin_game);
	sem_destroy(&end_game);
}
