# Tic-Tac-Toe Game Server

This project under `./server` develops a network-based Tic-Tac-Toe game server, enabling users to play Tic-Tac-Toe online. The server supports multiple clients simultaneously and provides functionalities such as user registration and sign-in, game observation, user-to-user messaging, and basic player statistics.

## Features

- **User registration and sign-in**
- **Game observation** for ongoing matches
- **In-game messaging** between users
- **Player statistics** tracking

This guide outlines the steps for compiling, launching the server, and demonstrating its capabilities.

## Execution

### Prerequisites

- Download the `.tar` file containing the project.
- Ensure remote servers are accessible via SSH.

### Steps

1. **Extract the `.tar` file**:
   ```bash
   tar -xf <FileName>.tar
   ```

2. **Compile the project**:
   ```bash
   make
   ```

3. **Run the server**:
   ```bash
   ./server <port_number>
   ```
   Replace `<port_number>` with the desired port number (e.g., `55551`).

4. **Clean the files**:
   ```bash
   make clean
   ```
   This removes executable files (`a.out`), object files (`*.o`), backup files (`*~`), and core dump files (`core.*`).

## Usage

The `./server` supports the following commands:

1. **who** – List all online players:
   ```bash
   who
   ```

2. **stats [name]** – Display statistics for the specified player or all players if no name is given:
   ```bash
   stats <user_id>
   stats
   ```

3. **game** – Display the total number of active games (0-indexed):
   ```bash
   game
   ```

4. **observe <game_num>** – Watch an ongoing game by its ID:
   ```bash
   observe <game_num>
   ```

5. **unobserve** – Stop observing a game:
   ```bash
   unobserve
   ```

6. **match <name> <b|w> [t]** – Challenge another player:
   ```bash
   match <name> <b|w> [t]
   ```

7. **<A|B|C><1|2|3>** – Make a move in the game:
   ```bash
   A1
   ```

8. **resign** – Forfeit the current game:
   ```bash
   resign
   ```

9. **refresh** – Refresh the current game status:
   ```bash
   refresh
   ```

10. **shout <msg>** – Broadcast a message to all players:
    ```bash
    shout <msg>
    ```

11. **tell <name> <msg>** – Send a private message to another player:
    ```bash
    tell <name> <msg>
    ```

12. **kibitz <msg>** – Send a message visible only to game observers:
    ```bash
    kibitz <msg>
    ```

13. **' <msg>** – Same as the `kibitz` command:
    ```bash
    ' <msg>
    ```

14. **quiet** – Disable all broadcast messages:
    ```bash
    quiet
    ```

15. **nonquiet** – Enable broadcast messages:
    ```bash
    nonquiet
    ```

16. **block <id>** – Block messages from a specific user:
    ```bash
    block <id>
    ```

17. **unblock <id>** – Unblock a previously blocked user:
    ```bash
    unblock <id>
    ```

18. **listmail** – List all mail in the inbox:
    ```bash
    listmail
    ```

19. **readmail <msg_num>** – Read a specific mail message:
    ```bash
    readmail <msg_num>
    ```

20. **deletemail <msg_num>** – Delete a specific mail message:
    ```bash
    deletemail <msg_num>
    ```

21. **mail <id> <title>** – Send a mail message to another user:
    ```bash
    mail <id> <title>
    ```

22. **info <msg>** – Update user information:
    ```bash
    info <msg>
    ```

23. **passwd <new>** – Change your password:
    ```bash
    passwd <new>
    ```

24. **exit** – Exit the server:
    ```bash
    exit
    ```

25. **quit** – Exit the server:
    ```bash
    quit
    ```

26. **help** – Display available commands:
    ```bash
    help
    ```

27. **?** – Display available commands (same as `help`):
    ```bash
    ?
    ```

## Additional Features

- **Blocking**: If a user blocks another, the blocked user cannot send messages to the blocker, but the blocker can still send messages to the blocked user.
- **Mail Composition**: Use `.` on a new line to end a mail message body.
- **Unread Mail Notification**: Users are notified of unread mail upon login.
- **Default Game Parameters**: If a match is initiated without all parameters, the default settings are `'w'` for color and `600` seconds for game time.
- **Time Limit**: If a player exceeds the game time, they automatically lose the match.
- **Guest Registration**: Successful guest registrations automatically log the user into the server.

## Notes

- Extra or unexpected options/arguments will result in an invalid option error.
- If the chosen port does not work, try using a different port number during execution:
   ```bash
   ./server <another_port_number>
   ```
