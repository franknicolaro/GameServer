CS352 Team Project
By:Frank Nicolaro, Joshua Walker, Kyle Rusinko
README.txt file

How to compile:
   For the client:
   gcc -o client GameClient.c client-thread-2021.c

   For the server:
   gcc -pthread -o gameServer GameServer.c server-thread-2021.c

How to run:
   For the server:
   ./gameServer port# testFileName

   For the client:
   ./client HostServer(on which the server was run) port# CurrentServer(on which the client is on)

How to play:
   The first player that connects will be assigned player 1 and will
   determine what game the two players play after the second player
   connects. When the game is chosen, a thread will run that game's method.
   Regardless of the game, the first player that connected will be
   the first to make a move. Player 1 should input two integers
   separated by a space if the game is Tic Tac Toe or just one
   integer if the game is Connect Four. If a valid move, then player 2 will
   get to make a move, and the game continues from there.

How it works:
   The game board is set up as an array of chars. Each location
   has two digits that correspond to each which the player will
   use to input their desired place on the board.
   Game board locations:
               |-----|-----|-----|
               | 0 0 | 0 1 | 0 2 |
               |-----|-----|-----|
               | 1 0 | 1 1 | 1 2 |
               |-----|-----|-----|
               | 2 0 | 2 1 | 2 2 |
               |-----|-----|-----|

   While playing, 'X' denotes player 1 on the board and 'O' denotes player 2
   on the board. The game will be played until one of the following
   conditions is met.
   
          - If the game board is full
               |-----|-----|-----|
               |  X  |  O  |  X  |
               |-----|-----|-----|
               |  X  |  O  |  O  |
               |-----|-----|-----|
               |  O  |  X  |  O  |
               |-----|-----|-----|

          - If P1 or P2 make a horizontal line
               |-----|-----|-----|
               |  X  |  X  |  X  |
               |-----|-----|-----|
               |     |     |     |
               |-----|-----|-----|
               |     |     |     |
               |-----|-----|-----|

          - If P1 or P2 make a vertical line
               |-----|-----|-----|
               |  X  |     |     |
               |-----|-----|-----|
               |  X  |     |     |
               |-----|-----|-----|
               |  X  |     |     |
               |-----|-----|-----|

          - If P1 or P2 make a diagonal line
               |-----|-----|-----|
               |  X  |     |     |
               |-----|-----|-----|
               |     |  X  |     |
               |-----|-----|-----|
               |     |     |  X  |
               |-----|-----|-----|

   NOTE: For Connect Four, the logic for checking lines and game over
   is different as well as the placing of a character in the array,
   however the game functions very similarly to TicTacToe: P1 makes a move,
   the game checks for a line or if the board is full, then P2 moves and does
   the same. The only other difference is that if P2 moves, the board must check
   if it is full, since P2's move can determine when the board is full.

   PlayerRecord is a defined structure that helps keep track of the record
   (W/L/T) of players from game to game. When the game starts a player is
   asked for their name. If the name is a new name, it is placed in the
   scoreboard variable at the first open index. This is that player's ID
   for as long as the server runs, and their record and name is tied to that
   ID. When a game is finished, the record of each player will update
   accordingly and the server will close their connections.
   
   Another defined structure is GameContext. This is used to keep track of
   the information about each player important to sending and receiving
   between the server and client (the socketfd of both players as well as
   their playerIDs). It also contains the PlayerRecord, which is used when
   the game ends. This is how the server is also able to close the connection
   between server and client.

   The file system is the newest addition (other than the option of game)
   and, on starting the server, the scoreboard reads in a record
   of each player one at a time from 0 <= i < 10 (though this can be changed to have
   as many users desired). After reading in the file and PlayerRecords, the main() of
   server will start a thread called updateFile(), which waits for 5 minutes for the 
   scoreboard to update. When the 5 minutes have passed, the PlayerRecords will be written
   back into the file. This continues until the server terminates.
