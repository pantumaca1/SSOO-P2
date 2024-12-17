#include <iostream>
#include <unistd.h>
#include <cerrno>
#include <cstring>

int main() {
    std::cout << "Antes de llamar a execl()" << std::endl;

    // Ejecutar el script en la carpeta de trabajo actual
    if (execl("./mi_script.sh", "mi_script.sh", (char *)0) == -1) {
        // Manejo de error
        std::cerr << "Error al ejecutar execl: " << strerror(errno) << std::endl;
        return 1;
    }

    // Esta línea no se ejecutará si execl tiene éxito
    std::cout << "Después de execl (esto no se verá)" << std::endl;

    return 0;
}