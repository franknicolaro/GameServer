/* Program:      Tic Tac Toe Game Server
** Authors:      Group 2 - Joshua Walker, Frank Nicolaro, Kyle Rusinko
** Date:         13 December 2021
** File Name:    GameServer.c
** Compile:      gcc -pthread -o gameServer GameServer.c server-thread-2021.c
** Run:          ./gameServer port# (filename.txt)
**
** Function:     This program will run a server in which multiple games containing two clients are run.
**               The server plays the game with the same rules as in the last assignment, the difference
**               being that everything to do with the player such as their player ID and Socket FD
**               is stored within a defined structure named GameContext. Also included in GameContext is PlayerRecord, another
**               defined structure in the program which keeps track of the player's name (entered upon starting their first
**               game) and their record (wins/losses/ties). Upon a game ending, the record of both players are
**               updated accordingly based on who won. This record then gets sent to both clients by the server, which
**               then closes the connections of the two players that finished.
**
** Note:         Utilizes the helper programs (server-thread-2021.c and server-thread-2021.h for
**               GameServer.c, client-thread-2021.c and client-thread-2021.h for
**               GameClient.c) to set up the connections between the server and clients
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <netdb.h>
#include <pthread.h>
#include <assert.h>
#include "server-thread-2021.h"

#define HOST "freebsd1.cs.scranton.edu"
#define BACKLOG 10
#define BUFFERSIZE 256

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

typedef struct PLAYERRECORD{
   char name[21];
   int wins;
   int losses;
   int ties;
}PlayerRecord;

typedef struct GAMECONTEXT{
   int playerXID;                                                 
   int playerOID;
   int playerXSockfd;
   int playerOSockfd;
   int fd;
   PlayerRecord *scoreboard;
}GameContext;


void *startGameTTT(void *ptr);
int gameResults(char board[]);
void sendStatus(int whoWon, int player_socket);
int handleMove(int player_socket, int playerNum, char board[]);
int isBoardFull(char board[]);
int checkLine(char board[], char playerChar);
int checkHorizontal(char board[], char playerChar);
int checkVertical(char board[], char playerChar);
int checkDiagonal(char board[], char playerChar);
void sendBoard(int player_socket, char board[], int playerNum);
void set(char board[], int i, int j, char value);
int isMoveValid(char board[], int i, int j);
void printBoard(char board[]);
void resetBoard(char board[]);
int findPlayerID(PlayerRecord *scoreboard, char pName[]);
void requestName(int playerSocket, int playerNum, GameContext *gc);
void initializeScoreBoard(PlayerRecord *scoreboard, int fd);
void sendNames(GameContext *gc);
void gameEnd(GameContext *gc, char board[]);
void updateRecord(GameContext *gc, int whoWon);
void sendRecords(GameContext *gc);
void sendPlayerRecord(GameContext *gc, int playerSocket, int pID, int oID);
int findOpenID(PlayerRecord *scoreboard);
void sendNameToPlayer(char pName[], char oName[], int playerSocket);
void *updateFile(void *gc);
PlayerRecord *readRecordAt(int fd, int index);
int writeRecordAt(int fd, PlayerRecord *record, int index);

//Methods added in for connect 4
void *startGameCF(void *ptr);
int handleGamemode(int player_socket1, int player_socket2);
void gameEndCF(GameContext *gc, char board[]);
void resetBoardCF(char board[]);
void printBoardCF(char board[]);
int isBoardFullCF(char board[]);
int gameResultsCF(char board[]);
int checkLineCF(char board[], char playerChar);
int handleMoveCF(int player_socket, int playerNum, char board[]);
int isMoveValidCF(char board[], int column, int playerNum);
int findOpenRowCF(char board[], int pos);
int checkMoveCF(char board[], int pos);
int checkHorizontalCF(char board[], char move);
int checkVerticalCF(char board[], char move);
int checkDiagonalCF(char board[], char move);
void setCF(char board[], int pos, char value);

int main(int argc, char *argv[]) {
   int server_socket;       //server socket
   int player_one_socket;   //socket for player one
   int player_two_socket;   //socket for player two
   int gameMode;            //gamemode selected by Player 1
   int fd = -1;
   PlayerRecord *scoreboard = (PlayerRecord*) malloc(sizeof(PlayerRecord) * 10);
   pthread_t thread, fileUpdater;

   if(argc != 3) {
      printf("Needs to be of format: progName Port# fileName\n");
      return 1;
   }
   //file opening and reading
   fd = open(argv[2], O_CREAT|O_RDWR, S_IRUSR|S_IWUSR);
   initializeScoreBoard(scoreboard, fd);

   //Starting server
   server_socket = start_server(HOST, argv[1], BACKLOG);

   //Initializing and starting updateFile() thread.
   GameContext *game = (GameContext *) malloc(sizeof(GameContext));
   game->scoreboard = scoreboard;
   game->fd = fd;
   pthread_create(&fileUpdater, NULL, updateFile, (void *)game);

   if(server_socket == -1) {
      printf("Server had error whilst starting\n");
      exit(1);
   }

   while(1) {
      //Accept both clients
      if((player_one_socket = accept_client(server_socket)) == -1) {
         continue;
      }
      if((player_two_socket = accept_client(server_socket)) == -1) {
         continue;
      }
      GameContext *game = (GameContext *) malloc(sizeof(GameContext));
      game->scoreboard = scoreboard;
      game->playerXSockfd = player_one_socket;
      game->playerOSockfd = player_two_socket;
      //Send the player numbers to each respective player
      sendStatus(1, game->playerXSockfd);
      sendStatus(2, game->playerOSockfd);
      //Get the gamemode selcted by player 1
      gameMode = handleGamemode(player_one_socket, player_two_socket);
      if(gameMode == 1) {
         pthread_create(&thread, NULL, startGameTTT, (void *)game);
      }
      else {
         pthread_create(&thread, NULL, startGameCF, (void *)game);
      }
   }
}

//A method which starts the game. After asking for both names, the game flows in logic like this:
//   1. Send a status to the players on who needs to wait their turn.
//   2. For the player that isn't waiting, make a move
//      a. If the player makes an invalid move, it remains at this
//      point until a valid move is made.
//      b. When a valid move is made, moves on to 3.
//   3. Update waiting variables and sends the
//   board to both players.
//   4. Determines if the game is over and sends that status to both players
//   5. if the game isn't over, sends the wait status again and
//   repeats steps 2 - 4 for the second player.
//

void *startGameTTT(void *ptr){
   int gameOver = 0;     //Variable to determine if the game is over
   int pOneMoved = 0;    //Variable to check if player one has moved
   int pTwoMoved = 0;    //Variable to check if player two has moved
   char board[] = {'-', '-', '-', '-', '-', '-', '-', '-', '-'};
   GameContext *gc = (GameContext *) ptr;
   requestName(gc->playerXSockfd, 1, gc);
   requestName(gc->playerOSockfd, 2, gc);
   sendNames(gc);

   while(gameOver == 0){
      pOneMoved = 0;
      pTwoMoved = 0;
      sendStatus(0, gc->playerXSockfd);
      sendStatus(1, gc->playerOSockfd);
      pOneMoved = handleMove(gc->playerXSockfd, 1, board);
      while(pOneMoved != 1){
         sendStatus(pOneMoved, gc->playerXSockfd);
         pOneMoved = handleMove(gc->playerXSockfd, 1, board);
      }
      sendStatus(pOneMoved, gc->playerXSockfd);
      sendBoard(gc->playerXSockfd, board, 1);
      sendBoard(gc->playerOSockfd, board, 2);
      gameOver = checkLine(board, 'x');
      if(gameOver != 1){
         gameOver = isBoardFull(board);
      }
      sendStatus(gameOver, gc->playerXSockfd);
      sendStatus(gameOver, gc->playerOSockfd);
      if(gameOver != 1){
         sendStatus(1, gc->playerXSockfd);
         sendStatus(0, gc->playerOSockfd);
         pTwoMoved = handleMove(gc->playerOSockfd, 2, board);
         while(pTwoMoved != 1){
            sendStatus(pTwoMoved, gc->playerOSockfd);
            pTwoMoved = handleMove(gc->playerOSockfd, 2, board);
         }
         sendStatus(pTwoMoved, gc->playerOSockfd);
         sendBoard(gc->playerXSockfd, board, 1);
         sendBoard(gc->playerOSockfd, board, 2);
         gameOver = checkLine(board, 'o');
         sendStatus(gameOver, gc->playerXSockfd);
         sendStatus(gameOver, gc->playerOSockfd);
      }
   }
   gameEnd(gc, board);
}

//A method which starts the game. After asking for both names, the game flows in logic like this:
//   1. Send a status to the players on who needs to wait their turn.
//   2. For the player that isn't waiting, make a move
//      a. If the player makes an invalid move, it remains at this
//      point until a valid move is made.
//      b. When a valid move is made, moves on to 3.
//   3. Update waiting variables and sends the
//   board to both players.
//   4. Determines if the game is over and sends that status to both players
//   5. if the game isn't over, sends the wait status again and
//   repeats steps 2 - 4 for the second player.
//
void *startGameCF(void *ptr){
   int gameOver = 0;     //Variable to determine if the game is over
   int pOneMoved = 0;    //Variable to check if player one has moved
   int pTwoMoved = 0;    //Variable to check if player two has moved
   char board[42];
   for(int i=0; i<42; i++) {
      board[i] = '-';
   }
   GameContext *gc = (GameContext *) ptr;
   requestName(gc->playerXSockfd, 1, gc);
   requestName(gc->playerOSockfd, 2, gc);
   sendNames(gc);

   while(gameOver == 0){
      pOneMoved = 0;
      pTwoMoved = 0;
      sendStatus(0, gc->playerXSockfd);
      sendStatus(1, gc->playerOSockfd);
      pOneMoved = handleMoveCF(gc->playerXSockfd, 1, board);
      while(pOneMoved != 1){
         sendStatus(pOneMoved, gc->playerXSockfd);
         pOneMoved = handleMoveCF(gc->playerXSockfd, 1, board);
      }
      sendStatus(pOneMoved, gc->playerXSockfd);
      sendBoard(gc->playerXSockfd, board, 1);
      sendBoard(gc->playerOSockfd, board, 2);
      gameOver = checkLineCF(board, 'x');
      if(gameOver != 1){
         gameOver = isBoardFullCF(board);
      }
      sendStatus(gameOver, gc->playerXSockfd);
      sendStatus(gameOver, gc->playerOSockfd);
      if(gameOver != 1){
         sendStatus(1, gc->playerXSockfd);
         sendStatus(0, gc->playerOSockfd);
         pTwoMoved = handleMoveCF(gc->playerOSockfd, 2, board);
         while(pTwoMoved != 1){
            sendStatus(pTwoMoved, gc->playerOSockfd);
            pTwoMoved = handleMoveCF(gc->playerOSockfd, 2, board);
         }
         sendStatus(pTwoMoved, gc->playerOSockfd);
         sendBoard(gc->playerXSockfd, board, 1);
         sendBoard(gc->playerOSockfd, board, 2);
         gameOver = checkLineCF(board, 'o');
         if(gameOver != 1){
            gameOver = isBoardFullCF(board);
         }
         sendStatus(gameOver, gc->playerXSockfd);
         sendStatus(gameOver, gc->playerOSockfd);
      }
   }
   gameEndCF(gc, board);
}

//Receives the gamemode selected by player 1 and sends the gamemode selected
//to each player
int handleGamemode(int player_socket1, int player_socket2) {
   int gMode;
   recv(player_socket1, &gMode, sizeof(int), 0);

   sendStatus(gMode, player_socket1);
   sendStatus(gMode, player_socket2);
   return gMode;
}

//Determines the end of the game using the given board and sends
//That result (whoWon) to both players. Afterwards, it updates the record
//of both players and closes their respective sockets
void gameEnd(GameContext *gc, char board[]){
   int whoWon = gameResults(board);

   printf("%d", whoWon);
   sendStatus(whoWon, gc->playerXSockfd);
   sendStatus(whoWon, gc->playerOSockfd);

   updateRecord(gc, whoWon);

   //Closes connections to players
   close(gc->playerXSockfd);
   close(gc->playerOSockfd);
}

//Determines the end of the game using the given board and sends
//That result (whoWon) to both players. Afterwards, it updates the record
//of both players and closes their respective sockets
void gameEndCF(GameContext *gc, char board[]){
   int whoWon = gameResultsCF(board);

   printf("%d", whoWon);
   sendStatus(whoWon, gc->playerXSockfd);
   sendStatus(whoWon, gc->playerOSockfd);

   updateRecord(gc, whoWon);

   //Closes connections to players
   close(gc->playerXSockfd);
   close(gc->playerOSockfd);
}

//Updates the record for both players and sends the updated records
//to both players
void updateRecord(GameContext *gc, int whoWon){
   pthread_mutex_lock(&lock);
   if(whoWon == 0){
      gc->scoreboard[gc->playerXID].ties = gc->scoreboard[gc->playerXID].ties + 1;
      gc->scoreboard[gc->playerOID].ties = gc->scoreboard[gc->playerOID].ties + 1;
   }
   else if(whoWon == 1){
      gc->scoreboard[gc->playerXID].wins = gc->scoreboard[gc->playerXID].wins + 1;
      gc->scoreboard[gc->playerOID].losses = gc->scoreboard[gc->playerOID].losses + 1;
   }
   else if(whoWon == 2){
      gc->scoreboard[gc->playerOID].wins = gc->scoreboard[gc->playerOID].wins + 1;
      gc->scoreboard[gc->playerXID].losses = gc->scoreboard[gc->playerXID].losses + 1;
   }
   pthread_mutex_unlock(&lock);
   sendRecords(gc);
}

//Sends records to both players, starting with whichever player was
//represented by 'X'
void sendRecords(GameContext *gc){
   sendPlayerRecord(gc, gc->playerXSockfd, gc->playerXID, gc->playerOID);
   sendPlayerRecord(gc, gc->playerOSockfd, gc->playerOID, gc->playerXID);
}

//A helper method to sendRecords() that sends both player names and their
//records to the specified player socket with the IDs of both players.
//It sends in this order:
//   1: Send player name
//   2: Send player stats
//   3: Send opponent name
//   4: Send opponent stats
void sendPlayerRecord(GameContext *gc, int playerSocket, int pID, int oID){
   int nBytes;
   char pName[BUFFERSIZE];
   char oName[BUFFERSIZE];
   int wins, losses, ties;

   strcpy(pName, gc->scoreboard[pID].name);
   strcpy(oName, gc->scoreboard[oID].name);

   nBytes = strlen(pName) + 1;

   //Send player name
   send(playerSocket, &nBytes, sizeof(int), 0);
   send(playerSocket, pName, nBytes, 0);

   wins = gc->scoreboard[pID].wins;
   losses =  gc->scoreboard[pID].losses;
   ties =  gc->scoreboard[pID].ties;

   //Send stats of player
   send(playerSocket, &wins, sizeof(int), 0);
   send(playerSocket, &losses, sizeof(int), 0);
   send(playerSocket, &ties, sizeof(int), 0);

   nBytes = strlen(oName) + 1;

   //Send opponent name
   send(playerSocket, &nBytes, sizeof(int), 0);
   send(playerSocket, oName, nBytes, 0);

   wins = gc->scoreboard[oID].wins;
   losses =  gc->scoreboard[oID].losses;
   ties =  gc->scoreboard[oID].ties;

   //Send stats of opponent
   send(playerSocket, &wins, sizeof(int), 0);
   send(playerSocket, &losses, sizeof(int), 0);
   send(playerSocket, &ties, sizeof(int), 0);
}

//Requests the name of the player at the specified socket.
void requestName(int playerSocket, int playerNum, GameContext *gc){
   int nBytes = 0;
   int read_count = 0;
   char name[BUFFERSIZE];
   int newID = 0;
   sendStatus(1, playerSocket);
   read_count = recv(playerSocket, &nBytes, sizeof(int), 0);
   read_count = recv(playerSocket, name, nBytes, 0);
   if(playerNum == 1){
      if(findPlayerID(gc->scoreboard, name) == -1){
         newID = findOpenID(gc->scoreboard);
         strcpy(gc->scoreboard[newID].name, name);
         gc->playerXID = newID;
      }
      else{
         gc->playerXID = findPlayerID(gc->scoreboard, name);
      }
   }
   else if(playerNum == 2){
      if(findPlayerID(gc->scoreboard, name) == -1){
         newID = findOpenID(gc->scoreboard);
         strcpy(gc->scoreboard[newID].name, name);
         gc->playerOID = newID;
      }
      else{
         gc->playerOID = findPlayerID(gc->scoreboard, name);
      }
   }
}

//Finds the playerID specified by the given name.
int findPlayerID(PlayerRecord *scoreboard, char pName[]){
   int result = -1;
   for(int i = 0; i < 10; i++){
      if(strcmp(scoreboard[i].name, pName) == 0){
         result = i;
         break;
      }
   }
   return result;
}

//Finds the first open ID in the scoreboard.
int findOpenID(PlayerRecord *scoreboard){
   int result = -1;
   for(int i = 0; i < 10; i++){
      if(strcmp(scoreboard[i].name, "") == 0){
         result = i;
         break;
      }
   }
   return result;
}

//Sends the names back to both players for them to print out.
void sendNames(GameContext *gc){
   sendNameToPlayer(gc->scoreboard[gc->playerXID].name, gc->scoreboard[gc->playerOID].name, gc->playerXSockfd);
   sendNameToPlayer(gc->scoreboard[gc->playerOID].name, gc->scoreboard[gc->playerXID].name, gc->playerOSockfd);
}

//Sends the player's name and the opponent's name to the
//player specified at the socket.
void sendNameToPlayer(char pName[], char oName[], int playerSocket){
   int nBytes = strlen(pName) + 1;
   send(playerSocket, &nBytes, sizeof(int), 0);
   send(playerSocket, pName, nBytes, 0);
   nBytes = strlen(oName) + 1;
   send(playerSocket, &nBytes, sizeof(int), 0);
   send(playerSocket, oName, nBytes, 0);
}

//Initializes the names of the scoreboard.
//This is used to ensure an easy way to find an
//open playerID,
void initializeScoreBoard(PlayerRecord *scoreboard, int fd){
   PlayerRecord *tmp = (PlayerRecord *) malloc(sizeof(PlayerRecord));
   int i = 0;
   while((tmp = readRecordAt(fd, i)) != NULL){
      scoreboard[i] = *tmp;
      printf("Record of P%d obtained: %s, W%d, L%d, T%d\n", i++, scoreboard[i].name, scoreboard[i].wins, scoreboard[i].losses, scoreboard[i].ties);
      free(tmp);
   }
}

//Resets the board, setting every char in the array to '-'.
void resetBoard(char board[]){
   for(int i = 0; i < 3; i++){
      for(int j = 0; j < 3; j++){
         board[(i*3) + j] = '-';
      }
   }
}

//Resets the board, setting every char in the array to '-'.
void resetBoardCF(char board[]){
   for(int i = 0; i < 42; i++){
      board[i] = '-';
   }
}

//Prints the board in a less formatted way for the server.
void printBoard(char board[]) {
   int j = 0;

   // print the game board with boarders surrounding it
   for(int i=0; i<3; i++) {
      for(j=0; j<3; j++) {
         printf("%c   ", board[(i*3) + j]);
      }
      printf("\n");
   }
}

//Helper method which prints the connect 4 board. Used in testing.
void printBoardCF(char board[]) {
   int j = 0;

   // print the game board with boarders surrounding it
   for(int i=0; i<6; i++) {
      for(j=0; j<7; j++) {
         printf("%c   ", board[(i*7) + j]);
      }
      printf("\n");
   }
}

//Checks if the board is full. The board is full when
//all spots of the board are taken and does not contain
//a '-'. If a '-' is in the board, return 0. If not, return 1.
int isBoardFull(char board[]){
   int result = 1;
   for(int i = 0; i < 3; i++){
      for(int j = 0; j < 3; j++){
         if(board[(i*3)+j] == '-'){
            result = 0;
         }
      }
   }
   return result;
}

//Checks if the board is full. The board is full when
//all spots of the board are taken and does not contain
//a '-'. If a '-' is in the board, return 0. If not, return 1.
int isBoardFullCF(char board[]){
   int result = 1;
   for(int row = 0; row < 6; row++) {
      for(int col = 0; col < 7; col++) {
         if(board[(row * 7) + col] == '-') {
            return 0;
         }
      }
   }
   return 1;
}

//Gets the game results to be send to the players.
//The results are as follows:
//   If returns a 0: The game ends in a draw
//   If returns a 1: Player one wins.
//   If returns a 2: player two wins.
//This is done by checking if a line was made for
//both players (using checkLine()).
int gameResults(char board[]){
   int result = 0;
   if(checkLine(board,'x') == 1){
      result = 1;
   }
   else if(checkLine(board, 'o') == 1){
      result = 2;
   }
   return result;
}

//Gets the game results to be send to the players.
//The results are as follows:
//   If returns a 0: The game ends in a draw
//   If returns a 1: Player one wins.
//   If returns a 2: player two wins.
//This is done by checking if a line was made for
//both players (using checkLineCF()).
int gameResultsCF(char board[]){
   int result = 0;
   if(checkLineCF(board,'x') == 1){
      result = 1;
   }
   else if(checkLineCF(board, 'o') == 1){
      result = 2;
   }
   return result;
}

//Checks if the specified player has made a line.
//A player ('x' = Player 1, 'o' = Player 2)
//Can make a line horizontally, vertically, or
//diagonally. The function returns 0 if a line was not
//made by the specified player and 1 if a line was made.
int checkLine(char board[], char playerChar){
   int result = 0;
   if(checkHorizontal(board, playerChar) == 1){
      result = 1;
   }
   else if(checkVertical(board, playerChar) == 1){
      result = 1;
   }
   else if(checkDiagonal(board, playerChar) == 1){
      result = 1;
   }
   return result;
}

//Checks if the specified player has made a line in Connect 4.
//A player ('x' = Player 1, 'o' = Player 2)
//Can make a line horizontally, vertically, or
//diagonally. The function returns 0 if a line was not
//made by the specified player and 1 if a line was made.
int checkLineCF(char board[], char playerChar){
   int result = 0;
   if(checkHorizontalCF(board, playerChar) == 1){
      result = 1;
   }
   else if(checkVerticalCF(board, playerChar) == 1){
      result = 1;
   }
   else if(checkDiagonalCF(board, playerChar) == 1){
      result = 1;
   }
   return result;
}

//Checks if a horizontal line was made by the specified player.
//A horizontal line is defined by a line made in the board as such:
//   1. if board[0], board[1], and board[2] contain the same playerChar
//   2. if board[3], board[4], and board[5] contain the same playerChar
//   3. if board[6], board[7], and board[8] contain the same playerChar
//Returns 0 if the horizontal line was not made by the specified player
//And 1 if the a horizontal line was made.
int checkHorizontal(char board[], char playerChar){
   int result = 0;
   if(board[0] == playerChar && board[1] == playerChar && board[2] == playerChar){
      result = 1;
   }
   else if(board[3] == playerChar && board[4] == playerChar && board[5] == playerChar){
      result = 1;
   }
   else if(board[6] == playerChar && board[7] == playerChar && board[8] == playerChar){
      result = 1;
   }
   return result;
}

//Checks if a vertical line was made by the specified player.
//A vertical line is defined by a line made in the board as such:
//   1. if board[0], board[3], and board[6] contain the same playerChar
//   2. if board[1], board[4], and board[7] contain the same playerChar
//   3. if board[2], board[5], and board[8] contain the same playerChar
//Returns 0 if the vertical line was not made by the specified player
//And 1 if the a vertical line was made.
int checkVertical(char board[], char playerChar){
   int result = 0;
   if(board[0] == playerChar && board[3] == playerChar && board[6] == playerChar){
      result = 1;
   }
   else if(board[1] == playerChar && board[4] == playerChar && board[7] == playerChar){
      result = 1;
   }
   else if(board[2] == playerChar && board[5] == playerChar && board[8] == playerChar){
      result = 1;
   }
   return result;
}

//Checks if a diagonal line was made by the specified player.
//A diagonal line is defined by a line made in the board as such:
//   1. if board[0], board[4], and board[8] contain the same playerChar
//   2. if board[2], board[4], and board[6] contain the same playerChar
//Returns 0 if the diagonal line was not made by the specified player
//And 1 if the a diagonal line was made.
int checkDiagonal(char board[], char playerChar){
   int result = 0;
   if(board[0] == playerChar && board[4] == playerChar && board[8] == playerChar){
      result = 1;
   }
   else if(board[2] == playerChar && board[4] == playerChar && board[6] == playerChar){
      result = 1;
   }
   return result;
}

//Sends the board to the player at the specified socket.
void sendBoard(int player_socket, char board[], int playerNum){
   int nBytes = strlen(board) + 1;
   printf("Sending game board to player %d\n", playerNum);
   send(player_socket, &nBytes, sizeof(int), 0);
   send(player_socket, board, nBytes, 0);
}

//Sends the status to the player at the specified socket.
void sendStatus(int status, int player_socket){
   send(player_socket, &status, sizeof(int), 0);
}

//Handles the move made by the player at the specified socket.
//It does this by receiving the inputs as if the board were a 2D
//array. Afterwards, the method checks if the move was valid
//and if it is, sets the board at the specified space in the array
//to the char represented by the player that entered their move.
//Returns 0 if the move was not made and 1 if the move was made.
int handleMove(int player_socket, int playerNum, char board[]) {
   int moveMade = 0;
   printf("Player %d is making their move...\n", playerNum);
   int read_count = 0;
   int posPlayedX;
   int posPlayedY;
   read_count = recv(player_socket, &posPlayedX, sizeof(int), 0);
   read_count = recv(player_socket, &posPlayedY, sizeof(int), 0);
   if(isMoveValid(board, posPlayedX, posPlayedY) == 1){
      if(playerNum == 1){
         set(board, posPlayedX, posPlayedY, 'x');
      }
      else if(playerNum == 2){
         set(board, posPlayedX, posPlayedY, 'o');
      }
      moveMade = 1;
   }
   return moveMade;
}

//Handles the move made by the specified player for Connect 4.
//It first asks the player for a column that they would like to
//play. If the move is valid (using isMoveValidCF()), returns 1 and
//continues to the other player for input. If not, returns 0 and asks
//the same player to make another move.
int handleMoveCF(int player_socket, int playerNum, char board[]) {
   int moveMade = 0;
   printf("Player %d is making their move...\n", playerNum);
   int read_count = 0;
   int colPlayed;
   read_count = recv(player_socket, &colPlayed, sizeof(int), 0);

   if(isMoveValidCF(board, colPlayed, playerNum) == 1){
      moveMade = 1;
   }
   return moveMade;
}

//Checks if the move is valid. A move is valid if
//The board at the specified index of [(i*3) + j]
//contains a '-'. Returns 0 if the move is not valid and
//returns 1 if the move is valid.
int isMoveValid(char board[], int i, int j){
   int result = 0;
   if(board[(i*3)+j] == '-'){
      result = 1;
   }
   return result;
}

//Checks if the move is valid in Connect Four. A move
//is valid if the board at the specified index of [(row*7) + column]
//contains a '-'. Returns 0 if the move is not valid and
//returns 1 if the move is valid.
int isMoveValidCF(char board[], int column, int playerNum){
   int index;

   index = findOpenRowCF(board, column);
   if(!checkMoveCF(board, index)) {
      return 0;
   }
   else if(playerNum == 1){
      setCF(board, index, 'x');
   }
   else {
      setCF(board, index, 'o');
   }
   return 1;
}

//Checks for the first open row based on the given column
int findOpenRowCF(char board[], int pos){
   int found = -1;
   int row = 5;

   while(row >= 0 && found == -1) {
      if(board[(row * 7) + pos] == '-') {
         found = (row * 7) + pos;
      }
      row--;
   }
   return found;
}

//Checks if a move is possible using the position given
//from findOpenRowCF(). Returns 1 if it is possible.
int checkMoveCF(char board[], int pos) {
   if(pos < 42 && pos > -1) {
      if(board[pos] == '-') {
         return 1;
      }
   }
   return 0;
}

//Checks for a horizontal line in Connect 4.
//A horizontal line is possible in 4 ways on a given row:
//(Column numbers from 0-6)
//
//   0 1 2 3 4 5 6
//   X X X X
//     X X X X
//       X X X X
//         X X X X
//
//Returns 1 if a horizontal line is found, 0 if not.
int checkHorizontalCF(char board[], char move) {
   int loc = 0;
   int count = 0;

   for(int i=0; i<36; i=i+7) {
      loc = i;
      for(int j=0; j<5; j++) {
         count = 0;
         for(int k=0; k<4; k++) {
            if(board[loc] == move) {
               loc++;
               count++;
            }
            if(count == 4) {
               return 1;
            }
         }
         loc = i+j;
      }
   }
   return 0;
}

//Checks for a vertical line in Connect 4. There are 3
//possible ways to check for a vertical line in a given column:
//(Row numbers from 0-5)
//
//0      X
//1      X  X
//2      X  X  X
//3      X  X  X
//4         X  X
//5            X
//
//Returns 1 if a line is found, 0 if not.
int checkVerticalCF(char board[], char move) {
   int loc = 0;
   int count = 0;

   for(int i=0; i<7; i++) {
      loc = i;
      for(int j=0; j<3; j++) {
         count = 0;
         for(int k=0; k<4; k++) {
            if(board[loc] == move) {
               loc = loc + 7;
               count++;
            }
            if(count == 4) {
               return 1;
            }
         }
         loc = i + ((j + 1) * 7);
      }
   }
   return 0;
}

//Checks for a diagonal line in connect four.
//A line is Diagonal if, from the first spot checked,
//the subsequent three spots in a diagonal create a line.
//5x5 Ex (this same logic applies to the function):
//
//   X X _ X X
//   _ X X X _
//   _ X X X _
//   X X _ X X
//
//Returns 1 if a line is found, 0 if not.
int checkDiagonalCF(char board[], char move) {
   int loc = 0;
   int count = 0;

   for(int i=0; i<4; i++) {
      loc = i;
      for(int j=0; j<3; j++) {
         count = 0;
         for(int k=0; k<4; k++) {
            if(board[loc] == move) {
               loc = loc + 8;
               count++;
            }
            if(count == 4) {
               return 1;
            }
         }
         loc = loc + 7;
      }
   }

   for(int i=6; i>2; i--) {
      loc = i;
      for(int j=0; j<3; j++) {
         count = 0;
         for(int k=0; k<4; k++) {
            if(board[loc] == move) {
               loc = loc + 6;
               count++;
            }
            if(count == 4) {
               return 1;
            }
         }
         loc = loc + 7;
      }
   }
   return 0;
}

//Sets the board at the specified index of [(i*3) + j]
//to the specified char value. This method will only be used
//if a move is valid in this location.
void set(char board[], int i, int j, char value){
   board[(i*3)+j] = value;
}

void setCF(char board[], int pos, char value){
   board[pos] = value;
}

//Updates the file every 5 minutes by writing the
//records one at a time to the file.
void *updateFile(void *gc){
   GameContext *game = (GameContext *)gc;
   PlayerRecord *tmp = (PlayerRecord *) malloc(sizeof(PlayerRecord));
   while(1){
      sleep(30);
      for(int j = 0; j < 10; j++){
         *tmp = game->scoreboard[j];
         if(writeRecordAt(game->fd, tmp, j) < 0){
            printf("Write File Error\n");
            exit(1);
         }
      }
   }
}

//***Helper methods to read and write files properly***
//--------------------------------------------------------------------------
//A method that reads in the record at the
//specified file descriptor (fd) if it can be read in.
PlayerRecord *readRecordAt(int fd, int index){
   PlayerRecord *record = (PlayerRecord *) malloc(sizeof(PlayerRecord));
   if(lseek(fd, index * sizeof(PlayerRecord), SEEK_SET) < 0){
      return NULL;
   }
   if(read(fd, record, sizeof(PlayerRecord)) <= 0){
      return NULL;
   }
   return record;
}

//A method that writes the record in to the specified file by
//its file descriptor (fd).
int writeRecordAt(int fd, PlayerRecord *record, int index){
   if(lseek(fd, index * sizeof(PlayerRecord), SEEK_SET) < 0){
      return -1;
   }
   if(write(fd, record, sizeof(PlayerRecord)) <= 0){
      return -1;
   }
   return 0;
}
