import socket
import tkinter as tk
from tkinter import ttk
import threading
import time
import math

game_buttons = ["A1", "A2", "A3", "B1", "B2", "B3", "C1", "C2", "C3"]
host = 'localhost'
port = 55551
respose= ""
login = False
login_message = "Login sucess"""
client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
root = tk.Tk()
user_name  = ""

def sendMessage(message):
    try:
        message_s = message + "  "
        client_socket.settimeout(1)
        client_socket.send(message_s.encode())
        response = client_socket.recv(1024)
        print(response.decode(),end="")
        x = response.decode()
        split = x.split('\n')
        split = split[:len(split)-1]
        res = ""
        for line in split:
            if(line.find('<')==-1 or line.find("match")):
                res = res + line + '\n'
        return res
    except socket.timeout:
        return "command Failed\n"


def recvMessage():
    try:
        client_socket.settimeout(1)
        response = client_socket.recv(1024)
        print(response.decode(),end="")
        x = response.decode()
        split = x.split('\n')
        split = split[:len(split)-1]
        res = ""
        for line in split:
            if(line.find('<')==-1 or line.find("match")):
                res = res + line + '\n'
        game(res)
        return res
    except socket.timeout:
        return "No new Messages\n"

def game(text):
    if(text.find("Game Board:")!=-1):
        lines = text.split('\n')
        for line in lines:
            if(len(line)>0):
                if(line[0] == "A"):
                    lines_A = line[1:]
                if(line[0] == "B"):
                    lines_B = line[1:]
                if(line[0] == "C"):
                    lines_C = line[1:]
        if(len(lines_A) > 0  and len(lines_B) and len(lines_C)):
            new_list = lines_A.split() + lines_B.split() + lines_C.split()
            for i in range(len(game_buttons)):
                if(new_list[i] == "O"):
                    game_buttons[i] = "O"
                if(new_list[i] == "X"):
                    game_buttons[i] = "X"
            print(new_list)
        print(game_buttons)


def function_who():
    response = sendMessage("who")
    if(len(response)>0):
        response_text.insert(tk.END, response + "\n")

def function_stats():
    response = sendMessage("stats")
    if(len(response)>0):
        response_text.insert(tk.END, response + "\n")

def function_game():
    response = sendMessage("game")
    if(len(response)>0):
        response_text.insert(tk.END, response + "\n")
        game(response)

def function_exit():
    response = sendMessage("exit")
    if(len(response)>0):
        response_text.insert(tk.END, response + "\n")
    closeConnection()
    exit()

def function_match():
    user_id = user_id_entry.get()
    color = color_var.get()
    time = time_entry.get()
    response_text.insert(tk.END, f"MATCH: User ID={user_id}, Color={color}, Time={time}\n")
    response = sendMessage("match " + user_id + " " + color + " " + time)
    if(len(response)>0):
        response_text.insert(tk.END, response + "\n")

def function_resign():
    response = sendMessage("resign")
    if(len(response)>0):
        response_text.insert(tk.END, response + "\n")

def function_refresh():
    response = recvMessage()
    if(len(response)>0):
        response_text.insert(tk.END, response + "\n")

def function_game_button(id):
    row = id // 3
    col = id % 3

    move = ""
    if(row == 0):
        move += "A"
    elif(row == 1):
        move += "B"
    else:
        move += "C"
    if(col == 0):
        move += "1"
    elif(col == 1):
        move += "2"
    else:
        move += "3"
    print(move)
    response = sendMessage(move)
    if(len(response)>0):
        response_text.insert(tk.END, response + "\n")
        game(response)



def render_gui():
    # Create the main window
    root.title("GUI Window For User : " + user_name)
    root.geometry("800x600")

    # Create Section 1: Options
    section1_frame = ttk.Frame(root, padding=(10, 10, 10, 10), borderwidth=2, relief="groove")
    section1_frame.grid(row=0, column=0, padx=10, pady=10, sticky="nsew")

    # Heading for Section 1
    options_heading = ttk.Label(section1_frame, text="Options", font=("Helvetica", 14, "bold"))
    options_heading.grid(row=0, column=0, columnspan=2, pady=10)

    # Define button names and functions
    button_names = ["who", "stats", "game", "match", "resign", "exit", "refresh"]
    button_functions = [function_who, function_stats, function_game, function_match, function_resign, function_exit, function_refresh]

    # Create buttons and input boxes
    for i, button_name in enumerate(button_names):
        # Create a frame for each button and input box
        button_frame = ttk.Frame(section1_frame)
        button_frame.grid(row=i+1, column=0, pady=5, sticky="w")

        # Create button
        button = ttk.Button(button_frame, text=button_name.capitalize(), command=button_functions[i], width=15)
        button.pack(side=tk.LEFT)

        # Create input box if required
        if button_name in ["observe", "match", "shout"]:
            if button_name == "match":
                global user_id_entry
                user_id_entry = ttk.Entry(button_frame)
                user_id_entry.pack(side=tk.LEFT)

                global color_var
                color_var = tk.StringVar(root)
                color_var.set("b")  # Set default value to "b"
                color_menu = tk.OptionMenu(button_frame, color_var, "b", "w")
                color_menu.pack(side=tk.LEFT, padx=5)

                global time_entry
                time_entry = ttk.Entry(button_frame)
                time_entry.pack(side=tk.LEFT)

    # Create Section 2: Game Section
    game_section_frame = ttk.Frame(root, padding=(10, 10, 10, 10), borderwidth=2, relief="groove")
    game_section_frame.grid(row=0, column=1, padx=10, pady=10, sticky="nsew")

    # Heading for Section 2
    game_heading = ttk.Label(game_section_frame, text="Game", font=("Helvetica", 14, "bold"))
    game_heading.grid(row=0, column=0, columnspan=3, pady=10)

    # Create buttons for Tic Tac Toe game
    for i in range(9):
        row = i // 3
        col = i % 3
        button_id = i
        button = ttk.Button(game_section_frame, text=game_buttons[i], width=5, command=lambda id=button_id: function_game_button(id))
        button.grid(row=row+1, column=col, padx=5, pady=5)

    # Create Section 3: Response Area
    section3_frame = ttk.Frame(root, padding=(10, 10, 10, 10), borderwidth=2, relief="groove")
    section3_frame.grid(row=1, column=0, columnspan=2, padx=10, pady=10, sticky="nsew")

    # Heading for Section 3
    response_heading = ttk.Label(section3_frame, text="Response", font=("Helvetica", 14, "bold"))
    response_heading.pack(pady=10)

    global response_text
    response_text = tk.Text(section3_frame, wrap=tk.WORD)
    response_text.pack(fill=tk.BOTH, expand=True)

    # Run the main event loop
    root.mainloop()


def closeConnection():
    client_socket.close()
    root.destroy()

try:
    client_socket.connect((host, port))
    print(f"Connected to {host}:{port}")
    response = client_socket.recv(1024)
    print(response.decode(),end="")
    # username
    message_s = input()
    user_name = message_s
    message_s = message_s + "  "
    client_socket.send(message_s.encode())
    response = client_socket.recv(1024)
    print(response.decode(),end="")
    # Password
    message_s = input()
    message_s = message_s + "  "
    client_socket.send(message_s.encode())
    response = client_socket.recv(1024)
    print(response.decode(),end="")
    res = str(response)
    if((res.find(login_message) != -1) and (not login)):
        login = True
        render_gui()
except ConnectionRefusedError:
    print("Connection refused. Make sure the server is running and accessible.")
