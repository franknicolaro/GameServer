/* Program:     Tic Tac Toe Game Client
** Authors:     Group 2 - Joshua Walker, Frank Nicolaro, Kyle Rusinko
** Date:        13 December 2021
** File name:   GameClient.c
** Compile:     gcc -o client GameClient.c client-thread-2021.c
** Run:         ./client HOST HTTPPORT webpage
** Function:    The purpose of this program is to interface with a server utilizing
**              a port and sockets. The client will open a socket and send the information
**              necessary to play a two-player game of tic tac toe. Two client programs
**              are required to properly execute the game. Once the game has concluded,
**              the program will close the socket. This program utilizes helper programs
**              provided in the git repo.
** Pre-req:     README.txt
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "client-thread-2021.h"

#define BUFFERSIZE 256

void make_move(int http_conn, int gMode);
void send_move(int http_conn, int rowNum, int colNum, int gMode);
void get_game_board(int http_conn, char board[], int gMode);
void print_game_board(char board[], int gMode);
void print_player_names(char current[], char opponent[]);
void get_move_status(int http_conn, int gMode);
void get_gameover(int http_conn);
void get_player_names(int http_conn);
void send_username(int http_conn);
void select_gamemode(int http_conn);
void get_scoreboard(int http_conn);
int get_game_status(int http_conn);
int get_wait_status(int http_conn);

int main(int argc, char *argv[]) {
   int web_server_socket;
   int game_finished = 0;
   int nBytes = 0;
   int read_count = 0;
   int wait_status = 0;
   int pNum;
   int gamemode = 0;                                              
   char buffer[BUFFERSIZE];
   char game_board_TTT[9];
   char game_board_CF[42];

   // determine if the number of paramters to connect to server is met
   if(argc != 4) {
      printf("usage: client HOST HTTPORT webpage\n");
      exit(1);
   }
   // connect to the server
   if((web_server_socket = get_server_connection(argv[1], argv[2])) == -1) {
      printf("connection error\n");
      exit(1);
   }
   // receive the player number for the current player
   recv(web_server_socket, &pNum, sizeof(int), 0);

   // if the current player is player 1, make them select the gamemode
   if(pNum == 1) {
      select_gamemode(web_server_socket);
   }
   recv(web_server_socket, &gamemode, sizeof(int), 0);

   send_username(web_server_socket);
   get_player_names(web_server_socket);

   // while the game is not finished, continue playing
   while(game_finished != 1) {

      wait_status = get_wait_status(web_server_socket);

      // if the player is not waiting, let them make their move
      if(wait_status == 0) {
         printf("It is your turn \n");
         make_move(web_server_socket, gamemode);
         get_move_status(web_server_socket, gamemode);
      }
      // receive the game board and status determining if the game has ended
      // dependent on which current game is being played
      if(gamemode == 1) {
         get_game_board(web_server_socket, game_board_TTT, gamemode);
      }
      else {
         get_game_board(web_server_socket, game_board_CF, gamemode);
      }
      game_finished = get_game_status(web_server_socket);
   }
   // receive and print how the game has ended, then close the socket
   get_gameover(web_server_socket);
   get_scoreboard(web_server_socket);
   close(web_server_socket);
}

void send_username(int http_conn) {
   char name[BUFFERSIZE];
   int status;

   // scan for entered username
   printf("Please enter your name \n");
   scanf("%s", &name);

   int nBytes = strlen(name) + 1;
   recv(http_conn, &status, sizeof(int), 0);

   // send length of username and the username itself
   send(http_conn, &nBytes, sizeof(int), 0);
   send(http_conn, name, nBytes, 0);
}

void get_player_names(int http_conn) {
   int numbytes, nBytes = 0;
   char current[BUFFERSIZE];
   char opponent[BUFFERSIZE];

   // receive the name of the current player
   recv(http_conn, &nBytes, sizeof(int), 0);
   numbytes = recv(http_conn, current, nBytes,  0);
   current[numbytes] = '\0';

   // receive the name of the opposing player
   recv(http_conn, &nBytes, sizeof(int), 0);
   numbytes = recv(http_conn, opponent, nBytes,  0);
   opponent[numbytes] = '\0';

   print_player_names(current, opponent);
}

void print_player_names(char current[], char opponent[]) {
   // print the current player's name and their opponent
   printf("Your name: %s, ", current);
   printf("Opponent name: %s \n", opponent);
}

void select_gamemode(int http_conn) {
   int gamemode = 0;
   // prompt for which gamemode player 1 wants
   printf("Select which game:\n");
   printf("1 - Tic Tac Toe \n");
   printf("2 - Connect Four \n");

   scanf("%d", &gamemode);
   // send the gamemode to the server
   send(http_conn, &gamemode, sizeof(int), 0);
}

void make_move(int http_conn, int gMode) {
   int row, column;

   if(gMode == 1) {
      // scan for row and column input from current player
      printf("Enter a row and column number: \n");
      scanf("%d%d", &row, &column);

      // send the position chosen to the server
      send_move(http_conn, row, column, gMode);
   }
   else {
      printf("Enter a column number: \n");
      scanf("%d", &column);

      // send the position chosen to the server
      send_move(http_conn, 0, column, gMode);
   }

}

void send_move(int http_conn, int rowNum, int colNum, int gMode) {
   if(gMode == 1) {
      // send the row and column chosen for the player's move (if TTT)
      send(http_conn, &rowNum, sizeof(int), 0);
      send(http_conn, &colNum, sizeof(int), 0);
   }
   else {
      // else send the column selected for connect four
      send(http_conn, &colNum, sizeof(int), 0);
   }
}

void get_move_status(int http_conn, int gMode) {
   int status;

   // recieve the status of the move made by the current player
   recv(http_conn, &status, sizeof(int), 0);

   // if the previous move made was not valid (the move has already
   // been made by a player), make the player chose another move
   if(status == 0) {
      printf("The position chosen has already been used, please pick another \n");
      make_move(http_conn, gMode);
      get_move_status(http_conn, gMode);
   }
}

int get_wait_status(int http_conn) {
   int status;

   // receive the status if the player is waiting
   recv(http_conn, &status, sizeof(int), 0);

   return status;
}

int get_game_status(int http_conn) {
   int status;

   // receive the status of the game (whether it has ended or not)
   recv(http_conn, &status, sizeof(int), 0);

   return status;
}

void get_game_board(int http_conn, char board[], int gMode) {
   int nBytes = 0;

   // receive the current game board from the server
   recv(http_conn, &nBytes, sizeof(int), 0);
   recv(http_conn, board, nBytes, 0);

   print_game_board(board, gMode);
}

void get_gameover(int http_conn) {
   int status;

   // receive the status of how the game ended
   recv(http_conn, &status, sizeof(int), 0);

   if(status == 0) {
      printf("The game has ended in a draw \n");
   }
   else if(status == 1) {
      printf("The game has concluded, player 1 wins \n");
   }
   else if(status == 2) {
      printf("The game has concluded, player 2 wins \n");
   }
}

void get_scoreboard(int http_conn) {
   int numbytes, nBytes = 0;
   int wins, losses, ties = 0;
   char current[BUFFERSIZE];
   char opponent[BUFFERSIZE];

   // receive the name of the current player
   recv(http_conn, &nBytes, sizeof(int), 0);
   numbytes = recv(http_conn, current, nBytes,  0);
   current[numbytes] = '\0';

   // receive current player's wins, losses, and ties
   recv(http_conn, &wins, sizeof(int), 0);
   recv(http_conn, &losses, sizeof(int), 0);
   recv(http_conn, &ties, sizeof(int), 0);

   printf("%s: %dW/%dL/%dT - ", current, wins, losses, ties);

   // receive the name of the opposing player
   recv(http_conn, &nBytes, sizeof(int), 0);
   numbytes = recv(http_conn, opponent, nBytes,  0);
   opponent[numbytes] = '\0';

   // receive opposing player's wins, losses, and ties
   recv(http_conn, &wins, sizeof(int), 0);
   recv(http_conn, &losses, sizeof(int), 0);
   recv(http_conn, &ties, sizeof(int), 0);

   printf("%s: %dW/%dL/%dT \n", opponent, wins, losses, ties);
}

void print_game_board(char board[], int gMode) {
   int j = 0;

   if(gMode == 1) {
      // print the game board with boarders surrounding it
      for(int i=0; i<3; i++) {
         printf("%s", "------------------- \n");
         printf("%s", "|");
         for(j=0; j<3; j++) {
            if(board[(i*3)+j] == '-') {
               printf("%s", "     |");
            }
            else if(board[(i*3)+j] == 'x') {
               printf("%s", "  X  |");
            }
            else if(board[(i*3)+j] == 'o') {
               printf("%s", "  O  |");
            }
         }
         printf("\n");
      }
      printf("%s", "------------------- \n");
   }
   // else if the gamemode is connect four
   else {
      printf("%s", "  0   1   2   3   4   5   6  \n");
      for(int i=0; i<6; i++) {
         printf("%s", "----------------------------- \n");
         printf("%s", "|");
         for(j=0; j<7; j++) {
            if(board[(i*7)+j] == '-') {
               printf("%s", "   |");
            }
            else if(board[(i*7)+j] == 'x') {
               printf("%s", " X |");
            }
            else if(board[(i*7)+j] == 'o') {
               printf("%s", " O |");
            }
         }
         printf("\n");
      }
      printf("%s", "----------------------------- \n");
   }
}
