
#ifndef GAME_H
#define GAME_H

#include <iostream>
#include <vector>
#include <string>

using namespace std;

class Game {

struct GamePlayer {
    string user_name = "";
    int player_time_limit = 0;
    int turn_start_time = -1;
    char choice = '\0';

    // Getters
    string getUserName() const { return user_name; }
    char getChoice() const { return choice; }
    // Setters
    void setUserName(const string& name) { user_name = name; }
    void setChoice(char ch) { choice = ch; }
    string printGamePlayer() {
        string result = "User Name: " + user_name + "\n";
        result += "Player Time Limit: " + to_string(player_time_limit) + "\n";
        result += "Turn Start Time: " + to_string(turn_start_time) + "\n";
        result += "Choice: ";
        if (choice != '\0') {
            result += choice;
        } else {
            result += "Not set";
        }
        result += "\n";
        return result;
    }
};

private:
    vector<vector<char>> gameBoard;
    GamePlayer player1;
    GamePlayer player2;
    GamePlayer* current_player = nullptr;
    int total_moves = 0;
    bool valid_game = false;
    int game_start_time = -1;
    vector<vector<char>> initializeGameBoard() {
        vector<vector<char>> gameBoard = {
            {' ', '1', '2', '3'},
            {'A', '.', '.', '.'},
            {'B', '.', '.', '.'},
            {'C', '.', '.', '.'}
        };
        return gameBoard;
    }


public:
    Game(string player1_user_name, string player2_user_name,
         int game_time_limit,
         char player1_choice)
        : gameBoard(initializeGameBoard()) {
        
        player1.user_name = player1_user_name;
        player1.player_time_limit = (int)game_time_limit/2;
        player1.choice = player1_choice;
        
        player2.user_name = player2_user_name;
        player2.player_time_limit = (int)game_time_limit/2;
        player2.choice = (player1.choice == 'w')? 'b' : 'w';

        current_player = (player1.choice == 'w')? &player1 : &player2;

        valid_game = (player1.choice != player2.choice) && (player1.choice == 'w' || player1.choice == 'b') && (player2.choice == 'w' || player2.choice == 'b');

        auto now = chrono::system_clock::now();
        game_start_time = chrono::system_clock::to_time_t(now);

        current_player->turn_start_time = chrono::system_clock::to_time_t(now);
    }
    Game() : gameBoard(initializeGameBoard()), current_player(&player1), total_moves(0), valid_game(false) {}


    GamePlayer getCurrentPlayer() { return *current_player; }

    int getTotalMoves() { return total_moves; }
    void setTotalMoves(int moves) { total_moves = moves; }

    bool isValidGame() { return valid_game; }
    void setValidGame(bool valid) { valid_game = valid; }

    GamePlayer getPlayer1() { return player1; }
    void setPlayer1( GamePlayer& p1) { player1 = p1; }

    void setPlayer1(const GamePlayer& p1) {
        player1 = p1;
    }

    GamePlayer getPlayer2() { return player2; }
    void setPlayer2(GamePlayer& p2) { player2 = p2; }

    void setPlayer2(const GamePlayer& p2) {
        player2 = p2;
    }

    vector<vector<char>> getGameBoard() const {
        return gameBoard;
    }
     void setGameStartTime() {
        auto now = chrono::system_clock::now();
        game_start_time = chrono::system_clock::to_time_t(now);
    }

    time_t getGameStartTime() const {
        return game_start_time;
    }
    bool validateMatch() {
        
        if (player1.player_time_limit < 0) {
            return false;
        }
        if (player2.player_time_limit < 0) {
            return false;
        }


        if (player1.choice == player2.choice || player1.choice == '\0' || player2.choice == '\0') {
            return false;
        }

        if ((player1.choice == 'b' && player2.choice == 'w') || (player1.choice == 'w' && player2.choice == 'b')) {
            return true;
        }

        return false;
    }

    string getBoard() {
        string board="";
        for (int i = 0; i < gameBoard.size(); ++i) {
            for (int j = 0; j < gameBoard[i].size(); ++j) {
                board += gameBoard[i][j];
                board += " ";
            }
            board += '\n';
        }
        // cout<<board<<endl;
        return board;
    }


    bool makeMove(int row, int col) {
        if(current_player->choice == 'w'){
            gameBoard[row][col] = 'O';
        }
        else{
            gameBoard[row][col] = 'X';
        }
        return true;
    }

    bool checkWin() {
        // rows and columns
        for (int i = 1; i <= 3; ++i) {
            if (gameBoard[i][1] != '.' && gameBoard[i][1] == gameBoard[i][2] && gameBoard[i][1] == gameBoard[i][3]) {
                return true; // Row win
            }
            if (gameBoard[1][i] != '.' && gameBoard[1][i] == gameBoard[2][i] && gameBoard[1][i] == gameBoard[3][i]) {
                return true; // Column win
            }
        }
        //diagonals
        if (gameBoard[1][1] != '.' && gameBoard[1][1] == gameBoard[2][2] && gameBoard[1][1] == gameBoard[3][3]) {
            return true;
        }
        if (gameBoard[1][3] != '.' && gameBoard[1][3] == gameBoard[2][2] && gameBoard[1][3] == gameBoard[3][1]) {
            return true;
        }
        return false;
    }

    bool checkTie() {
        for (int i = 1; i <= 3; ++i) {
            for (int j = 1; j <= 3; ++j) {
                if (gameBoard[i][j] == '.') {
                    return false;
                }
            }
        }
        return true;
    }


    void switchPlayer() {
        current_player = (current_player->user_name == player1.user_name) ? &player2 : &player1;
    }

    void update_time_limit(){
        auto now = chrono::system_clock::now();
        int current_time = chrono::system_clock::to_time_t(now);
        GamePlayer* player = (current_player->user_name == player1.user_name) ? &player1 : &player2;
        int time_difference = current_time - player->turn_start_time;
        player->player_time_limit -= time_difference;
        player->turn_start_time = -1;
        //update other player
        GamePlayer* otherPlayer = (current_player->user_name == player1.user_name) ? &player2 : &player1;
        otherPlayer->turn_start_time = current_time;

        // cout<<"after :"<<endl;
        // cout<<player1.printGamePlayer()<<endl;
        // cout<<player2.printGamePlayer()<<endl;
    }
};

#endif