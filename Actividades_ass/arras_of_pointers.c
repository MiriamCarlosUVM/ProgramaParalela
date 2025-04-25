
#include <stdio.h>

#define SIZE 7

// Prototipo de la función
void print_strings(char **str);

int main() {
    char *str[SIZE] = {
        "Lunes",
        "Martes",
        "Miércoles",
        "Jueves",
        "Viernes",
        "Sábado",
        "Domingo"
    };

    // Llamamos a la función para imprimir las cadenas
    print_strings(str);

    return 0;
}

// Implementación de la función
void print_strings(char **str) {
    for (int i = 0; i < SIZE; i++) {
        printf("%s\n", str[i]);
    }
}
