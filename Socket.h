#pragma once

#include <iostream>
#include <cstdint>
#include <expected>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <cstring>

#include "SafeFD.h"


std::expected<SafeFD, int> make_socket(uint16_t port){
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if(fd < 0) return std::unexpected(errno);

  sockaddr_in local_address{};
  local_address.sin_family = AF_INET;
  local_address.sin_addr.s_addr = htonl(INADDR_ANY);
  local_address.sin_port = htons(port);

  int result = bind(fd, reinterpret_cast<const sockaddr*>(&local_address), sizeof(local_address));
  if (result < 0) return std::unexpected(errno);

  return SafeFD(fd);
}


int listen_connection(const SafeFD& socket){
  if(listen(socket.get(), 5) < 0){
    std::cout << std::strerror(errno) << std::endl;
    return errno;
  }
  else return EXIT_SUCCESS;
}


std::expected<SafeFD, int> accept_connection(const SafeFD& socket, sockaddr_in& client_addr, bool verbose){
  socklen_t client_addr_length{sizeof(client_addr)};
  SafeFD new_fd(accept(socket.get(), reinterpret_cast<sockaddr*>(&client_addr), &client_addr_length));
  if(verbose) std::cerr << "Connection accepted ..." << std::endl;

  if(new_fd.get() < 0) return std::unexpected(errno);
  else return new_fd;
}


std::expected<std::string, int> receive_request(const SafeFD& socket, size_t max_size){
  std::string str(max_size, '0');
  ssize_t size = recv(socket.get(), &str[0], max_size, 0);
  if(size < 0) return std::unexpected(errno);
  else{
    str.resize(static_cast<long unsigned int>(size));
    return str;
  }
}

int send_response(const SafeFD& socket, std::string_view header, bool verbose, std::string_view body = {}){
  if(verbose) std::cerr << "Sending response..." << std::endl;
  if(send(socket.get(), header.data(), header.size(), 0) < 0) return errno;

  //Salto de linea entre header y body
  if(send(socket.get(), new char{'\n'}, 1, 0) < 0) return errno;

  if(send(socket.get(), body.data(), body.size(), 0) < 0) return errno;
  return EXIT_SUCCESS;
}