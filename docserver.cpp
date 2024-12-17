//TODO: error cogiendo -p con decimales

#include <limits.h>

#include <iostream>
#include <string>
#include <unistd.h>
#include <cstdint>
#include <expected>
#include <string_view>
#include <vector>
#include <format>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <charconv>
#include <cstring>

#include "SafeFD.h"
#include "SafeMap.h"

enum class parse_args_errors{
  missing_argument,
  wrong_argument,
  unknown_option,
  missing_filename
};


struct program_options{
  bool show_help{false};
  bool verbose{false};
  uint16_t port{8080};
  std::string filename;
};


void help(){
  std::cout << "Modo de empleo: docserver [OPCION]... FICHERO" << std::endl;
  std::cout << "Compartir ficheros por internet" << std::endl;
  std::cout << "  -h, --help            mostrar unicamente un mensaje de ayuda" << std::endl;
  std::cout << "  -v, --verbose         mostrar mensajes informativos por la salida de error" << std::endl;
  std::cout << "  -p, --port <puerto>   seleccionar el puerto por el que comunicarse" << std::endl;
}


std::string Getenv(const std::string& name){
  char* value = getenv(name.c_str());
  if(value) return std::string(value);
  else return std::string();
}


std::expected<program_options, parse_args_errors> parse_args(int argc, char* argv[]){
  std::vector<std::string_view> args(argv + 1, argv + argc);
  program_options options;

  bool p_selected{false};

  if(args.size() == 0) return std::unexpected(parse_args_errors::missing_filename);

  //verificar si algun argumento es el de ayuda para ignorar el resto
  for(auto it = args.begin(), end = args.end(); it != end; it++){
    if(*it == "-h" || *it == "--help"){
      options.show_help = true;
      return options;
    }
  }

  for (auto it = args.begin(), end = args.end(); it != end; it++){
    if(it == end - 1){
      if(*it == "-v" || *it == "--verbose" || *it == "-p" || *it == "--port") return std::unexpected(parse_args_errors::missing_filename);
      else options.filename = *it;
    }
    else{
      if(*it == "-v" || *it == "--verbose") options.verbose = true;
      else if(*it == "-p" || *it == "--port"){
        p_selected = true;
        if(it == end - 1 || it + 1 == end - 1){
          //si -p es el ultimo parametro se interpreta que falta el argumento de -p
          //Si despues de -p solo hay un argumento, se interpreta como el filename, tambien informando de que falta el argumento de -p
          return std::unexpected(parse_args_errors::missing_argument);
        }
        else{
          it++;
          auto [ptr, ec] = std::from_chars((*it).begin(), (*it).end(), options.port);
          if(ec != std::errc()) return std::unexpected(parse_args_errors::wrong_argument);
        }
      }
      else return std::unexpected(parse_args_errors::unknown_option);
    }
  }
  if(!p_selected){
    std::string port_env = Getenv("DOCSERVER_PORT");
    if(!port_env.empty()){
      auto [ptr, ec] = std::from_chars(&port_env[0], &port_env[0] + std::strlen(&port_env[0]), options.port);
      if(ec != std::errc()) options.port = 8080;
    }
  }
  return options;
}


std::expected<SafeMap, int> read_all(const std::string& path, bool verbose){
  int fd = open(&path[0], O_RDONLY);

  if(fd < 0){
    return std::unexpected(errno);
  }
  if(verbose) std::cerr << "open: se abre el archivo " << path << std::endl;
  size_t length = static_cast<size_t>(lseek(fd, 0, SEEK_END));
  void* mem = mmap(NULL, length, PROT_READ, MAP_PRIVATE, fd, 0);
  if(mem == MAP_FAILED){
    return std::unexpected(errno);
  }
  if(verbose) std::cerr << "read: se leen " << length << " bytes del archivo " << path << std::endl;
  close(fd);
  std::string_view mem_sv(static_cast<char*>(mem), length);
  SafeMap mem_sm(mem_sv);
  return mem_sm;
}


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


int send_response(const SafeFD& socket, std::string_view header, bool verbose, std::string_view body = {}){
  if(verbose) std::cerr << "Sending response..." << std::endl;
  if(send(socket.get(), header.data(), header.size(), 0) < 0) return errno;

  //Salto de linea entre header y body
  if(send(socket.get(), new char{'\n'}, 1, 0) < 0) return errno;
  if(send(socket.get(), body.data(), body.size(), 0) < 0) return errno;
  return EXIT_SUCCESS;
}


int main(int argc, char* argv[]){
  std::expected<program_options, parse_args_errors> arguments = parse_args(argc, argv);
  if(!arguments){
    if(arguments.error() == parse_args_errors::missing_argument) std::cerr << "Missing argument" << std::endl;
    else if(arguments.error() == parse_args_errors::wrong_argument) std::cerr << "Wrong argument" << std::endl;
    else if(arguments.error() == parse_args_errors::unknown_option) std::cerr << "Unknown option" << std::endl;
    else if(arguments.error() == parse_args_errors::missing_filename) std::cerr << "Missing filename" << std::endl;
    return -1;
  }
  if(arguments.value().show_help){
    help();
    return 0;
  }

  //Crear un socket y asignarle el puerto indicado
  std::expected<SafeFD, int> socket = make_socket(arguments.value().port);
  if(!socket){
    std::cerr << "Error while making socket" << std::endl;
    return -1;
  }

  if(listen_connection(socket.value()) != EXIT_SUCCESS){
    std::cerr << "Error while setting socket to listen" << std::endl;
    return -1;
  }

  if(arguments.value().verbose) std::cerr << "Listening for incoming connections on port " << arguments.value().port << std::endl;

  sockaddr_in client_addr;
  std::string header{};
  std::string body{};
  
  while(true){
    std::expected<SafeFD, int> new_fd = accept_connection(socket.value(), client_addr, arguments.value().verbose);
    if(!new_fd){
      std::cerr << "Error accepting connection" << std::endl;
      return -1;
    }

    std::expected<SafeMap, int> map = read_all(arguments.value().filename, arguments.value().verbose);
    if(!map){
      std::cerr << std::strerror(map.error()) << std::endl;
      if(map.error() == 13) send_response(new_fd.value(), "403 Forbidden", arguments.value().verbose);
      else if(map.error() == 2) send_response(new_fd.value(), "404 Not Found", arguments.value().verbose);
      else return -1;
    }
    else{
      int response = send_response(new_fd.value(), std::format("{0}: {1} bytes", arguments.value().filename, map.value().get().size()), arguments.value().verbose, map.value().get());
      if(response != 0){
        std::cerr << "Error sending response" << std::endl;
        if(response != ECONNRESET) return -1;
      }
    }
  }
  return 0;
}