/*
Purpose: networking functions and constructs
abstraction for opening connections and dealing with clients
*/

#include <iostream>
#include <cstring>
#include <string>
#include <cstdlib>
#include <unistd.h>
#include <sys/types.h>
#ifndef NAME_LEN
#define NAME_LEN 16
#endif

using namespace std;

struct Player {
	char Name[NAME_LEN+1];
	int Port;
	int Score;
	int Client_FD;
	int Server_FD;
};

typedef Player PLAYER[12];

void clear_buffer(char buf[]) {
	for (unsigned int i = 0; i < sizeof buf; i++) {
		buf[i] = '\0';
	}
}

//	ACCEPT NAME


//	SENDING PLAYER DATA


//	SENDING BOARD DATA


//	RECIEVE PLAYER CARDS


//	SEND END SCORES
