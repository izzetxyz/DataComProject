#include <iostream>
#include <map>
#include <vector>
#include <string>
#include <sstream>
#include <algorithm>
#include <random>
#include <chrono>
#include <thread>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define PORT_NO 12345
#define BUFFER_SIZE 1024

std::map<int, std::string> clients; // Map to store client sockets and namess
std::vector<std::thread> threads; // Vector to store client threads

// Function to handle incoming client connections
void handleClient(int clientSocket)
{
  char buffer[BUFFER_SIZE]; // Buffer to store incoming messages
  int n;

  // Receive CONN command from client
  bzero(buffer, BUFFER_SIZE);
  n = read(clientSocket, buffer, BUFFER_SIZE - 1);
  if (n < 0)
  {
    perror("ERROR reading from socket");
    exit(1);
  }

  std::string connCommand(buffer);
  std::istringstream connStream(connCommand);
  std::string connType;
  std::string clientName;
  connStream >> connType >> clientName;

  // Add client to client list
  clients[clientSocket] = clientName;

  // Send client list to all clients
  std::string clientList;
  for (const auto &client : clients)
  {
    clientList += client.second + "|";
  }
  for (const auto &client : clients)
  {
    bzero(buffer, BUFFER_SIZE);
    std::string message = "CONN|" + clientList;
    strcpy(buffer, message.c_str());
    n = write(client.first, buffer, strlen(buffer));
    if (n < 0)
    {
      perror("ERROR writing to socket");
      exit(1);
    }
  }

  // Randomly choose whether to corrupt message or not
  std::mt19937_64 rng;
  uint64_t timeSeed = std::chrono::high_resolution_clock::now().time_since_epoch().count();
  std::seed_seq ss{uint32_t(timeSeed & 0xffffffff), uint32_t(timeSeed >> 32)};
  rng.seed(ss);
  std::uniform_int_distribution<int> dist(0, 1);
  int corruptProbability = dist(rng);

  // Loop to handle messages from this client
  while (true)
  {
    bzero(buffer, BUFFER_SIZE);
    n = read(clientSocket, buffer, BUFFER_SIZE - 1);
    if (n < 0)
    {
      perror("ERROR reading from socket");
      exit(1);
    }

    std::string message(buffer);
    std::istringstream messageStream(message);
    std::string command;
    messageStream >> command;
    if (command == "MESG")
{
  std::string receiver;
  messageStream >> receiver;

  std::string errorBits;
  messageStream >> errorBits;

  std::string actualMessage;
  std::getline(messageStream, actualMessage);
  actualMessage = actualMessage.substr(1);

  if (corruptProbability == 1)
  {
    actualMessage = "CORRUPTED: " + actualMessage;
  }

  for (const auto &client : clients)
  {
    if (client.second == receiver)
    {
      bzero(buffer, BUFFER_SIZE);
      std::string sendMessage = "MESG|" + errorBits + "|" + actualMessage;
      strcpy(buffer, sendMessage.c_str());
      n = write(client.first, buffer, strlen(buffer));
      if (n < 0)
      {
        perror("ERROR writing to socket");
        exit(1);
      }
      break;
    }
  }
}
else if (command == "MERR")
{
  std::string sender;
  messageStream >> sender;

  std::string errorBits;
  messageStream >> errorBits;

  std::string actualMessage;
  std::getline(messageStream, actualMessage);
  actualMessage = actualMessage.substr(1);

  for (const auto &client : clients)
  {
    if (client.second == sender)
    {
      bzero(buffer, BUFFER_SIZE);
      std::string sendMessage = "MERR|" + errorBits + "|" + actualMessage;
      strcpy(buffer, sendMessage.c_str());
      n = write(client.first, buffer, strlen(buffer));
      if (n < 0)
      {
        perror("ERROR writing to socket");
        exit(1);
      }
      break;
    }
  }
}
else if (command == "GONE")
{
  // Remove client from client list and send updated client list to all clients
  std::string clientName = clients[clientSocket];
  clients.erase(clientSocket);

  std::string clientList;
  for (const auto &client : clients)
  {
    clientList += client.second + "|";
  }
  for (const auto &client : clients)
  {
    bzero(buffer, BUFFER_SIZE);
    std::string message = "CONN|" + clientList;
    strcpy(buffer, message.c_str());
    n = write(client.first, buffer, strlen(buffer));
    if (n < 0)
    {
      perror("ERROR writing to socket");
      exit(1);
    }
  }

  // Close socket and exit thread
  close(clientSocket);
  break;
}

}
}

int main()
{
    int serverSocket, clientSocket;
    socklen_t clientLen;
    struct sockaddr_in serverAddress, clientAddress;

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
    serverAddress.sin_addr.s_addr = INADDR_ANY;
    serverAddress.sin_port = htons(PORT_NO);

    // Bind socket to address
    if (bind(serverSocket, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0)
    {
      perror("ERROR on binding");
      exit(1);
    }

    // Listen for incoming connections
    listen(serverSocket, 5);
    clientLen = sizeof(clientAddress);

    // Loop to continuously accept incoming client connections
    while (true)
    {
    // Accept incoming connection
      clientSocket = accept(serverSocket, (struct sockaddr *)&clientAddress, &clientLen);
      if (clientSocket < 0)
      {
      perror("ERROR on accept");
      exit(1);
      }

      // Start a new thread to handle the client
      threads.emplace_back(handleClient, clientSocket);

      }

    return 0;
}
