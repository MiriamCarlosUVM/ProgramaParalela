#include <stdio.h>
#include <stddef.h>

struct informacion_operador; // Declaración forward

struct informacion_celda {
    char nombre[100];
    unsigned int identificador;
    float calidad_senal;
    struct informacion_operador *ptr_operador;
};
/*dad*/

int main() {
    struct informacion_celda *ptr = (struct informacion_celda *)0x5000; // Puntero ficticio

    printf("Dirección del puntero ptr: %p\n", (void *)&ptr);
    printf("Dirección de la estructura: %p\n", (void *)ptr);

    printf("\nDirecciones de los campos:\n");
    printf("nombre:        %p\n", (void *)&ptr->nombre);
    printf("identificador: %p\n", (void *)&ptr->identificador);
    printf("calidad_senal: %p\n", (void *)&ptr->calidad_senal);
    printf("ptr_operador:  %p\n", (void *)&ptr->ptr_operador);

    return 0;
}