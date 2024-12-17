#include <iostream>
#include <unistd.h>
#include <cstdint>
#include <limits.h>

struct program_options{
  bool show_help{false};
  bool verbose{false};
  uint16_t port{8080};
  std::string filename;
  std::string basedir;
};

int main(){
  program_options options;
  char cwd[PATH_MAX];
  options.basedir = getcwd(cwd, sizeof(cwd));
  std::cout << options.basedir << std::endl;
}