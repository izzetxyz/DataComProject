#include <iostream>
#include <map>
#include <vector>
#include <string>
#include <sstream>
#include <chrono>
#include <ctime>
#include <fstream>
#include <unistd.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <thread>
#include <sys/stat.h>
#define PORT_NO 12345
#define BUFFER_SIZE 1024
#include <vector>
#include <string>

std::vector<std::string> split(const std::string &str, char delimiter)
{
  std::vector<std::string> parts;
  std::string currentPart;
  for (const auto &c : str)
  {
    if (c == delimiter)
    {
      parts.push_back(currentPart);
      currentPart.clear();
    }
    else
    {
      currentPart += c;
    }
  }
  parts.push_back(currentPart);
  return parts;
}

std::map<std::string, int> clients; // Map to store client names and sockets

// Function to handle incoming messages from server
void receiveChat(int socket)
{
  char buffer[BUFFER_SIZE];
  while (true)
  {
    bzero(buffer, BUFFER_SIZE);
    int n = read(socket, buffer, BUFFER_SIZE - 1);
    if (n < 0)
    {
      perror("ERROR reading from socket");
      exit(1);
    }

    std::string message(buffer);
    std::vector<std::string> parts = split(message, '|');
    std::string command = parts[0];

    if (command == "MESG")
    {
      // Check parity of received message
      int parity = std::stoi(parts[1]);
      int actualParity = 0;
      for (int i = 2; i < parts.size(); i++)
      {
        for (const auto &c : parts[i])
        {
          actualParity += c == '1';
        }
      }
      actualParity %= 2;

      if (parity != actualParity)
      {
        // Message was corrupted, request resend
        std::string sender = parts[2];
        std::string errorBits = parts[1];
        std::string actualMessage;
        for (int i = 3; i < parts.size(); i++)
        {
          actualMessage += parts[i];
          if (i < parts.size() - 1)
          {
            actualMessage += "|";
          }
        }

        bzero(buffer, BUFFER_SIZE);
        std::string sendMessage = "MERR|" + sender + "|" + errorBits + "|" + actualMessage;
        strcpy(buffer, sendMessage.c_str());
        n = write(socket, buffer, strlen(buffer));
        if (n < 0)
        {
          perror("ERROR writing to socket");
          exit(1);
        }
      }
      else
      {
        // Message was not corrupted, display it to the user
        std::string sender = parts[2];
        std::string actualMessage;
        for (int i = 3; i < parts.size(); i++)
        {
          actualMessage += parts[i];
          if (i < parts.size() - 1)
          {
            actualMessage += "|";
          }
        }
        std::cout << sender << ": " << actualMessage << std::endl;
      }
    }
  }
}
int main()
{
  int serverSocket;
  socklen_t clientLen;
  struct sockaddr_in serverAddress;
  struct hostent *server;

  // Get user's chat name
  std::string name;
  std::cout << "Enter your chat name: ";
  std::getline(std::cin, name);

  // Create log directory if it doesn't exist
  std::string logDir = "logs";
  if (mkdir(logDir.c_str(), 0700) == -1)
  {
    if (errno != EEXIST)
    {
      perror("ERROR creating log directory");
      exit(1);
    }
  }

  // Open log file
  std::time_t t = std::time(nullptr);
  char timeString[100];
  strftime(timeString, sizeof(timeString), "%Y-%m-%d_%H-%M-%S", std::localtime(&t));
  std::string logFileName = logDir + "/" + name + "_" + timeString + ".txt";
  std::ofstream logFile(logFileName);
  if (!logFile.is_open())
  {
    std::cerr << "ERROR opening log file" << std::endl;
    exit(1);
  }

  // Create socket
  serverSocket = socket(AF_INET, SOCK_STREAM, 0);
  if (serverSocket < 0)
  {
    perror("ERROR opening socket");
    exit(1);
  }

  // Clear address structure
  bzero((char *)&serverAddress, sizeof(serverAddress));

  // Set fields in address structure
    serverAddress.sin_family = AF_INET;
  serverAddress.sin_port = htons(PORT_NO);
  server = gethostbyname("localhost");
  if (server == nullptr)
  {
    perror("ERROR, no such host");
    exit(1);
  }
  bcopy((char *)server->h_addr, (char *)&serverAddress.sin_addr.s_addr, server->h_length);

  // Connect to server
  if (connect(serverSocket, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0)
  {
    perror("ERROR connecting");
    exit(1);
  }

  // Send CONN command to server with client name
  char buffer[BUFFER_SIZE];
  bzero(buffer, BUFFER_SIZE);
  std::string sendMessage = "CONN|" + name;
  strcpy(buffer, sendMessage.c_str());
  int n = write(serverSocket, buffer, strlen(buffer));
  if (n < 0)
  {
    perror("ERROR writing to socket");
    exit(1);
  }

  // Start thread to handle incoming messages from server
  std::thread receiveThread(receiveChat, serverSocket);

  // Loop to continuously send messages to server
  while (true)
  {
    std::string receiver;
    std::cout << "Enter receiver name: ";
    std::getline(std::cin, receiver);

    std::string message;
    std::cout << "Enter message: ";
    std::getline(std::cin, message);

    // Construct MESG command and send to server
    bzero(buffer, BUFFER_SIZE);
    std::string sendMessage = "MESG|" + receiver + "|" + message;
    strcpy(buffer, sendMessage.c_str());
    n = write(serverSocket, buffer, strlen(buffer));
    if (n < 0)
    {
      perror("ERROR writing to socket");
      exit(1);
    }

    // Write message to log file
    logFile << name << "->" << receiver << ": " << message << std::endl;
  }

  // Close socket and log file
  close(serverSocket);
  logFile.close();

  return 0;
}



