#include <iostream>
#include <string>
#include <sstream>
#include <limits.h>
#include <unistd.h>
#include <cstdint>
#include <cstdlib>
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
#include <sys/wait.h>

#include "SafeFD.h"
#include "SafeMap.h"
#include "Socket.h"


enum class parse_args_errors{
  missing_argument,
  wrong_argument,
  unknown_option,
};


struct program_options{
  bool show_help{false};
  bool verbose{false};
  uint16_t port{8080};
  std::string basedir;
};


struct execute_program_error{
  int exit_code;
  int error_code;
};


struct exec_environment{
  std::string REQUEST_PATH;
  std::string SERVER_BASEDIR;
  std::string REMOTE_PORT;
  std::string REMOTE_IP;
};


void help(){
  std::cout << "Modo de empleo: docserver [OPCION]..." << std::endl;
  std::cout << "Compartir ficheros por internet" << std::endl;
  std::cout << "  -h, --help            mostrar unicamente un mensaje de ayuda" << std::endl;
  std::cout << "  -v, --verbose         mostrar mensajes informativos por la salida de error" << std::endl;
  std::cout << "  -p, --port <puerto>   seleccionar el puerto por el que comunicarse" << std::endl;
  std::cout << "  -b, --base <ruta>     indicar el directorio base de los archivos que pida el cliente" << std::endl;
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
  bool b_selected{false};

  //verificar si algun argumento es el de ayuda para ignorar el resto
  for(auto it = args.begin(), end = args.end(); it != end; it++){
    if(*it == "-h" || *it == "--help"){
      options.show_help = true;
      return options;
    }
  }

  for (auto it = args.begin(), end = args.end(); it != end; it++){
    if(*it == "-v" || *it == "--verbose") options.verbose = true;

    if(it == end - 1){
      if(*it == "-p" || *it == "--port" || *it == "-b" || *it == "--base") return std::unexpected(parse_args_errors::missing_argument);
    }
    else{
      if(*it == "-p" || *it == "--port"){
        p_selected = true;
        if(it == end - 1){
          return std::unexpected(parse_args_errors::missing_argument);
        }
        else{
          it++;
          //Comprobar si el argumento para -p es un decimal, ya que en tal caso std::from_chars() actua ignorando
          //la parte decimal, sin retornar ningun error
          for(long unsigned int i = 0; i < (*it).size(); i++){
            if((*it)[i] == '.') return std::unexpected(parse_args_errors::wrong_argument);
          }
          auto [ptr, ec] = std::from_chars((*it).begin(), (*it).end(), options.port);
          if(ec != std::errc()) return std::unexpected(parse_args_errors::wrong_argument);
        }
      }
      else if(*it == "-b" || *it == "--base"){
        b_selected = true;
        if(it == end - 1){
          return std::unexpected(parse_args_errors::missing_argument);
        }
        else{
          it++;
          if((*it)[0] != '/') return std::unexpected(parse_args_errors::wrong_argument);
          else options.basedir = *it;
        }
      }
      else if(*it != "-v" && *it != "--verbose") return std::unexpected(parse_args_errors::unknown_option);
    }
  }
  //Dar a port su valor por defecto si el usuario no lo ha especificado
  if(!p_selected){
    std::string port_env = Getenv("DOCSERVER_PORT");
    if(!port_env.empty()){
      auto [ptr, ec] = std::from_chars(&port_env[0], &port_env[0] + std::strlen(&port_env[0]), options.port);
      if(ec != std::errc()) options.port = 8080;
    }
  }
  //Dar a basedir su valor por defecto si el usuario no lo ha especificado
  if(!b_selected){
    options.basedir = Getenv("DOCSERVER_BASEDIR");
    if(options.basedir.empty()){
      char cwd[PATH_MAX];
      options.basedir = getcwd(cwd, sizeof(cwd));
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


std::expected<std::string, execute_program_error> execute_program(const std::string& path){
  char buffer[1024];
  memset(buffer, 0, sizeof(buffer));
  execute_program_error error;
  error.exit_code = -1;

  //Comprobar que el programa existe y tiene permisos
  if(access(&path[0], X_OK) < 0 || access(&path[0], F_OK) < 0){
    std::cerr << "Archivo no valido" << std::endl;
    error.error_code = errno;
    return std::unexpected(error);
  }

  //Crear tubería
  int pipefd[2];
  int result = pipe(pipefd);
  if(result < 0){
    std::cerr << "Error al crear pipe" << std::endl;
    error.error_code = errno;
    return std::unexpected(error);
  }

  //Crear proceso hijo
  pid_t pid_hijo = fork();
  if(pid_hijo < 0){
    std::cerr << "Error al crear el proceso" << std::endl;
    error.error_code = errno;
    return std::unexpected(error);
  }
  if(pid_hijo == 0){
    //Bloque proceso hijo
    if(dup2(pipefd[1], STDOUT_FILENO) < 0){
      std::cerr << "Error en la redireccion de salida estandar al proceso hijo" << std::endl;
      error.error_code = errno;
      return std::unexpected(error);
    }
    if(execl(path.c_str(), path.c_str(), NULL) == -1){
      std::cerr << std::strerror(errno) << std::endl;
      error.error_code = errno;
      _exit(126);
    }
  }
  else{
    //Esperar a que el proceso hijo termine
    int status_hijo;
    waitpid(pid_hijo, &status_hijo, 0);
    read(pipefd[0], buffer, sizeof(buffer));
  }
  return buffer;
}


int main(int argc, char* argv[]){
  std::expected<program_options, parse_args_errors> arguments = parse_args(argc, argv);
  if(!arguments){
    if(arguments.error() == parse_args_errors::missing_argument) std::cerr << "Missing argument" << std::endl;
    else if(arguments.error() == parse_args_errors::wrong_argument) std::cerr << "Wrong argument" << std::endl;
    else if(arguments.error() == parse_args_errors::unknown_option) std::cerr << "Unknown option" << std::endl;
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
  exec_environment env_vars;

  while(true){
    std::expected<SafeFD, int> new_fd = accept_connection(socket.value(), client_addr, arguments.value().verbose);
    if(!new_fd){
      std::cerr << "Error accepting connection" << std::endl;
      return -1;
    }
    std::expected<std::string, int> request_str = receive_request(new_fd.value(), 1024);
    if(!request_str){
      std::cerr << std::strerror(request_str.error());
      if(request_str.error() != ECONNRESET) return -1;
    }

    bool bad_req{false};
    std::istringstream iss(request_str.value());
    std::string first_str, file_str, path_str;
    iss >> first_str >> file_str;
    if(first_str != "GET" || file_str[0] != '/' || request_str.value().empty()) bad_req = true;

    if(bad_req) send_response(new_fd.value(), "400 Bad Request", arguments.value().verbose);
    else path_str = arguments.value().basedir + file_str;

    if(file_str.starts_with("/bin")){
      std::expected<std::string, execute_program_error> output = execute_program(path_str);
      std::cout << output.value() << std::endl;
    }

    else{
      std::expected<SafeMap, int> map = read_all(path_str, arguments.value().verbose);
      if(!map){
        std::cerr << std::strerror(map.error()) << std::endl;
        if(map.error() == 13) send_response(new_fd.value(), "403 Forbidden", arguments.value().verbose);
        else if(map.error() == 2) send_response(new_fd.value(), "404 Not Found", arguments.value().verbose);
        else return -1;
      }
      else{
        file_str.erase(0, 1);
        int response = send_response(new_fd.value(), std::format("{0}: {1} bytes", file_str, map.value().get().size()), arguments.value().verbose, map.value().get());
        if(response != 0){
          std::cerr << "Error sending response" << std::endl;
          if(response != ECONNRESET) return -1;
        }
      }
    }
  }
  return 0;
}