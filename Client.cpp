#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <cstring>
#include <vector>
#include <sstream>
#include <algorithm>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <mutex>
#include <queue>
#include <condition_variable>

#define PORT 2908
#define BUFFER_SIZE 1024

std::mutex coutMutex;
std::queue<std::string> messageQueue;
std::mutex queueMutex;
std::condition_variable queueCondVar;
int serverSocket;
bool isConnected = false;
bool isAuthenticated = false;
std::string username;

void trim(std::string& s)
{
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](int ch)
    {
        return !isspace(ch);
    }));
    s.erase(std::find_if(s.rbegin(), s.rend(), [](int ch)
    {
        return !isspace(ch);
    }).base(), s.end());
}

std::vector<std::string> splitString(const std::string& str, char delimiter)
{
    std::vector<std::string> tokens;
    std::stringstream ss(str);
    std::string token;
    while (getline(ss, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}

bool connect_to_server(const std::string& serverIP)
{
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT);
    inet_aton(serverIP.c_str(), &serverAddr.sin_addr);

    if (connect(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == -1)
    {
        std::cerr << "Error connecting to server\n";
        close(serverSocket);
        return false;
    }

    isConnected = true;
    return true;
}

bool send_server(const std::string& message)
{
    if (send(serverSocket, message.c_str(), message.size(), 0) == -1)
    {
        std::cerr << "Error sending message to server\n";
        return false;
    }
    return true;
}

std::string receiveFromServer()
{
    std::string message;
    char buffer[BUFFER_SIZE];
    while (true)
    {
        memset(buffer, 0, sizeof(buffer));
        int bytesReceived = recv(serverSocket, buffer, sizeof(buffer) - 1, 0);
        if (bytesReceived <= 0)
        {
            isConnected = false;
            close(serverSocket);
            return "";
        }
        buffer[bytesReceived] = '\0';
        message += buffer;
        if (message.find('\n') != std::string::npos)
        {
            break;
        }
    }
    trim(message);
    return message;
}

void handleServerMessages()
{
    while (isConnected)
    {
        std::string msg = receiveFromServer();
        if (msg.empty())
        {
            break;
        }
        auto tokens = splitString(msg, '|');
        if (tokens.empty()) continue;

        if (tokens[0] == "NOTIF" || tokens[0] == "SHARED_NOTIF")
        {
            std::lock_guard<std::mutex> lock(coutMutex);
            if (tokens[0] == "NOTIF")
            {
                std::cout << "\n*** Notification: " << tokens[1] << " ***\n";
            }
            else if (tokens[0] == "SHARED_NOTIF" && tokens.size() >= 5)
            {
                std::cout << "\n!!! Shared Notification !!!\n";
                std::cout << "From: " << tokens[1] << "\n";
                std::cout << "Action: " << tokens[2] << "\n";
                std::cout << "Trigger Time: " << tokens[3] << "\n";
                std::cout << "Shared With: " << tokens[4] << "\n";
            }
            std::cout << "> " << std::flush;
        }
        else if (tokens[0] == "PREDEF" && tokens.size() == 4)
        {
            std::lock_guard<std::mutex> lock(coutMutex);
            std::cout << "\n*** Predefined Notification ***\n";
            std::cout << "Name: " << tokens[1] << "\n";
            std::cout << "Action: " << tokens[2] << "\n";
            std::cout << "Can Share?: " << tokens[3] << "\n";
            std::cout << "> " << std::flush;
        }
        else
        {
            std::cout << msg << "\n";

            std::unique_lock<std::mutex> lock(queueMutex);
            messageQueue.push(msg);
            lock.unlock();
            queueCondVar.notify_one();
        }
    }
}

std::string waitForResponse(const std::string& expectedCommand)
{
    std::unique_lock<std::mutex> lock(queueMutex);
    queueCondVar.wait(lock, []{ return !messageQueue.empty() || !isConnected; });
    if (!isConnected) return "";

    std::string message = messageQueue.front();
    messageQueue.pop();
    lock.unlock();

    auto tokens = splitString(message, '|');
    if (tokens.empty() || tokens[0] != expectedCommand)
    {
        return "";
    }
    return message;
}

void authenticate()
{
    std::cout << "Enter username: ";
    std::getline(std::cin, username);
    std::cout << "Enter password: ";
    std::string password;
    std::getline(std::cin, password);

    std::string authMessage = "AUTH|" + username + "|" + password + "\n";
    if (!send_server(authMessage))
    {
        return;
    }
    std::string resp = waitForResponse("OK");
    if (resp.empty())
    {
        return;
    }
    auto tokens = splitString(resp, '|');
    if (tokens[0] == "OK")
    {
        isAuthenticated = true;
        std::cout << "Authenticated successfully!\n";
    }
}

void registerUser()
{
    std::cout << "Enter username: ";
    std::string newUsername;
    std::getline(std::cin, newUsername);
    std::cout << "Enter password: ";
    std::string newPassword;
    std::getline(std::cin, newPassword);

    std::string msg = "REGISTER|" + newUsername + "|" + newPassword + "\n";
    if (!send_server(msg))
    {
        return;
    }
    std::string resp = waitForResponse("OK");
    if (resp.empty())
    {
        return;
    }
    auto tokens = splitString(resp, '|');
    if (tokens[0] == "OK")
    {
        std::cout << "Registration successful!\n";
    }
}

void getPredefinedList()
{
    if (!send_server("GET_PREDEF\n"))
    {
        return;
    }
    std::string done = waitForResponse("OK"); 

}

void addNotification()
{
    std::cout << "Enter short name for new notification: ";
    std::string name;
    std::getline(std::cin, name);

    std::cout << "Enter the action text: ";
    std::string action;
    std::getline(std::cin, action);

    std::cout << "Can share? (YES/NO): ";
    std::string canShare;
    std::getline(std::cin, canShare);

    std::string msg = "ADD_PREDEF|" + name + "|" + action + "|" + canShare + "\n";
    if (!send_server(msg))
    {
        return;
    }
    std::string resp = waitForResponse("OK"); 
    if (resp.empty())
    {
        std::cout << "Eroare sau nu a dat raspuns.\n";
    } else
    {
        std::cout << "Predefined notification created successfully!\n";
    }
}

/*3*/void useNotification()
{
    std::cout << "Enter the name of the predefined to use: ";
    std::string predefName;
    std::getline(std::cin, predefName);

    std::cout << "Enter trigger time (YYYY-MM-DD HH:MM:SS): ";
    std::string triggerTime;
    std::getline(std::cin, triggerTime);

    std::cout << "Enter usernames to share with (comma-separated), or leave blank: ";
    std::string sharedWith;
    std::getline(std::cin, sharedWith);
    if (sharedWith.empty())
    {
        sharedWith = username;
    }
    std::string cmd = "USE_PREDEF|" + predefName + "|" + triggerTime + "|" + sharedWith + "\n";
    if (!send_server(cmd))
    {
        return;
    }
    std::string resp = waitForResponse("OK");
    if (resp.empty())
    {
        std::cout << "Eroare la schedule.\n";
    } else
    {
        std::cout << "Predefined notification scheduled successfully!\n";
    }
}

void getSharedNotifications()
{
    if (!send_server("GET_SHARED_NOTIFS\n"))
    {
        return;
    }
}

/*1*/void mainMenu()
{
    while (isConnected && isAuthenticated)
    {
        std::cout << "\n***** Main Menu *****\n";
        std::cout << "1. Get predefined list\n";
        std::cout << "2. Add a custom notification\n";
        std::cout << "3. Use a predefined notification\n";
        std::cout << "4. Get shared notifications\n";
        std::cout << "5. Logout\n";
        std::cout << "Enter choice: ";

        std::string choice;
        std::getline(std::cin, choice);

        if (choice == "1")
        {
            getPredefinedList();
        }
        else if (choice == "2")
        {
            addNotification();
        }
        else if (choice == "3")
        {
            useNotification();
        }
        else if (choice == "4")
        {
            getSharedNotifications();
        }
        else if (choice == "5")
        {
            send_server("LOGOUT\n");
            isAuthenticated = false;
            isConnected = false;
            break;
        }
        else
        {
            std::cout << "Invalid choice.\n";
        }
    }
}

int main()
{
    std::string serverIP = "127.0.0.1";
    if (!connect_to_server(serverIP))
    {
        std::cerr << "Failed to connect to server.\n";
        return -1;
    }
    /*2*/std::thread messageThread(handleServerMessages);

    /*1*/while (!isAuthenticated && isConnected)
    {
        std::cout << "\n--- Welcome to NotyHelper ---\n";
        std::cout << "1. Login\n";
        std::cout << "2. Register\n";
        std::cout << "3. Exit\n";
        std::cout << "Enter choice: ";

        std::string choice;
        std::getline(std::cin, choice);
        if (choice == "1")
        {
            authenticate();
            if (isAuthenticated) break;
        }
        else if (choice == "2")
        {
            registerUser();
        }
        else if (choice == "3")
        {
            isConnected = false;
            break;
        }
        else
        {
            std::cout << "Choose a correct number\n";
        }
    }

    if (isAuthenticated)
    {
        mainMenu();
    }

    shutdown(serverSocket, SHUT_RDWR);
    close(serverSocket);
    isConnected = false;

    if (messageThread.joinable())
    {
        messageThread.join();
    }
    std::cout << "Client exited\n";
    return 0;
}