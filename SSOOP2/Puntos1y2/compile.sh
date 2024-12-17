g++ -o server -std=c++23 -Wall -Wextra -Werror -Wpedantic -Wshadow -Wnon-virtual-dtor -Wold-style-cast \
-Wcast-align -Wunused -Woverloaded-virtual -Wconversion -Wsign-conversion -Wnull-dereference -Wdouble-promotion \
-Wformat=2 -Wmisleading-indentation -Wduplicated-cond -Wduplicated-branches -Wlogical-op -Wuseless-cast \
-fsanitize=address,undefined,leak docserver.cpp