#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <strings.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <vector>
#include <fstream>
#include <sstream>
#include <fcntl.h>
#include <unordered_map>
#include <iomanip>
#include <string>
#include <bitset>
#include <chrono>
#include <thread>
#include <mutex>
#include <condition_variable>
#include "game.h"

using namespace std;

struct Player {
    int fd = -1;
    string user_name="";
    string password="$";
    int count = 0;
    bool valid= false;
    string info = "<none>";
    float rating = 0.0;
    int wins = 0;
    int loses = 0;
    bool quiet = false;
    vector<string> blocked_users;
    Game game = Game();
    bool in_game = false;
    bool close_session = false;
    bool mail_body = false;
};

struct Mail {
    Player* player;
    string from = "";
    string to = "";
    string title = "";
    string body = "";
    bool read = false;
    string time = "";
};
struct GameOptions{
    int total_game_time = 600;
    char choice = 'w';
};
struct GameRequest{
    Player* host;
    GameOptions host_game_options;
    Player* invitee;

};

uint16_t port = 55551;
const unsigned MAXBUFLEN = 2048;
char buf[MAXBUFLEN];
vector<Player> players;
vector<Game> games;
int guest_count = 0;
unordered_map<Player*, Game*> observers;
vector<Mail> mails_in_progress;
auto now = chrono::system_clock::now();
int current_time = chrono::system_clock::to_time_t(now);
vector<GameRequest> game_invites;
vector<Game*> games_ptr;


// Create a socket
int createSocket() {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }
    return sockfd;
}

// Bind the socket
void bindSocket(int sockfd, struct sockaddr_in& serv_addr) {
    if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1) {
        perror("Binding failed");
        close(sockfd); // Close the socket before exiting
        exit(EXIT_FAILURE);
    }
}

// Listen for connections
void listenForConnections(int sockfd) {
    if (listen(sockfd, 5) == -1) {
        perror("Listen failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
}

// Accept a connection
int acceptConnection(int sockfd, struct sockaddr_in& cli_addr, socklen_t& sock_len) {
    int client_sockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &sock_len);
    if (client_sockfd == -1) {
        perror("Accept failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    return client_sockfd;
}

bool writeToClient(int client_sockfd,ssize_t n, Player* player, bool override_buf){
    if(!override_buf){
        if(player!= nullptr && player->fd != -1) {
            player->count = player->count+ 1;
            if(player->valid){
                strcat(buf, ("\n<"+player->user_name+": " + to_string(player->count) + '>').c_str());
            }
            else{
                strcat(buf, ("\n<guest: "+ to_string(player->count) + '>').c_str());
            }
        }
        else{
            strcat(buf, "\nusername (guest):");
        }
    }
    if (write(client_sockfd, buf, strlen(buf)) < 0) {
        perror("Write error");
        return true;
    }
    return false;
}

int findPlayer(int client_fd) {
    for (size_t i = 0; i < players.size(); ++i) {
        if (players[i].fd == client_fd) {
            return i;
        }
    }
    return -1;
}

Player* getOnlinePlayer(string user_name) {
    for (auto& player : players) {
        if (player.user_name == user_name) {
            return &player;
        }
    }
    return nullptr;
}

Player* getOtherActiveSession(string user_name) {
    for (auto& player : players) {
        if (player.user_name == user_name && player.valid == true) {
            return &player;
        }
    }
    return nullptr;
}

bool removePlayer(int fd) {
    int player_index = findPlayer(fd);
    if(player_index != -1){
        cout<<"Removig player at index:"<<player_index<<endl;
        players.erase(players.begin() + player_index);
        return true;
    }

    return false;
}

bool handleClose(int client_fd){
    // for (size_t i = 0; i < client_fds.size(); ++i) {
    //     if (client_fds[i] == client_fd) {
    //         client_fds.erase(client_fds.begin() + i);
    //         break; 
    //     }
    // }
    close(client_fd);
    return removePlayer(client_fd);
}

void savePlayerDetails(Player* player) {
    ifstream file("player_details.txt", ios::app);
    if (!file.is_open()) {
        cerr << "Failed to open the file." << endl;
        return;
    }

    vector<string> lines;
    string line;
    bool userExists = false;
    string updatedLine = player->user_name + "/" + player->password + "/" + player->info + "/" 
                        + to_string(player->rating)+ "/"  + to_string(player->wins) + "/" + to_string(player->loses)
                        + "/" + (player->quiet ? "Yes" : "No") + "/";
 
    for (size_t i = 0; i < player->blocked_users.size(); ++i) {
        updatedLine += player->blocked_users[i];
        if (i != player->blocked_users.size() - 1) {
            updatedLine += ",";
        }
    }

    while (getline(file, line)) {
        string userName;
        istringstream iss(line);
        getline(iss, userName, '/');
        if (userName == player->user_name) {
            userExists = true;
            line = updatedLine;
        }
        lines.push_back(line);
    }
    file.close();

    //Add New record for Player
    if (!userExists) {
        lines.push_back(updatedLine);
    }

    // Update player details
    ofstream outfile("player_details.txt");
    if (!outfile.is_open()) {
        cerr << "Failed to open the file." << endl;
        return;
    }
    for (const string& updatedLine : lines) {
        outfile << updatedLine << endl;
    }
    outfile.close();
}

vector<string> getBlockedUsers(string blockedUsersStr) {
    vector<string> blockedUsers;
    stringstream ss(blockedUsersStr);
    string username;
    while (getline(ss, username, ',')) {
        blockedUsers.push_back(username);
    }
    return blockedUsers;
}

Player getPlayerDetails(string user_name) {
    ifstream file("player_details.txt");
    if (!file.is_open()) {
        cerr << "Failed to open the file." << endl;
        return Player{};
    }

    string line;
    while (getline(file, line)) {
        istringstream iss(line);
        string userName,details;
        
        getline(iss, userName, '/'); // Extract user name from line
        if (userName == user_name) {
            string remaining;
            getline(iss, details);
            istringstream detailsStream(details);
            string password, info, quietStr, blockedUsersStr, ratingStr, winsStr,losesStr;
            double rating;
            int wins, loses;
            bool quiet;
            getline(detailsStream, password, '/');
            getline(detailsStream, info, '/');
            getline(detailsStream, ratingStr, '/');
            getline(detailsStream, winsStr, '/');
            getline(detailsStream, losesStr, '/');
            getline(detailsStream, quietStr, '/');
            getline(detailsStream, blockedUsersStr, '/');

            file.close();

            cout << password << endl;
            Player player = Player{};
            player.user_name = userName;
            player.password = password;
            player.info = info;
            player.rating = stod(ratingStr);
            player.wins = stoi(winsStr);
            player.loses = stoi(losesStr);
            player.quiet = (quietStr == "Yes" ? true : false);
            player.blocked_users = getBlockedUsers(blockedUsersStr);

            return player;
        }
    }

    file.close();
    return Player{};
}

vector<Player> getAllPlayers() {
    vector<Player> all_players;
    ifstream file("player_details.txt");
    if (!file.is_open()) {
        cerr << "Failed to open the file." << endl;
        return all_players;
    }

    string line;
    while (getline(file, line)) {
        istringstream iss(line);
        string userName, password, info, quietStr, blockedUsersStr, ratingStr, winsStr, losesStr;
        double rating;
        int wins, loses;
        bool quiet;

        getline(iss, userName, '/');
        getline(iss, password, '/');
        getline(iss, info, '/');
        getline(iss, ratingStr, '/');
        getline(iss, winsStr, '/');
        getline(iss, losesStr, '/');
        getline(iss, quietStr, '/');
        getline(iss, blockedUsersStr, '/');
        rating = stod(ratingStr);
        wins = stoi(winsStr);
        loses = stoi(losesStr);
        quiet = (quietStr == "Yes");

        vector<string> blocked_users = getBlockedUsers(blockedUsersStr);
        Player player;
        player.user_name = userName;
        player.password = password;
        player.info = info;
        player.rating = rating;
        player.wins = wins;
        player.loses = loses;
        player.quiet = quiet;
        player.blocked_users = blocked_users;

        all_players.push_back(player);
    }

    file.close();

    return all_players;
}


bool findPlayerUserName(string user_name) {
    Player player = getPlayerDetails(user_name);
    cout<<"result of finding player :"<<player.user_name<<endl;
    if(player.user_name != ""){
        return true;
    }
    return false;
}

bool registerPlayer(int client_fd, string user_name, string password) {
    cout << "Registering user: " << user_name << endl;
    int player_index = findPlayer(client_fd);
    Player* player = &players[player_index];

    if (findPlayerUserName(user_name)) {
        strcat(buf, "Username already exists. Please use a different username and retry.");
        return false;
    } else {
        player->user_name = user_name;
        player->password = password;
        player->valid = true;
        savePlayerDetails(player);
        return true;
    }
}

int addPlayer(int client_fd, string user_name){
    Player player = Player();
    player.fd = client_fd;
    player.user_name = user_name;
    players.push_back(player);

    return players.size()-1;

}

void printClientSockets(const vector<int>& client_fds) {
    cout << "Client sockets: ";
    for (int sockfd : client_fds) {
        cout << sockfd << " ";
    }
    cout << endl;
}

void printPlayerDetails() {
    cout << "Player Details:" << endl;
    for (size_t i = 0; i < players.size(); ++i) {
        const Player& player = players[i];
        cout << "Index: " << i << ", FD: " << player.fd << ", Username: " << player.user_name << ", Count: " << player.count << ", Valid User: " << player.valid << endl;
    }
}

bool login(int client_fd, string password) {
    int player_index = findPlayer(client_fd);
    if(player_index != -1) {
        Player* player = &players[player_index];
        Player playerDetails = getPlayerDetails(player->user_name);
        // cout<<password<<" "<<playerDetails.password<<endl;
        if(password == playerDetails.password) {
            Player* otherActiveSession = getOtherActiveSession(player->user_name);
            cout<<"Current sessions before login:" << endl;
            printPlayerDetails();
            if(otherActiveSession != nullptr){
                cout<<"Another session found at fd:"<<otherActiveSession->fd<<endl;
                strcat(buf, "Another session created\n");
                writeToClient(otherActiveSession->fd,strlen(buf), otherActiveSession, true);
                memset(buf, 0, sizeof(buf));
                otherActiveSession->close_session = true;
            }
            cout<<"Current sessions after login :" << endl;
            printPlayerDetails();

            player->valid = true;
            player->password = playerDetails.password;
            player->info = playerDetails.info;
            player->rating = playerDetails.rating;
            player->wins = playerDetails.wins;
            player->loses = playerDetails.loses;
            player->quiet = playerDetails.quiet;
            player->blocked_users = playerDetails.blocked_users;
            return true;
        }
        else {
            strcat(buf, "Invalid username or password");
            return false;
        }
    }
    else {
        strcat(buf, "Invalid username or password");
        return false;
    }
    return false;
}

string processText(const char* input) {
    string text;
    for(int i=0;i<strlen(input)-2;i++){
        text = text+input[i];
    }
    return text;
}

char* getHelpText() {
    char* help = new char[MAXBUFLEN];

    ifstream file("help.txt");
    if (!file.is_open()) {
        cerr << "Failed to open the file." << endl;
        return nullptr;
    }
    file.read(help, MAXBUFLEN);
    help[file.gcount()] = '\0';
    return help;
}

ssize_t readFromClient(int client_sockfd,ssize_t n){
    memset(buf, 0, sizeof(buf));
    n = read(client_sockfd, buf, MAXBUFLEN);
    return n;
}

char* getOnlineUsers() {
    // Calculate the required buffer size
    int totalLength = 0;
    for (const auto& player : players) {
        totalLength += player.user_name.length() + 1; // Add 1 for space or newline
    }
    char* usersBuf = new char[MAXBUFLEN];
    usersBuf[0]= '\0';
    strcat(usersBuf, "Total ");
    strcat(usersBuf, to_string(players.size()).c_str());
    strcat(usersBuf, " user(s) online:\n");
    for (const auto& player : players) {
        strcat(usersBuf, player.user_name.c_str());
        strcat(usersBuf, " ");
    }

    return usersBuf;
}

vector<string> splitInputText(string receivedText){
    vector<string> split;
    stringstream ss(receivedText);
    string token;
    while (ss >> token) {
        split.push_back(token);
    }
    return split;
}

void handleInfo(Player* player, string receivedText){
    vector<string> split = splitInputText(receivedText);
    if(split.size() == 2){
        player->info = split[1];
    }
    else{
        strcat(buf,"Invalid usage of info\n,\t info <msg>             # change your information to <msg>");
    }
}

bool checkForOnline(string user_name) {
    for (const Player& player : players) {
        if (player.user_name == user_name && player.valid) {
            return true;
        }
    }
    return false;
}

string getAllPlayerStats() {
    string allStats = "";
    vector<Player> allPlayers = getAllPlayers();
    if (allPlayers.empty()) {
        cout << "No players found." << endl;
        return allStats;
    }
    cout<<"All players:"<< allPlayers.size()<<endl;
    for (const Player& player : allPlayers) {
        allStats += "User: " + player.user_name + "\n";
        allStats += "Info: " + player.info + "\n";
        allStats += "Rating: " + to_string(player.rating) + "\n";
        allStats += "Wins: " + to_string(player.wins) + ", Loses: " + to_string(player.loses) + "\n";
         if(player.quiet){
                allStats += "Quiet: Yes\n";
            }
            else{
                allStats += "Quiet: No\n";
            }
        allStats += "Blocked users: ";
        if (player.blocked_users.empty()) {
            allStats += "<none>\n";
        } else {
            for (size_t i = 0; i < player.blocked_users.size(); ++i) {
                allStats += player.blocked_users[i];
                if (i != player.blocked_users.size() - 1) {
                    allStats += ", ";
                }
            }
            allStats += "\n";
        }
        allStats += "\n";
    }

    return allStats;
}


void handleStats(Player* player, string receivedText) {
    string user_name = "";
    vector<string> split = splitInputText(receivedText);
    if (split[0] == "stats") {
        if(split.size() == 1){
            string stats = getAllPlayerStats();
            strcat(buf, stats.c_str());
            return;
        }
        else if (split.size() == 2) {
            user_name = split[1];
        }
        else{
            strcat(buf, "Invalid usage of stats\n,\t stats [name]           # Display user information");
            return;
        }
        Player playerDetails = getPlayerDetails(user_name);
        if (playerDetails.user_name != "") {
            string details;
            details += '\n';
            details += "User: " + playerDetails.user_name + "\n";
            details += "Info: " + playerDetails.info + "\n";
            details += "Rating: " + to_string(playerDetails.rating) + "\n";
            details += "Wins: " + to_string(playerDetails.wins) + ", Loses: " + to_string(playerDetails.loses) + "\n";
            if(playerDetails.quiet){
                details += "Quiet: Yes\n";
            }
            else{
                details += "Quiet: No\n";
            }
            details += "Blocked users: ";
            if (playerDetails.blocked_users.empty()) {
                details += "<none>";
            } else {
                for (const string& blockedUser : playerDetails.blocked_users) {
                    details += blockedUser + ", ";
                }
                details = details.substr(0, details.size() - 2);
            }
            details += '\n';
            //check for oniline
            if(checkForOnline(user_name)){
                details += user_name + " is currently online.\n";
            }
            else{
                details += user_name + " is off-line.\n";
            }
            strcat(buf,details.c_str());
        }
        else{
            string error = "User "+ user_name +" does not exist.\n";
            strcat(buf, error.c_str());
        }
    } else {
        strcat(buf, "Invalid usage of stats\n,\t stats [name]           # Display user information");
    }
}

bool userBlocked(Player* player, string user_name) {
    for (string blocked_user : player->blocked_users) {
        if (blocked_user == user_name) {
            return true;
        }
    }
    return false;
}

void handleUnBlock(Player* player, string receivedText) {
    cout<<"entered unblock module"<<endl;
    string status = "";
    vector<string> split = splitInputText(receivedText);
    if (split.size() == 2 && split[0] == "unblock") {
        string user_name = split[1];
        if (findPlayerUserName(user_name)) {
            if (userBlocked(player, user_name)) {
                for (auto it = player->blocked_users.begin(); it != player->blocked_users.end(); ++it) {
                    if (*it == user_name) {
                        player->blocked_users.erase(it);
                        status += "User " + user_name + " unblocked.\n";
                        break;
                    }
                }
            } else {
                status += "User " + user_name + " is not in blocked List.\n";
            }
        } else {
            status += "User " + user_name + " not found.\n";
        }
    } else {
        status = "Invalid usage of unblock command.\n\tunblock <id>           # Allow communication from <id> again.\n";
    }
    strcat(buf, status.c_str());
}

void handleBlock(Player* player, string receivedText) {
    string status="";
    vector<string> split = splitInputText(receivedText);
    if (split.size() == 2 && split[0] == "block") {
        string user_name = split[1];
        if (findPlayerUserName(user_name)) {
            if(!userBlocked(player, user_name)){
                player->blocked_users.push_back(user_name);
                
                status += "User " + user_name + " blocked.\n";
            }
            else{
                status += "User " + user_name + " was blocked before.\n";
            }
        } else {
            status += "User " + user_name + " not found\n";
        }
    } else {
        status="Invalid usage of block command.\n \tblock <id>             # No more communication from <id>\n";
    }
    strcat(buf, status.c_str());
}

void handleShout(Player* player, string receivedText){
    string message = "";
    vector<string> split = splitInputText(receivedText);
    if (split[0] == "shout") {
        message += "*" + player->user_name + "* shouted ";
        if(split.size() == 2){
            message += split[1];
        }
        for(Player onlinePlayer: players){
            if(!userBlocked(&onlinePlayer,player->user_name) && !onlinePlayer.quiet){
                memset(buf, 0, sizeof(buf));
                strcat(buf,message.c_str());
                if(onlinePlayer.user_name == player->user_name){
                    writeToClient(onlinePlayer.fd,strlen(buf), &onlinePlayer, true);
                }
                else{
                    writeToClient(onlinePlayer.fd,strlen(buf), &onlinePlayer, false);
                }
            }
        }
    }
    else{
        strcat(buf,"Invalid usage of shout.\n \tshout <msg>            # shout <msg> to every one online");
    }
    memset(buf, 0, sizeof(buf));
}

void handleTell(Player* player, string receivedText){
    string message = "";
    string status = "";
    vector<string> split = splitInputText(receivedText);
    if (split[0] == "tell" && split.size()==3) {
        message += "*" + player->user_name + "* : " + split[2] + '\n';
        string user_name = split[1];
        for(Player onlinePlayer: players){
            if(!userBlocked(&onlinePlayer,player->user_name)){
                memset(buf, 0, sizeof(buf));
                strcat(buf,message.c_str());
                if(onlinePlayer.user_name == player->user_name && onlinePlayer.user_name == user_name){
                    writeToClient(onlinePlayer.fd,strlen(buf), &onlinePlayer, true);
                    break;
                }
                else if(onlinePlayer.user_name == user_name){
                    writeToClient(onlinePlayer.fd,strlen(buf), &onlinePlayer, false);
                    break;
                }
            }
        }
        memset(buf, 0, sizeof(buf));
    }
     else{
        status += "Invalid usage of tell.\n \ttell <name> <msg>      # tell user <name> message";
    }
     strcat(buf, status.c_str());
}

void saveGame(Player* player1,Player* player2, Game game){
    
    player1->game= game;
    player2->game = game;
    string player1_username = game.getPlayer1().user_name;
    string player2_username = game.getPlayer2().user_name;
    string filename = "game_" + player1_username + "_" + player2_username + ".txt";

    int game_file = open(filename.c_str(), O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    if (game_file == -1) {
        cerr << "Error: Unable to open file for writing.\n";
        return;
    }
    string match_summary= "";
    match_summary += "Game Summary:\n";
    match_summary += "--------------------\n";
    match_summary += "Player1: " + player1_username + "\t" + " Remainig Time:" + to_string(game.getPlayer1().player_time_limit) +"\n";
    match_summary += "Player2: " + player2_username + "\t" + " Remainig Time:" + to_string(game.getPlayer2().player_time_limit) +"\n";
    match_summary += "Current Turn: " + game.getCurrentPlayer().user_name + "\n";
    match_summary += "Total moves: " + to_string(game.getTotalMoves()) + "\n";
    // match_summary += "Time: " + player1->getGameTime() + "\n";
    match_summary += "Game Board:\n";
    match_summary += game.getBoard() + "\n";

    write(game_file, match_summary.c_str(), match_summary.size());
    close(game_file);
    return;

}

void createGameFile(Player* player1,Player* player2){
    Game game = player1->game;
    string player1_username = game.getPlayer1().user_name;
    string player2_username = game.getPlayer2().user_name;
    string filename = "game_" + player1_username + "_" + player2_username + ".txt";
    int game_file = open(filename.c_str(), O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    if (game_file == -1) {
        cerr << "Error: Unable to craete file.\n";
        return;
    }
    close(game_file);
    saveGame(player1,player2, game);
    return;
}

string getMatchSummary(Game game){
    string player1_username = game.getPlayer1().user_name;
    string player2_username = game.getPlayer2().user_name;
    string filename = "game_" + player1_username + "_" + player2_username + ".txt";
    cout<<filename<<endl;

    int game_file = open(filename.c_str(), O_RDONLY);
    string line, match_summary= "";
    if (game_file == -1) {
        cerr << "Error: Unable to read file.\n";
        return match_summary;
    }
    char buffer[MAXBUFLEN]; // Buffer to read file contents
    ssize_t bytesRead;

    while ((bytesRead = read(game_file, buffer, sizeof(buffer))) > 0) {
        match_summary.append(buffer, bytesRead);
    }
    close(game_file);

    return match_summary;
}

void displayGame(Player* player, Game game, string additional_comments, bool override){
    memset(buf, 0, sizeof(buf));
    string displayText = "";
    if(additional_comments.size() > 0){
        displayText += additional_comments;
    }
    displayText += getMatchSummary(game);
    strcat(buf,displayText.c_str());
    writeToClient(player->fd,strlen(buf), player, override);
    memset(buf, 0, sizeof(buf));
}

void removeGame(Game game) {
    string player1_username = game.getPlayer1().user_name;
    string player2_username = game.getPlayer2().user_name;

    for (auto it = games.begin(); it != games.end(); ++it) {
        if (it->getPlayer1().user_name == player1_username && it->getPlayer2().user_name == player2_username) {
            games.erase(it);
            break;
        }
    }
    for (int i =0;i<games_ptr.size();i++) {
        if (games_ptr[i]->getPlayer1().user_name == player1_username && games_ptr[i]->getPlayer2().user_name == player2_username) {
            games_ptr.erase(games_ptr.begin() + i);
            break;
        }
    }
}

void removeObservers(Game* game) {
    for (auto it = observers.begin(); it != observers.end();) {
        if (it->second->getPlayer1().user_name == game->getPlayer1().user_name && it->second->getPlayer2().user_name == game->getPlayer2().user_name) {
            Player* player = it->first;
            it = observers.erase(it);
        } else {
            ++it;
        }
    }
}

void resetGameStatus(Player* player1,Player* player2){

    removeObservers(&player1->game);
    removeGame(player1->game);
    player1->in_game = false;
    player1->game = Game();

    player2->in_game = false;
    player2->game = Game();
}

string getAllGames() {
    string games_list = "Total Games: " + to_string(games.size()) + "\n";
    int count = 0;
    for (auto& game : games) {
        games_list += to_string(count++) + " " + game.getPlayer1().user_name + " vs " + game.getPlayer2().user_name + "\n";
    }
    return games_list;
}

void displayGameForObservers(Game* game, string status) {

    for (const auto& observer : observers) {
        if (observer.second->getPlayer1().user_name == game->getPlayer1().user_name && observer.second->getPlayer2().user_name == game->getPlayer2().user_name) {
            Player* player = observer.first;
            displayGame(player, *game, status, false);
        }
    }
}

GameRequest* getGameRequest(GameRequest game_request) {
    for (auto& request : game_invites) {
        if (request.host->user_name == game_request.host->user_name &&
            request.invitee->user_name == game_request.invitee->user_name) {
            return &request;
        }
    }
    return nullptr;
}

void deleteGameRequest(GameRequest game_request) {
    auto it = game_invites.begin();
    while (it != game_invites.end()) {
        if (it->host->user_name == game_request.host->user_name &&
            it->invitee->user_name == game_request.invitee->user_name) {
            it = game_invites.erase(it);
        } else {
            ++it;
        }
    }
}

void sendGameRequestToPlayer(Player* host,Player* invitee,GameOptions host_game_options){
    GameRequest new_game_request;
    new_game_request.host = host;
    new_game_request.invitee = invitee;
    new_game_request.host_game_options = host_game_options;
    game_invites.push_back(new_game_request);
    char intitee_choice = (new_game_request.host_game_options.choice == 'w')? 'b' : 'w';

    string request = host->user_name + " invited to game with below game options:\n";
    request += "Your choice [b/w]:" + new_game_request.host_game_options.choice;
    request += "Total game time:" + to_string(new_game_request.host_game_options.total_game_time) + "\n";
    request += "\nTo Accept and start game usee command <match " + host->user_name +" "+ intitee_choice +" "+ to_string(new_game_request.host_game_options.total_game_time) + ">\n";
    request += "To differ game options, you can invite using 'match' command\n";
    strcat(buf, request.c_str());
    writeToClient(invitee->fd,strlen(buf), invitee, false);
    memset(buf, 0, sizeof(buf));
    return;                            
}

void startGame(Player* player1,Player* player2, GameRequest game_request) {
    Player* host = player1;
    Player* invitee = player2;
    Game game(host->user_name, invitee->user_name, game_request.host_game_options.total_game_time, game_request.host_game_options.choice);
    host->game = game;
    invitee->game = game;
    if (host->game.isValidGame()) {
        host->in_game = true;
        invitee->in_game = true;
        string status = "Match Started\n";
        createGameFile(host, invitee);
        saveGame(host, invitee, game);
        displayGame(player1, game, status, true);
        displayGame(player2, game, status, false);
        games.push_back(host->game);
        games_ptr.push_back(&host->game);
    }
    return;
}

void removeAllOtherInvites(string username) {
    string status = username + " has joined other match, send new reuest later\n";
    for (auto it = game_invites.begin(); it != game_invites.end();) {
        if (it->host->user_name == username) {
            strcat(buf,status.c_str());
            writeToClient(it->host->fd,strlen(buf), it->host, false);
            memset(buf, 0, sizeof(buf));
            it = game_invites.erase(it);
        }
        else if(it->invitee->user_name == username){
            strcat(buf,status.c_str());
            writeToClient(it->invitee->fd,strlen(buf), it->invitee, false);
            memset(buf, 0, sizeof(buf));
            it = game_invites.erase(it);
        }else {
            ++it;
        }
    }
    return;
}

void handleMatch(Player* player1, string receivedText){
    string user_name = "";
    string error = "";
    string general_error = "Invalid usage of 'match'.\n \tmatch <name> <b|w> [t] # Try to start a game,\n\t\tt -> total game time in seconds\n\t\t<b|w> -> player choice should be either 'b' or 'w'\n";
    general_error += "Usages : \n\tmatch <name>\n\tmatch <name> <b|w>\n\tmatch <name> <b|w> [t]\n";
    general_error += "Sample Usages : \n\tmatch user_a\n\tmatch user_a w\n\tmatch user_a w 300\n";
    int total_game_time = 600;
    char player2_choice;
    char player_choice = 'w';
    vector<string> split = splitInputText(receivedText);
    if (split[0] == "match" && split.size() >= 2 && split.size() <= 4) {
        user_name = split[1];
        if(split.size()  >= 3){
            if(split[2].size()==1 && (split[2][0] == 'b' || split[2][0] == 'w')){
                player_choice = split[2][0];
            }
            else{
                strcat(buf,general_error.c_str());
                return;
            }
        }
        if(split.size()  == 4){
            total_game_time = stoi(split[3]);
        }
        if(!player1->in_game){
            if(player1->user_name != user_name){
                if(findPlayerUserName(user_name)){
                    Player* player2 = getOnlinePlayer(user_name);
                    if(player2 != nullptr){
                        GameRequest old_game_request;
                        old_game_request.host = player2;
                        old_game_request.invitee = player1;
                        GameRequest* game_request = getGameRequest(old_game_request);
                        if(game_request!=nullptr){
                            player2_choice = (player_choice == 'w')? 'b' : 'w';
                            if(game_request->host_game_options.choice != player_choice && game_request->host_game_options.total_game_time == total_game_time){
                                startGame(player1, player2, *game_request);
                                deleteGameRequest(*game_request);
                                removeAllOtherInvites(player1->user_name);
                                removeAllOtherInvites(player2->user_name);
                            }
                            else{
                                deleteGameRequest(*game_request);
                                GameOptions host_game_options = {total_game_time, player_choice};
                                sendGameRequestToPlayer(player1, player2, host_game_options);
                                strcat(buf, "Match invitation sent with updated game options\n");
                            }
                        }
                        else{
                            GameOptions host_game_options = {total_game_time, player_choice};
                            sendGameRequestToPlayer(player1, player2, host_game_options);
                            strcat(buf, "Match invitation sent\n");
                        }
                    }
                    else{
                        error = "User " + user_name + " offline";
                    }
                }
                else{
                    error = "User " + user_name + " not found.\n";
                }
            }
            else{
                error = "You can't with your-self.\n";
            }
        }
        else{
            error = "Please finish a game before starting a new one.";
        }
    }
    else{
        error = general_error;
    }
    if(error != ""){
        strcat(buf,error.c_str());
    }
    return;
}

void validateGames() {
    for(auto game : games_ptr) {
        auto now = chrono::system_clock::now();
        int current_time = chrono::system_clock::to_time_t(now);
        cout<<"Validating Game:"<< game->getCurrentPlayer().user_name<<endl;
        if(getOnlinePlayer(game->getCurrentPlayer().user_name) !=nullptr ){
        int time_difference = current_time - game->getCurrentPlayer().turn_start_time;
        if (game->getCurrentPlayer().player_time_limit - time_difference < 0) {
            Player* current_player;
            Player* other_player;
            current_player = getOnlinePlayer(game->getCurrentPlayer().user_name);
            if(current_player->user_name == game->getPlayer1().user_name){
                other_player = getOnlinePlayer(game->getPlayer2().user_name);
            }
            else{
                other_player = getOnlinePlayer(game->getPlayer1().user_name);
            }
            other_player->wins++;
            other_player->rating = other_player->rating +0.5;
            current_player->loses++;

            //send status to other player
            string status = "User "+ current_player->user_name + " exceded time limit, game ended.\n ";
            status += "You won the game\n ";

            memset(buf, 0, sizeof(buf));
            strcat(buf, status.c_str());
            writeToClient(other_player->fd,strlen(buf), other_player, false);
            memset(buf, 0, sizeof(buf));

            //obervers
            status = "User "+ current_player->user_name + " exceded time limit, game ended.\n ";
            status += "User "+ other_player->user_name + " won the game\n ";
            displayGameForObservers(game, status);

            //send status to current player
            memset(buf, 0, sizeof(buf));
            status = "You exceded time limit, game ended.\n ";
            status += "User "+ other_player->user_name + " won the game\n ";
            strcat(buf, status.c_str());
            writeToClient(current_player->fd,strlen(buf), current_player, false);

            savePlayerDetails(current_player);
            savePlayerDetails(other_player);
            resetGameStatus(current_player,other_player);
        }
        }
    }
}

void handleMove(Player* player, string receivedText){
    cout<<"check point 1"<<endl;
    int row=0,col=0;
    string error = "";
    Game game = player->game;
    cout<<player->user_name<<endl;
    if(game.getCurrentPlayer().user_name == player->user_name){
        //row
        if(receivedText[0] =='A' || receivedText[0] =='a'){
            row = 1;
        }
        else if(receivedText[0] =='B' || receivedText[0] =='b'){
            row = 2;
        }
        else if(receivedText[0] =='C' || receivedText[0] =='c'){
            row = 3;
        }
        else{
            error = "Invalid row ";
            error +=  receivedText[0];
            error +='\n';
            strcat(buf, error.c_str());
            return;
        }
        if(receivedText[1] =='1'){
            col = 1;
        }
        else if(receivedText[1] =='2'){
            col = 2;
        }
        else if(receivedText[1] =='3'){
            col = 3;
        }
        else{
            error = "Invalid column ";
            error +=  receivedText[1];
            error +='\n';
            strcat(buf, error.c_str());
            return;
        }
        cout<<"check point 2"<<endl;
        cout<<"row,col:"<<row<<","<<col<<endl;
        vector<vector<char>> gameBoard = game.getGameBoard();
        if(gameBoard[row][col] != '.'){
            error = receivedText + " already occupied\n";
            strcat(buf, error.c_str());
            return;
        }
        else{
            Player* otherPlayer;
            if(game.getCurrentPlayer().user_name == game.getPlayer1().user_name){
                otherPlayer = getOnlinePlayer(game.getPlayer2().user_name);
            }
            else{
                otherPlayer = getOnlinePlayer(game.getPlayer1().user_name);
            }
            game.makeMove(row, col);
            game.setTotalMoves(game.getTotalMoves()+1);
            game.update_time_limit();
            game.switchPlayer();
            saveGame(player,otherPlayer,game);
            if(game.checkWin()){
                // cout<<"enter check win"<<endl;
                string status = "**User "+ player->user_name + " won the game**\n";
                displayGame(otherPlayer, game, status, false);
                displayGameForObservers(&game, status);
                status = "**You won the game**\n";
                displayGame(player, game, status, true);
                //save stats
                player->wins++;
                player->rating = player->rating + 0.5;
                otherPlayer->loses++;
                savePlayerDetails(player);
                savePlayerDetails(otherPlayer);
                resetGameStatus(player,otherPlayer);
                return;
            }

            if(game.checkTie()){
                string status = "**Game Tied**\n";
                displayGame(player, game, status, true);
                displayGame(otherPlayer, game, status, false);
                displayGameForObservers(&game, status);
                //update stats
                resetGameStatus(player,otherPlayer);
                return;
            }
            displayGame(player, game, "", true);
            displayGame(otherPlayer, game, "", false);
            displayGameForObservers(&game, "");
        }
    }
    else{
        error = "Not your turn\n";
        strcat(buf, error.c_str());
        return;
    }
    return;
}

void handleResign(Player* player, string receivedText){

    string message = "";
    string status = "";
    vector<string> split = splitInputText(receivedText);
    if (split[0] == "resign" && split.size()==1) {
        Player* otherPlayer;
        Game game = player->game;
        if(player->user_name == game.getPlayer1().user_name){
            otherPlayer = getOnlinePlayer(game.getPlayer2().user_name);
        }
        else{
            otherPlayer = getOnlinePlayer(game.getPlayer1().user_name);
        }
        //send status to other player
        string status = "User "+ player->user_name + " resigned\n";
        strcat(buf, status.c_str());
        writeToClient(otherPlayer->fd,strlen(buf), otherPlayer, false);
        //obervers
        displayGameForObservers(&game, status);
        //send status to current player
        memset(buf, 0, sizeof(buf));
        status = "You Resigned\n";
        strcat(buf, status.c_str());
        // save stats
        otherPlayer->wins++;
        otherPlayer->rating = otherPlayer->rating +0.5;
        player->loses++;
        savePlayerDetails(player);
        savePlayerDetails(otherPlayer);
        resetGameStatus(player,otherPlayer);
    }
    else{
        strcat(buf, "Invalid usage of 'resign'.\n \tresign                 # Resign a game");
    }

}

void handleChangePassword(Player* player, string receivedText){
    string message = "";
    string status = "";
    vector<string> split = splitInputText(receivedText);
    if (split[0] == "passwd" && split.size()==2) {
        player->password = split[1];
        savePlayerDetails(player);
        strcat(buf, "Password changed successfully.\n");
    }
    else{
        strcat(buf, "Invalid usage of 'passwd'.\n \tpasswd <new>           # change password");
    }
    return;
}

void handleGames(Player* player, string receivedText){
    vector<string> split = splitInputText(receivedText);
    if (split[0] == "game" && split.size()==1) {
        string games_list = getAllGames();;
        cout<< games_list<<endl;
        strcat(buf, games_list.c_str());
    }
    else{
        strcat(buf, "Invalid usage of 'game'.\n \tgame                   # list all current games");
    }
    return;

}

string addObserver(Player* player, int gameId) {
    string status = "";
    if (gameId >= 0 && gameId < games.size()) {
        observers[player] = &games[gameId];
        status += "You are now observing game: " + to_string(gameId) + "\n";
        displayGame(player, games[gameId], status, true);
        memset(buf, 0, sizeof(buf));
    } else {
        status += "Invalid game ID.\n";
         strcat(buf, status.c_str());
    }

    return status;
}

string removePlayerObserver(Player* player) {
    string status = "";
    auto it = observers.find(player);
    if (it != observers.end()) {
        status += "You unobserved game";
        observers.erase(it);
    } else {
        status += "You are not observing any game";
    }
    return status;
}

void handleObserve(Player* player, string receivedText){

    vector<string> split = splitInputText(receivedText);
    if (split[0] == "observe" && split.size()==2) {
        if(!player->in_game){
            string status = addObserver(player, stoi(split[1]));
            cout<<status<<endl;
        }
        else{
            strcat(buf, "You are already in game, you can't observe other game\n");
        }
    }
    else{
        strcat(buf, "Invalid usage of 'observe'.\n \tobserve <game_num>     # Observe a game");
    }
    return;

}

void handleUnObserve(Player* player, string receivedText){
    vector<string> split = splitInputText(receivedText);
    if (split[0] == "unobserve" && split.size()==1) {
        string status = removePlayerObserver(player);
        strcat(buf, status.c_str());
    }
    else{
        strcat(buf, "Invalid usage of 'unobserve'.\n \tunobserve              # Unobserve a game");
    }
    return;
}

void handleQuiet(Player* player, string receivedText){
    vector<string> split = splitInputText(receivedText);
    if (split[0] == "quiet" && split.size()==1) {
        player->quiet = true;
        savePlayerDetails(player);
        string status = "Quiet mode Activated, no broadcast messages";
        strcat(buf, status.c_str());
    }
    else{
        strcat(buf, "Invalid usage of 'quiet'.\n \tquiet                  # Quiet mode, no broadcast messages");
    }
    return;
}

void handleNonQuiet(Player* player, string receivedText){
    vector<string> split = splitInputText(receivedText);
    if (split[0] == "nonquiet" && split.size()==1) {
        player->quiet = false;
        string status = "Non Quiet mode";
        savePlayerDetails(player);
        strcat(buf, status.c_str());
    }
    else{
        strcat(buf, "Invalid usage of 'nonquiet'.\n \tnonquiet               # Non-quiet mode");
    }
    return;
}

void handleKibitz(Player* player, string receivedText){
    vector<string> split = splitInputText(receivedText);
    if ((split[0] == "'" || split[0] =="kibitz") && split.size()==2) {
        string status = "";
        auto it = observers.find(player);
        if (it != observers.end()) {
            Game* game = it->second;
            status += "kibitx* " + player->user_name + " : " + split[1] + "\n";
            for (const auto& observer : observers) {
                if (observer.second->getPlayer1().user_name == game->getPlayer1().user_name && observer.second->getPlayer2().user_name == game->getPlayer2().user_name) {
                    if(!userBlocked(observer.first,player->user_name) && !observer.first->quiet){
                        memset(buf, 0, sizeof(buf));
                        strcat(buf,status.c_str());
                        if(observer.first->user_name == player->user_name){
                            writeToClient(observer.first->fd,strlen(buf), observer.first, true);
                        }
                        else{
                            writeToClient(observer.first->fd,strlen(buf), observer.first, false);
                        }
                    }
                }
            }
        } else {
            status += "You are not observing any game";
            strcat(buf,status.c_str());
        }
        strcat(buf,status.c_str());
    }
    else{
        strcat(buf, "Invalid usage of '.\n \t' <msg>                # Comment on a game\n or\n");
        strcat(buf, "Invalid usage of kibitz.\n \tkibitz <msg>           # Comment on a game when observing\n");
    }
    return;
}

string stringToBinary(string input) {
    string binaryString;
    for (char c : input) {
        binaryString += bitset<8>(c).to_string();
    }
    return binaryString;
}

string binaryToString(string binaryString) {
    string output;
    for (size_t i = 0; i < binaryString.length(); i += 8) {
        string byte = binaryString.substr(i, 8);
        char c = static_cast<char>(bitset<8>(byte).to_ulong());
        output += c;
    }
    return output;
}

string getCurrentDateTime() {
    time_t now = time(nullptr);
    tm* local_time = localtime(&now);
    char buffer[80];
    strftime(buffer, sizeof(buffer), "%a %b %d %H:%M:%S %Y", local_time);
    return std::string(buffer);
}

Mail* getInprogessMail(Player* player){
    for(int i=0;i<mails_in_progress.size();i++){
        if(mails_in_progress[i].player->user_name == player->user_name){
            return &mails_in_progress[i];
        }
    }
    return nullptr;
}

void saveMail(Player* player, Mail* mail) {
    ifstream mail_file("mails.txt", ios::app);
    if (!mail_file.is_open()) {
        cerr << "Failed to open the file." << endl;
        return;
    }

    vector<string> lines;
    string line;
    bool mailFound = false;

    string updatedLine = mail->from + "/" + mail->to + "/" + mail->title + "/" + mail->time + "/" + stringToBinary(mail->body) + "/" + (mail->read ? "Yes" : "No");

    while (getline(mail_file, line)) {
        string mailFrom, mailTo, mailBody, readStatus, mailTitle, mailTime;
        istringstream iss(line);
        getline(iss, mailFrom, '/');
        getline(iss, mailTo, '/');
        getline(iss, mailTitle, '/');
        getline(iss, mailTime, '/');
        getline(iss, mailBody, '/');
        getline(iss, readStatus, '/');

        // cout<<"mailFrom == mail->from :"<<(mailFrom == mail->from)<<endl;
        // cout<<"mailTitle == mail->title :"<<(mailTitle == mail->title)<<endl;
        // cout<<"mailBody == mail->body :"<<(mailBody == stringToBinary(mail->body))<<endl;
        // cout<<"mailTime == mail->time :"<<(mailTime == mail->time)<<endl;
        if (mailFrom == mail->from && mailTo == mail->to && mailTitle == mail->title && mailBody == stringToBinary(mail->body)  && mailTime == mail->time) {
            mailFound = true;
            line = updatedLine;
        }
        lines.push_back(line);
    }
    mail_file.close();

    if (!mailFound) {
        lines.push_back(updatedLine);
        cout<<"Mail not found adding new record"<<endl;
    }
    // Update
    ofstream outFile("mails.txt");
    if (!outFile.is_open()) {
        cerr << "Failed to open the file." << endl;
        return;
    }

    for (const string& line : lines) {
        outFile << line << endl;
    }
    outFile.close();
}

vector<Mail> readPlayerMails(Player* player) {
    ifstream inFile("mails.txt");
    vector<Mail> player_mails;
    if (!inFile.is_open()) {
        cerr << "Failed to open the file." << endl;
        return player_mails;
    }

    string line;
    while (getline(inFile, line)) {
        stringstream ss(line);
        string mailFrom, mailTo, mailBody, readStatus, mailTitle,mailTime;
        getline(ss, mailFrom, '/');
        getline(ss, mailTo, '/');
        getline(ss, mailTitle, '/');
        getline(ss, mailTime, '/');
        getline(ss, mailBody, '/');
        getline(ss, readStatus, '/');

        if (mailTo == player->user_name) {
            cout<<readStatus<<endl;
            Mail mail;
            mail.from = mailFrom;
            mail.to = mailTo;
            mail.title =mailTitle;
            mail.time = mailTime;
            mail.body = binaryToString(mailBody);
            mail.read = (readStatus == "Yes");
            player_mails.push_back(mail);
        }
    }
    inFile.close();

    return player_mails;
}

void handleListMails(Player* player, string receivedText){
    vector<string> split = splitInputText(receivedText);
    if ((split[0] == "listmail") && split.size()==1) {
        vector<Mail> mails = readPlayerMails(player);
        if(mails.size() > 0){
            strcat(buf, "Your messages:\n");
            for (size_t i = 0; i < mails.size(); ++i) {
                Mail mail = mails[i];
                string index = to_string(i);
                string mailInfo = "  " + index + "\t" + mail.from + "\t\t\"" + mail.title + "\t\t\"" + mail.time + "\"\n";
                strcat(buf, mailInfo.c_str());
            }
        }
        else{
            strcat(buf, " You have no mails.\n");
        }
    }
    else{
        strcat(buf, "Invalid usage of listmail.\n \tlistmail               # List the header of the mails");
    }
}

bool handleMailBody(Player* player, string receivedText){
    Mail* mail = getInprogessMail(player);
    if(mail != nullptr){
        if(receivedText[0] != '.'){
            mail->body += receivedText + "\n";
            return true;
        }
        if (receivedText[0] == '.') {
            mail->body += receivedText.substr(1,receivedText.size()-1);
            player->mail_body = false;
            mail->time = getCurrentDateTime();
            Player to_player = getPlayerDetails(mail->to);
            if(!userBlocked(&to_player,player->user_name)){
                saveMail(player, mail);
                string notif = "You have new mail from user " + player->user_name + "\n";
                strcat(buf,notif.c_str());
                if(checkForOnline(mail->to)){
                    Player* online_to_player = getOnlinePlayer(mail->to);
                    writeToClient(online_to_player->fd,strlen(buf), online_to_player, false);
                    memset(buf, 0, sizeof(buf));
                }
            }
            for (auto it = mails_in_progress.begin(); it != mails_in_progress.end(); ++it) {
                if ((*it).player->user_name == player->user_name) {
                    mails_in_progress.erase(it);
                    break;
                }
            }
            return false;
        }
    }
    return false;
}

bool handleMail(Player* player, string receivedText){
    vector<string> split = splitInputText(receivedText);
    if ((split[0] == "mail") && split.size()==3) {
        if(findPlayerUserName(split[1])){
            Mail mail;
            mail.player = player;
            mail.from = player->user_name;
            mail.to = split[1];
            mail.title = split[2];
            mail.body = "";
            mails_in_progress.push_back(mail);
            strcat(buf,"Please input mail body, finishing with '.' at the beginning of a line\n");
            player->mail_body = true;
            return true;
        }
        else{
            string err = "User Not Found " + split[1] + "\n";
            strcat(buf,err.c_str());
        }

    }
    else{
        strcat(buf, "Invalid usage of mail.\n \tmail <id> <title>      # Send id a mail\n");
    }
    return false;
}

void handleReadMail(Player* player, string receivedText){
    vector<string> split = splitInputText(receivedText);
    if ((split[0] == "readmail") && split.size()==2) {
        vector<Mail> mails = readPlayerMails(player);
        int mail_id = stoi(split[1]);
        if(mails.size() > mail_id){
            Mail mail = mails[mail_id];
            string mailInfo = "From: " + mail.from + "\n";
            mailInfo += "Title: " + mail.title + "\n";
            mailInfo += "Time: " + mail.time + "\n";
            mailInfo += "Body:\n" + mail.body + "\n";
            strcat(buf, mailInfo.c_str());
            mail.read = true;
            saveMail(player, &mail);
        }
        else{
            string err = "Enter valid mail id number.\n";
            strcat(buf,err.c_str());
        }

    }
    else{
        strcat(buf, "Invalid usage of readmail.\n \treadmail <msg_num>     # Read the particular mail\n");
    }
    return;
}

void deleteMail(Mail mail) {
    ifstream inFile("mails.txt");
    if (!inFile) {
        cerr << "Failed to open the mails file." << endl;
        return;
    }

    vector<string> lines;
    string line;
    bool mailFound = false;

    string mailFrom, mailTo, mailBody, readStatus,mailTitle,mailTime;
    
    while (getline(inFile, line)) {
        istringstream iss(line);
        getline(iss, mailFrom, '/');
        getline(iss, mailTo, '/');
        getline(iss, mailTitle, '/');
        getline(iss, mailTime, '/');
        getline(iss, mailBody, '/');
        getline(iss, readStatus, '/');

        if (mailFrom == mail.from && mailTo == mail.to && mailTitle == mail.title && mailBody == stringToBinary(mail.body) && mailTime == mail.time) {
            mailFound = true;
        }
        else{
            lines.push_back(line);
        }
    }
    inFile.close();

    if (!mailFound) {
        cerr << "Mail not found in the file." << endl;
        return;
    }

    ofstream outFile("mails.txt");
    if (!outFile) {
        cerr << "Failed to open the file for writing." << endl;
        return;
    }

    for (const string& updatedLine : lines) {
        outFile << updatedLine << endl;
    }
    outFile.close();
}

void handleDeleteMail(Player* player, string receivedText){
    vector<string> split = splitInputText(receivedText);
    if ((split[0] == "deletemail") && split.size()==2) {
        vector<Mail> mails = readPlayerMails(player);
        int mail_id = stoi(split[1]);
        if(mails.size() > mail_id){
            deleteMail(mails[mail_id]);
            for (auto it = mails_in_progress.begin(); it != mails_in_progress.end(); ++it) {
                if ((*it).player->user_name == player->user_name) {
                    mails_in_progress.erase(it);
                    break;
                }
            }
            strcat(buf, "Mail deleted.\n");
        }
        else{
            string err = "Enter valid mail id number.\n";
            strcat(buf,err.c_str());
        }

    }
    else{
        strcat(buf, "Invalid usage of deletemail.\n \tdeletemail <msg_num>   # Delete the particular mail\n");
    }
    return;
}

void displayNotification(int client_fd){
    int player_index = findPlayer(client_fd);
    if(player_index != -1){
        Player* player = &players[player_index];
        vector<Mail> mails = readPlayerMails(player);
        int count =0;
        if(mails.size() > 0){
            for (size_t i = 0; i < mails.size(); ++i) {
                if(!mails[i].read){
                    count++;
                }
            }
        }
        if(count>0){
            string notif = "\nYou have " + to_string(count) + " unread message";
            strcat(buf, notif.c_str());
        }
    }
}

// Handle client
bool handleClient(int client_fd) {

    ssize_t n;
    Player* player;
    int player_index = findPlayer(client_fd);
    bool close_connection = false;
    bool override_buf = false;
    bool saveDetails = false;
    now = chrono::system_clock::now();
    current_time = chrono::system_clock::to_time_t(now);
    n = readFromClient(client_fd, MAXBUFLEN);
    if (n < 0) {
        perror("Read error");
        return true;
    } else if (n == 0) {
        cout << "Connection closed by client." << endl;
        return true;
    }
    else{
        // No reading error
        string receivedText = processText(buf);
        memset(buf, 0, sizeof(buf));
        cout << "Received: " << receivedText << endl;
        cout<<"Player found, player index: "<<player_index<<endl;
        if(player_index != -1) {
            player = &players[player_index];
            // printPlayerDetails();
            if(player->valid){
                if ((receivedText == "exit" || receivedText == "quit") && !player->mail_body) {
                    cout << "Client requested to exit. Closing connection." << endl;
                    return true;
                }
                else if(receivedText == "game" && !player->mail_body){
                    handleGames(player,receivedText);
                    
                }
                else if((receivedText == "help" || receivedText == "?") && !player->mail_body){
                    strcat(buf,getHelpText());

                }
                else if(receivedText == "who" && !player->mail_body){
                    strcat(buf,getOnlineUsers());
                }
                else if(receivedText.find("info") != string::npos && !player->mail_body){
                    handleInfo(player,receivedText);
                    saveDetails = true;
                }
                 else if(receivedText.find("stats") != string::npos && !player->mail_body){
                    handleStats(player,receivedText);
                }
                else if(receivedText.find("unblock") != string::npos && !player->mail_body){
                    handleUnBlock(player,receivedText);
                    saveDetails = true;
                }
                else if(receivedText.find("block") != string::npos && !player->mail_body){
                    handleBlock(player,receivedText);
                    saveDetails = true;
                }
                else if(receivedText.find("shout") != string::npos && !player->mail_body){
                    handleShout(player,receivedText);
                }
                else if(receivedText.find("tell") != string::npos && !player->mail_body){
                    handleTell(player,receivedText);
                }
                else if(receivedText.find("match") != string::npos && !player->mail_body){
                    handleMatch(player,receivedText);
                }
                else if(receivedText.find("resign") != string::npos && !player->mail_body){
                    handleResign(player,receivedText);
                }
                else if(receivedText.find("passwd") != string::npos && !player->mail_body){
                    handleChangePassword(player,receivedText);
                }
                else if(receivedText.find("unobserve") != string::npos && !player->mail_body){
                    handleUnObserve(player,receivedText);
                }
                else if(receivedText.find("observe") != string::npos && !player->mail_body){
                    handleObserve(player,receivedText);
                }
                else if(receivedText.find("nonquiet") != string::npos && !player->mail_body){
                    handleNonQuiet(player,receivedText);
                }
                else if(receivedText.find("quiet") != string::npos && !player->mail_body){
                    handleQuiet(player,receivedText);
                }
                else if(receivedText.find("kibitz") != string::npos && !player->mail_body){
                    handleKibitz(player,receivedText);
                }
                else if(receivedText.find("listmail") != string::npos && !player->mail_body){
                    handleListMails(player,receivedText);
                }
                else if(receivedText.find("readmail") != string::npos && !player->mail_body){
                    handleReadMail(player,receivedText);
                }
                else if(receivedText.find("deletemail") != string::npos && !player->mail_body){
                    handleDeleteMail(player,receivedText);
                }
                else if(receivedText.find("mail") != string::npos && !player->mail_body){
                    override_buf = handleMail(player,receivedText);
                }
                else if(receivedText[0] == '\'' && !player->mail_body){
                    handleKibitz(player,receivedText);
                }
                else{
                    if(player->in_game && receivedText.size()==2 && !player->mail_body){
                        handleMove(player,receivedText);
                    }
                    else if(player->mail_body){
                        override_buf = handleMailBody(player,receivedText);
                    }
                    else{
                        strcat(buf,"Invalid option..!");
                    }
                }
            }
            else if (player->user_name.find("guest_") != string::npos){ //handle guest
                vector<string> split = splitInputText(receivedText);
                if (split.size()>0 && split[0] == "register") {
                    if(split.size() == 3){
                        if(registerPlayer(client_fd, split[1], split[2])){
                            strcat(buf,"\nSuccesfully Registered...!");
                        }
                        else{
                            strcat(buf,"\nRegistration Failed...!");
                        }
                    }
                    else{
                        strcat(buf,"Invalid usage, register should have in below format:\n'register <username> <password>'");
                    }
                }
                else if (receivedText == "exit" || receivedText == "quit") {
                    cout << "Client requested to exit. Closing connection." << endl;
                    return true;
                }
                else{
                    strcat(buf,"Invalid option, Guest can only use 'register <username> <password>', 'exit', and 'quit'");
                }
            }
            else{
                if(login(client_fd, receivedText)){
                    strcat(buf,"\nLogin sucess...!");
                    displayNotification(client_fd);
                }
                else{
                     strcat(buf,"\nLogin Failed...!");
                     close_connection = true;
                }
            }
        }
        else if(receivedText.length() == 0){
            int new_player = addPlayer(client_fd, "guest_" + to_string(guest_count));
            player = &players[new_player];
            guest_count++;
            strcat(buf, "Loged in as guest.!\n Guest can only use 'register <username> <password>'");
        }
        else{
            int new_player = addPlayer(client_fd, receivedText);
            player = &players[new_player];
            strcat(buf, "password:");
            override_buf = true;
        }
    }
    if(saveDetails){
        savePlayerDetails(player);
    }
    return writeToClient(client_fd,strlen(buf), player, override_buf) || close_connection;
}

bool handleNewClient(int client_fd){
    memset(buf, 0, sizeof(buf));
    return writeToClient(client_fd,strlen(buf), nullptr, false);
}

int main(int argc, char *argv[]) {
    int serv_sockfd;
    struct sockaddr_in serv_addr, cli_addr;
    socklen_t sock_len;
    ssize_t n;
    fd_set readfds;
    vector<int> client_fds;
    if (argc > 1) {
        port = stoi(argv[1]);
    }

    cout << "port = " << port << endl;
    serv_sockfd = createSocket();

    bzero((void*)&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(port);

    bindSocket(serv_sockfd, serv_addr);
    listenForConnections(serv_sockfd);

    client_fds.push_back(serv_sockfd);

    for (;;) {
        FD_ZERO(&readfds);
        int maxfd = 0;

        // Add all client sockets to readfds
        for (auto fd : client_fds) {
            FD_SET(fd, &readfds);
            maxfd = max(maxfd, fd);
        }

        // Wait for activity on any of the sockets
        if (select(maxfd + 1, &readfds, NULL, NULL, NULL) == -1) {
            perror("Select error");
            close(serv_sockfd);
            exit(EXIT_FAILURE);
        }

        // New connection
        if (FD_ISSET(serv_sockfd, &readfds)) {
            sock_len = sizeof(cli_addr);
            int client_fd = acceptConnection(serv_sockfd, cli_addr, sock_len);
            cout<<"New client FD: "<<client_fd<<endl;
            cout<< "New connection accepted from " << inet_ntoa(cli_addr.sin_addr) << ":" << ntohs(cli_addr.sin_port) << endl;
            if(handleNewClient(client_fd)){
                handleClose(client_fd);
            }
            else{
                client_fds.push_back(client_fd);
            }
        }
        // Handle all active client sockets
        for (auto it = next(client_fds.begin()); it != client_fds.end();) {
            int fd = *it;
            if (FD_ISSET(fd, &readfds)) {
                cout<<"fd"<<fd<<endl;
                memset(buf, 0, sizeof(buf));
                bool closeConnection = handleClient(fd);
                if(closeConnection){
                    handleClose(fd);
					it = client_fds.erase(it);
                    continue;
                }
                ++it;
            }
            else{
                ++it;
            }
            try{
                memset(buf, 0, sizeof(buf));
                validateGames();
            }
            catch(const exception& e) {
                cout << "continue "<< endl;
            }
        }

        for (auto it = next(client_fds.begin()); it != client_fds.end();) {
            int fd = *it;
            int player_index = findPlayer(fd);
            if(player_index != -1) {
                Player* player = &players[player_index];
                if(player->close_session) {
                    handleClose(fd);
                    it = client_fds.erase(it);
                    continue;
                }
                ++it;
            }
            else{
                ++it;
            }
        }
    }

    // Close the listening socket
    close(serv_sockfd);

    return 0;
}

