#include <stdlib.h>
#include <stdio.h>

/*
Definición de la estructura de datos para almacenar la información de una celda y declaración
de las variables necesarias para almacenar los datos que se piden.
*/
typedef struct Cell{
    int id;
    int signal_quality;
    struct Cell *next;
    struct Cell *prev;
} Cell;

// Puntero head (primer nodo) y current head (Celda actual)
static Cell* head = NULL;
static Cell* current_cell = NULL;

/*
void fill_fields(...). Función que asigna los valores de los campos pasados como parámetros
a las variables de una estructura cuyo puntero se recibe como parámetro.
Decide primero qué parámetros debe recibir la función y luego escribe el código.
*/


void fill_fields(Cell* cell, int id, int signal_quality) {
    cell->id = id;
    cell->signal_quality = signal_quality;
    cell->next = NULL;
    cell->prev = NULL;
}

// void update_current(...). Función que asigna como celda “actual” la que tiene mayor calidad de señal.

void update_current(){
    if (head == NULL) {
        current_cell = NULL;
        return;
    }

    Cell* max_cell = head;
    Cell* current = head->next;

    while (current != NULL && current != head)
    {
        if (current->signal_quality > max_cell->signal_quality){
            max_cell = current;
        }
        current = current->next;
    }

    current_cell = max_cell;
}

Cell* new_cell() {
    Cell* new = malloc(sizeof(Cell));
    if (new == NULL) exit(EXIT_FAILURE);  // Manejo básico de errores
    return new;
}


int main(){
       // Crear 3 celdas
       Cell* c1 = new_cell();
       fill_fields(c1, 1, 80);
       
       Cell* c2 = new_cell();
       fill_fields(c2, 2, 95);
       
       Cell* c3 = new_cell();
       fill_fields(c3, 3, 75);
   
       // Enlazar como lista circular doblemente enlazada
       head = c1;
       
       // Configurar vecinos para c1
       c1->next = c2;
       c1->prev = c3;
       
       // Configurar vecinos para c2
       c2->next = c3;
       c2->prev = c1;
       
       // Configurar vecinos para c3
       c3->next = c1;
       c3->prev = c2;
   
       update_current();
       
       if (current_cell != NULL) {
        printf("Celda actual: ID=%d, Señal=%d\n", current_cell->id, current_cell->signal_quality);
       }

       return 0;
}
/*
¿Qué cambio harías en la definición de la estructura de datos para almacenar la información de forma
“circular” (es decir, que estén ordenadas como un círculo)?
*/

/*
En una lista circular, el último nodo apunta al primero (next) y viceversa (prev en doblemente enlazada).

Implementación en C: Al agregar nodos, se ajustan los punteros para mantener el círculo.
 Por ejemplo, en una lista doblemente enlazada circular:

El next del último nodo apunta al primero.

El prev del primero apunta al último.

Esto permite recorrer las celdas en un bucle sin fin, manejando cuidadosamente las condiciones de parada.
*/