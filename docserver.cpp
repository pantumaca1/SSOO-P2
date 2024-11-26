#include <iostream>
#include <unistd.h>
#include <cstdint>
#include <expected>
#include <string_view>
#include <vector>

#include <algorithm>
#include <system_error>

#include <fcntl.h>
#include <libgen.h>     // Cabecera para basename()
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

enum class parse_args_errors{
  missing_argument,
  unknown_option
};

struct program_options{
  bool show_help = false;
  std::string output_filename;
  std::vector<std::string> additional_args;
};

class SafeMap{
  public:
    explicit SafeMap(std::string_view sv) noexcept : sv_{sv}{}
    ~SafeMap() noexcept{
      if(!sv_.empty()) sv_ = {};
    }
    [[nodiscard]] std::string_view get() const noexcept{
      if(munmap(sv_.data(), sv_.length()));
    }
  private:
    std::string_view sv_;
};

std::expected<program_options, parse_args_errors> parse_args(int argc, char* argv[]){
  std::vector<std::string_view> args(argv + 1, argv + argc);
  program_options options;
  for (auto it = args.begin(), end = args.end(); it != end; ++it){
    if (*it == "-h" || *it == "--help"){
      options.show_help = true;
    }
    else if (*it == "-o" || *it == "--output"){
      if (++it != end){
        options.output_filename = *it;
      }
      else{
        return std::unexpected( parse_args_errors::missing_argument );
      }
    }
    // Procesar otras opciones...
    else if(!it->starts_with("-")){
      std::string it_str{*it};
      options.additional_args.push_back(it_str);
    }
    else{
      return std::unexpected( parse_args_errors::unknown_option );
    }
  }
  return options;
}

void send_response(std::string_view header, std::string_view body = {}){
  std::cout << header << "\n\n";
  if(!body.empty()){
    std::cout << body << std::endl;
  }
}

std::string_view read_all(const std::string& path){
  int fd = open(&path[0], O_RDONLY);
  int length = lseek(fd, 0, SEEK_END);
  void* mem = mmap(NULL, length, PROT_READ, MAP_PRIVATE, fd, 0);
  std::string_view firstenchars(static_cast<char*>(mem), 10);
  std::cout << firstenchars << std::endl;
  return firstenchars;
}

int main(int argc, char* argv[]){
  read_all("./foo.txt");
}
