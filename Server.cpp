#include <iostream>
#include <fstream>
#include <string>
#include <thread>
#include <vector>
#include <map>
#include <chrono>
#include <ctime>
#include <cstring>
#include <sstream>
#include <algorithm>
#include <mutex>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 2908

std::mutex coutMutex; 
std::map<int, std::string> clientUserMap;
std::vector<int> clientSockets;
std::mutex clientSocketsMutex; 

void sendToClient(int clientSocket, const std::string& message)
{
    send(clientSocket, message.c_str(), message.size(), 0);
}

std::vector<std::string> splitString(const std::string& str, char delimiter)
{
    std::vector<std::string> tokens;
    std::stringstream ss(str);
    std::string token;
    while (getline(ss, token, delimiter))
    {
        tokens.push_back(token);
    }
    return tokens;
}

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
bool authenticateUser(const std::string& username, const std::string& password)
{
    std::ifstream userFile("users.txt");
    if (!userFile.is_open())
    {
        return false;
    }
    std::string line;
    while (std::getline(userFile, line))
    {
        trim(line);
        size_t delimPos = line.find(':');
        if (delimPos != std::string::npos)
        {
            std::string fileUsername = line.substr(0, delimPos);
            std::string filePassword = line.substr(delimPos + 1);
            if (fileUsername == username && filePassword == password)
            {
                userFile.close();
                return true;
            }
        }
    }
    userFile.close();
    return false;
}

bool registerUser(const std::string& username, const std::string& password)
{
    if (authenticateUser(username, password))
    {
        return false;
    }

    std::ofstream userFile("users.txt", std::ios::app);
    userFile << username << ":" << password << "\n";
    userFile.close();
    return true;
}

bool addNotification(const std::string& username, const std::string& action, const std::string& triggerTime, const std::string& sharedWith)
{
    std::ofstream notifFile("notifications.txt", std::ios::app);
    if (!notifFile.is_open())
    {
        return false;
    }
    notifFile << username << "|" << action << "|" << triggerTime << "|" << sharedWith << "\n";
    notifFile.close();
    return true;
}

std::vector<std::string> getSharedNotifications(const std::string& username)
{
    std::vector<std::string> sharedNotifs;
    std::ifstream notifFile("notifications.txt");
    if (!notifFile.is_open())
    {
        return sharedNotifs;
    }

    std::string line;
    while (std::getline(notifFile, line))
    {
        trim(line);
        auto tokens = splitString(line, '|');
        if (tokens.size() == 4)
        {
            std::string sharedWith = tokens[3];
            auto sharedUsers = splitString(sharedWith, ',');
            if (std::find(sharedUsers.begin(), sharedUsers.end(), username) != sharedUsers.end())
            {
                sharedNotifs.push_back(line);
            }
        }
    }
    notifFile.close();
    return sharedNotifs;
}

std::vector<std::array<std::string,3>> loadPredefined()
{
    std::vector<std::array<std::string,3>> result;
    std::ifstream in("predefined.txt");
    if (!in.is_open()) return result;

    std::string line;
    while (std::getline(in, line))
    {
        trim(line);
        auto tokens = splitString(line, '|');
        if (tokens.size() == 3)
        {
            result.push_back({ tokens[0], tokens[1], tokens[2] });
        }
    }
    in.close();
    return result;
}

bool addPredefined(const std::string& name, const std::string& action, const std::string& canShare)
{
    std::ofstream out("predefined.txt", std::ios::app);
    if (!out.is_open())
    {
        return false;
    }
    out << name << "|" << action << "|" << canShare << "\n";
    out.close();
    return true;
}

void clientHandler(int clientSocket)
{
    std::cout << "Client handler online socket: " << clientSocket << std::endl;
    char buffer[1024];
    std::string username;

    while (true)
    {
        memset(buffer, 0, sizeof(buffer));
        int bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
        if (bytesReceived <= 0)
        {
            std::cout << "Client disconnected: socket " << clientSocket << std::endl;
            break;
        }
        buffer[bytesReceived] = '\0';
        std::string request(buffer);
        trim(request);
        auto tokens = splitString(request, '|');
        for(auto& token : tokens)
        {
            trim(token);
        }
        if (tokens.empty())
        {
            sendToClient(clientSocket, "ERROR|Comanda invalida\n");
            continue;
        }

        if (tokens[0] == "AUTH")
        {
            if (tokens.size() != 3)
            {
                sendToClient(clientSocket, "ERROR|Format autentificare invalid\n");
                continue;
            }
            if (authenticateUser(tokens[1], tokens[2]))
            {
                username = tokens[1];
                {
                    std::lock_guard<std::mutex> lock(clientSocketsMutex);
                    clientUserMap[clientSocket] = username;
                    clientSockets.push_back(clientSocket);
                }
                sendToClient(clientSocket, "OK|Autentificare reusita\n");
            } else
            {
                sendToClient(clientSocket, "ERROR|Autentificare esuata\n");
            }
        }
        else if (tokens[0] == "REGISTER")
        {
            if (tokens.size() != 3)
            {
                sendToClient(clientSocket, "ERROR|Format inregistrare invalid\n");
                continue;
            }
            if (registerUser(tokens[1], tokens[2]))
            {
                sendToClient(clientSocket, "OK|inregistrare reusita\n");
            } else
            {
                sendToClient(clientSocket, "ERROR|inregistrare esuata\n");
            }
        }
        else if (tokens[0] == "GET_PREDEF")
        {
            auto predefs = loadPredefined();
            for (auto &p : predefs)
            {
                std::string msg = p[0] + "\n";
                sendToClient(clientSocket, msg);
            }
        }
        else if (tokens[0] == "ADD_PREDEF")
        {
            if (username.empty())
            {
                sendToClient(clientSocket, "ERROR|Trebuie sa va autentificați\n");
                continue;
            }
            if (tokens.size() != 4)
            {
                sendToClient(clientSocket, "ERROR|Format ADD_PREDEF invalid\n");
                continue;
            }
            if (tokens[3] != "YES" && tokens[3] != "NO")
            {
                sendToClient(clientSocket, "ERROR|awnser must be YES or NO\n");
                continue;
            }
            if (addPredefined(tokens[1], tokens[2], tokens[3]))
            {
                sendToClient(clientSocket, "OK|Notificare creata\n");
            } else
            {
                sendToClient(clientSocket, "ERROR|Eroare la crearea notificarii\n");
            }
        }
        else if (tokens[0] == "USE_PREDEF")
        {
            if (username.empty())
            {
                sendToClient(clientSocket, "ERROR|Trebuie sa va autentificați\n");
                continue;
            }
            if (tokens.size() != 4)
            {
                sendToClient(clientSocket, "ERROR|Format USE_PREDEF invalid\n");
                continue;
            }
            auto predefs = loadPredefined();
            bool found = false;
            std::string action;
            std::string canShare;
            for (auto &p : predefs)
            {
                if (p[0] == tokens[1])
                {
                    found = true;
                    action = p[1];
                    canShare = p[2];
                    break;
                }
            }
            if (!found)
            {
                sendToClient(clientSocket, "ERROR|Notificare inexistenta\n");
                continue;
            }
            if (canShare == "NO")
            {
                tokens[3] = username;
            }
            if (addNotification(username, action, tokens[2], tokens[3]))
            {
                sendToClient(clientSocket, "OK|Notificare adaugata\n");
            } else
            {
                sendToClient(clientSocket, "ERROR|Eroare la adaugare notificare\n");
            }
        }
        else if (tokens[0] == "GET_SHARED_NOTIFS")
        {
            if (username.empty())
            {
                sendToClient(clientSocket, "ERROR|Trebuie sa te autentifici\n");
                continue;
            }
            auto sharedNotifs = getSharedNotifications(username);
            for (const auto& notif : sharedNotifs)
            {
                sendToClient(clientSocket, "SHARED_NOTIF|" + notif + "\n");
            }
            sendToClient(clientSocket, "OK|Sfarsit notificari partajate\n");
        }
        else if (tokens[0] == "LOGOUT")
        {
            sendToClient(clientSocket, "OK|Deconectare reusita \n");
        }
        else
        {
            sendToClient(clientSocket, "ERROR|Comanda necunoscuta\n");
        }
    }
    {
        std::lock_guard<std::mutex> lock(clientSocketsMutex);
        clientSockets.erase(std::remove(clientSockets.begin(), clientSockets.end(), clientSocket), clientSockets.end());
        clientUserMap.erase(clientSocket);
    }
    close(clientSocket);
    std::cout << "Client handler ending for socket: " << clientSocket << std::endl;
}

void notificationDispatcher()
{
    while (true)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));

        std::ifstream notifFile("notifications.txt");
        if (!notifFile.is_open())
        {
            continue;
        }

        std::string line;
        std::vector<std::string> remainingNotifs;
        while (std::getline(notifFile, line))
        {
            trim(line);
            auto tokens = splitString(line, '|');
            if (tokens.size() == 4) {
                std::string username   = tokens[0];
                std::string action     = tokens[1];
                std::string triggerTime = tokens[2];
                std::string sharedWith = tokens[3];
                std::time_t now = std::time(nullptr);
                std::tm tmTrigger = {};
                tmTrigger.tm_isdst = -1;
                std::istringstream ss(triggerTime);
                ss >> std::get_time(&tmTrigger, "%Y-%m-%d %H:%M:%S");
                if(ss.fail())
                {
                    std::cerr << "Fail la parsare timp " << triggerTime << std::endl;
                    remainingNotifs.push_back(line);
                    continue;
                }
                std::time_t triggerEpoch = mktime(&tmTrigger);
                if (triggerEpoch == -1)
                {
                    std::cerr << "Invalid trigger time: " << triggerTime << std::endl;
                    remainingNotifs.push_back(line);
                    continue;
                }

                if (difftime(now, triggerEpoch) >= 0)
                {
                {
                std::lock_guard<std::mutex> lock(clientSocketsMutex);

                bool foundOne = false;
                for (const auto& pair : clientUserMap)
                {
                if (pair.second == username)
                {
                sendToClient(pair.first, "NOTIF|" + action + "\n");
                foundOne = true;
                }
                }
                }
                }
                else
                {
                    remainingNotifs.push_back(line);
                }
            }
        }
        notifFile.close();

        std::ofstream notifFileOut("notifications.txt", std::ios::trunc);
        for (const auto& notifLine : remainingNotifs)
        {
            notifFileOut << notifLine << "\n";
        }
        notifFileOut.close();
    }
}

int main()
{
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == -1)
    {
        std::cerr << "Eroare la crearea socket-ului server\n";
        return -1;
    }

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT);
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    memset(&(serverAddr.sin_zero), 0, 8);

    if (bind(serverSocket, (sockaddr*)&serverAddr, sizeof(sockaddr)) == -1)
    {
        std::cerr << "Eroare la bind\n";
        return -1;
    }

    if (listen(serverSocket, SOMAXCONN) == -1)
    {
        std::cerr << "Eroare la listen\n";
        return -1;
    }

    std::cout << "Serverul ruleaza pe portul " << PORT << "\n";

    std::thread dispatcherThread(notificationDispatcher);
    dispatcherThread.detach();

    while (true)
    {
        sockaddr_in clientAddr;
        socklen_t clientSize = sizeof(sockaddr_in);
        int clientSocket = accept(serverSocket, (sockaddr*)&clientAddr, &clientSize);
        if (clientSocket == -1)
        {
            std::cerr << "Eroare la accept\n";
            continue;
        }

        char clientIP[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(clientAddr.sin_addr), clientIP, INET_ADDRSTRLEN);
        {
            std::lock_guard<std::mutex> lock(coutMutex);
            std::cout << "Client conectat: " << clientIP << ":" << ntohs(clientAddr.sin_port) << "\n";
        }

        std::thread(clientHandler, clientSocket).detach();
    }

    close(serverSocket);
    return 0;
}