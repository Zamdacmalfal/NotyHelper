#!/bin/bash

# Compilation commands
g++ -std=c++11 -pthread -o client Client.cpp
g++ -std=c++11 -pthread -o server Server.cpp

# Check if compilation was successful
if [ ! -f "client" ] || [ ! -f "server" ]; then
    echo "Compilation failed. Check for errors."
    exit 1
fi

echo "Compilation successful."

# Ensure necessary files exist
touch users.txt notifications.txt predefined.txt

echo "Checked and ensured existence of users.txt, notifications.txt, and predefined.txt."

