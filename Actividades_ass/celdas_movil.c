/* Gestion de celdas en el movil */
#include <stdlib.h>
#include <stdio.h>

// Definición de la estructura de datos para una celda
typedef struct Cell {
    int id;                 // Identificador único (entero positivo)
    int signal_quality;     // Calidad de señal (0-100)
    struct Cell* next;      // Puntero a la siguiente celda
    struct Cell* prev;      // Puntero a la celda anterior
} Cell;

// Variables globales
Cell* current_cell = NULL;  // Puntero a la celda actual (mejor cobertura)
Cell* cell_list = NULL;     // Puntero al inicio de la lista circular

/* Función new_cell 
   Devuelve un puntero a una nueva estructura de celda */
Cell* new_cell() {
    return (Cell*)malloc(sizeof(Cell));
}

/* Función remove_cell 
   Libera la memoria de una celda */
void remove_cell(Cell* cell) {
    free(cell);
}

/* Función fill_fields - Asigna valores a los campos de una celda
   Parámetros:
   - cell: puntero a la celda a modificar
   - id: identificador único
   - quality: nivel de calidad de señal (0-100) */
void fill_fields(Cell* cell, int id, int quality) {
    if (cell == NULL) return;
    
    cell->id = id;
    cell->signal_quality = quality;
    // Los punteros next y prev se establecerán al insertar en la lista
}

/* Función add_cell - Añade una nueva celda a la estructura circular */
void add_cell(Cell* new_cell) {
    if (new_cell == NULL) return;
    
    if (cell_list == NULL) {
        // Primera celda de la lista
        cell_list = new_cell;
        new_cell->next = new_cell;
        new_cell->prev = new_cell;
    } else {
        // Insertar al final de la lista (antes de cell_list)
        new_cell->next = cell_list;
        new_cell->prev = cell_list->prev;
        cell_list->prev->next = new_cell;
        cell_list->prev = new_cell;
    }
    
    // Actualizar celda actual si es la primera o tiene mejor señal
    if (current_cell == NULL || new_cell->signal_quality > current_cell->signal_quality) {
        current_cell = new_cell;
    }
}

/* Función update_current - Actualiza la celda actual (la de mejor señal) */
void update_current() {
    if (cell_list == NULL) {
        current_cell = NULL;
        return;
    }
    
    Cell* max_cell = cell_list;
    Cell* current = cell_list->next;
    
    // Recorremos todas las celdas
    while (current != cell_list) {
        if (current->signal_quality > max_cell->signal_quality) {
            max_cell = current;
        }
        current = current->next;
    }
    
    current_cell = max_cell;
}

/* Función remove_cell_from_list - Elimina una celda de la estructura */
void remove_cell_from_list(Cell* cell) {
    if (cell == NULL || cell_list == NULL) return;
    
    // Caso especial: única celda en la lista
    if (cell->next == cell) {
        cell_list = NULL;
        current_cell = NULL;
    } else {
        // Reenlazar las celdas adyacentes
        cell->prev->next = cell->next;
        cell->next->prev = cell->prev;
        
        // Actualizar cell_list si estamos eliminando el primer elemento
        if (cell == cell_list) {
            cell_list = cell->next;
        }
    }
    
    // Actualizar current_cell si era la que estamos eliminando
    if (cell == current_cell) {
        update_current();
    }
    
    // Liberar memoria
    remove_cell(cell);
}

/* Función que imprime los detalles de la celda actual */
void print_current_cell() {
    if (current_cell != NULL) {
        printf("Celda actual: ID = %d, Calidad de señal = %d\n", current_cell->id, current_cell->signal_quality);
    } else {
        printf("No hay celda actual\n");
    }
}

/* Ejemplo de uso */
int main() {
    // Crear y añadir celdas de ejemplo
    Cell* cell1 = new_cell();
    fill_fields(cell1, 1, 85);
    add_cell(cell1);
    
    Cell* cell2 = new_cell();
    fill_fields(cell2, 2, 92);
    add_cell(cell2);
    
    Cell* cell3 = new_cell();
    fill_fields(cell3, 3, 78);
    add_cell(cell3);
    
    // Imprimir la celda actual después de añadir las celdas
    printf("Después de añadir todas las celdas:\n");
    print_current_cell();  // Imprimir la celda con mejor señal
    
    // Eliminar una celda
    remove_cell_from_list(cell1);
    
    // Imprimir la celda actual después de eliminar una celda
    printf("Después de eliminar cell1:\n");
    print_current_cell();  // Imprimir la celda con mejor señal
    
    return 0;
}
