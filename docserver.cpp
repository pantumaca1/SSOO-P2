#include <iostream>
#include <expected>
#include <string_view>
#include <vector>

void parse_args(){}

std::expected<std::string_view, int> read_all(const std::string& path);

int main(int argc, char* argv[]){
  std::vector<std::string_view> args(argv + 1, argv + argc);
  /*for(int i = 0; i < args.size(); i++){
    std::cout << args[i] << std::endl;
  }*/
 std::cout << argv + 1 << std::endl;
 std::cout << argv + argc << std::endl;
}