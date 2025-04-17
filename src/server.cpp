#include <iostream>
#include <cstdlib>
#include <string>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <thread>
#include <netinet/in.h>
#include <fstream>
#include <sstream>

constexpr int PORT = 4221;
constexpr int BACKLOG = 5;
constexpr int BUFFER_SIZE = 4096;

void handle_client(int client_fd, const std::string& directory) {
  char buffer[BUFFER_SIZE] = {0};
  ssize_t bytes_received = recv(client_fd, buffer, sizeof(buffer) - 1, 0);

  if (bytes_received <= 0) {
    std::cerr << "Failed to receive data or client disconnected.\n";
    close(client_fd);
    return;
  }

  buffer[bytes_received] = '\0';
  std::string request(buffer);
  std::cout << "Received request:\n" << request << std::endl;

  std::istringstream request_stream(request);
  std::string method, path, http_version;
  request_stream >> method >> path >> http_version;

  std::string response;

  if (method != "GET") {
    response = "HTTP/1.1 405 Method Not Allowed\r\n\r\n";
  } else if (path == "/") {
    response = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: 0\r\n\r\n";
  } else if (path.rfind("/files/", 0) == 0) {
    std::string filename = path.substr(7);
    std::string fullpath = directory;
    if (fullpath.back() != '/') {
      fullpath += '/';
    }
    fullpath += filename;

    std::ifstream infile(fullpath, std::ios::binary);
    if (!infile) {
      response = "HTTP/1.1 404 Not Found\r\n\r\n";
    } else {
      std::ostringstream buf;
      buf << infile.rdbuf();
      std::string body = buf.str();

      response  = "HTTP/1.1 200 OK\r\n";
      response += "Content-Type: application/octet-stream\r\n";
      response += "Content-Length: " + std::to_string(body.size()) + "\r\n";
      response += "\r\n";
      response += body;
    }
  } else if (path.find("/echo/") == 0) {
    std::string content = path.substr(6);
    response = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: " 
            + std::to_string(content.size()) + "\r\n\r\n" + content;
  } else if (path == "/user-agent") {
    size_t agent_start = request.find("User-Agent: ");
    if (agent_start != std::string::npos) {
        agent_start += 12;
        size_t agent_end = request.find("\r\n", agent_start);
        std::string user_agent = request.substr(agent_start, agent_end - agent_start);
        response = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: " 
                + std::to_string(user_agent.size()) + "\r\n\r\n" + user_agent;
      } else {
        response = "HTTP/1.1 400 Bad Request\r\n\r\n";
      }
  } else {
    response = "HTTP/1.1 404 Not Found\r\n\r\n";
  }

  send(client_fd, response.c_str(), response.size(), 0);
  close(client_fd);
}

int main(int argc, char **argv) {
  // Flush after every std::cout / std::cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  std::string dir;
  if (argc == 3 && strcmp(argv[1], "--directory") == 0) {
    dir = argv[2];
  } else if (argc == 1) {
    dir = "";
  } else {
    std::cerr << "Usage: " << argv[0] << " [--directory <path>]\n";
    return 1;
  }

  int server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd < 0) {
   std::cerr << "Failed to create server socket\n";
   return 1;
  }
  
  // // Since the tester restarts your program quite often, setting SO_REUSEADDR
  // // ensures that we don't run into 'Address already in use' errors
  int reuse = 1;
  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
    std::cerr << "setsockopt failed\n";
    return 1;
  }
  
  struct sockaddr_in server_addr;
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(PORT);
  
  if (bind(server_fd, (struct sockaddr *) &server_addr, sizeof(server_addr)) != 0) {
    std::cerr << "Failed to bind to port " << PORT << "\n";
    return 1;
  }
  
  if (listen(server_fd, BACKLOG) != 0) {
    std::cerr << "Listen failed\n";
    return 1;
  }
  
  std::cout << "Server is listening on port " << PORT << "...\n";
  std::cout << "Waiting for a client to connect...\n";

  while (true) {
    struct sockaddr_in client_addr;
    int client_addr_len = sizeof(client_addr);
    
    int client_fd = accept(server_fd, (struct sockaddr *) &client_addr, (socklen_t *) &client_addr_len);
    if (client_fd < 0) {
      std::cerr << "Failed to accept client connection\n";
      continue;
    }
    std::cout << "Client connected\n";

    std::thread([client_fd, dir]() {
      handle_client(client_fd, dir);
    }).detach();
  }

  close(server_fd);
  return 0;
}
