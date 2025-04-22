/* Teniendo esto en cuenta, calculad la media entre los elementos de dos arrays. 
Se pide para ello que imprimáis por pantalla los elementos de dos arrays y que calculéis e imprimáis la media entre cada par de elementos. 
Para ello, cread en vuestro entorno de desarrollo un fichero llamado arrays_as_pointers.c, 
e implementad en él lo siguiente (veréis que es el mismo programa que el de arrays básicos, pero ahora tendréis que enviar el array en forma de puntero):

Una función que imprima por pantalla los elementos de un array de enteros: void print_array(int *array);

Una segunda función que calcule la media entre cada par de elementos de un array y que la vaya imprimiendo por pantalla: 
void calculate_average(int *array1, int *array2);. Para la primera posición de cada array,
sumará los dígitos y los dividirá por dos, y ese resultado lo sacará por pantalla, y así con el resto de posiciones.

Una función main que declare e inicialice dos arrays de 10 enteros  y que haga uso de la primera función para imprimir los arrays y de la segunda para calcular la media.*/


#include <stdio.h>

#define SIZE 10

// Función que imprime un array utilizando punteros
void print_array(int *array) {
    for (int i = 0; i < SIZE; i++) {
        printf("%d ", *(array + i));
    }
    printf("\n");
}

// Función que calcula e imprime la media entre cada par de elementos
void calculate_average(int *array1, int *array2) {
    printf("Medias entre elementos:\n");
    for (int i = 0; i < SIZE; i++) {
        float media = (*(array1 + i) + *(array2 + i)) / 2.0;
        printf("Media de %d y %d = %.2f\n", *(array1 + i), *(array2 + i), media);
    }
}

int main() {
    int array1[SIZE] = {1, 5, 7, 3, 12, 6, 9, 10, 2, 8};
    int array2[SIZE] = {4, 8, 2, 7, 6, 3, 11, 5, 9, 1};

    printf("Array 1:\n");
    print_array(array1);

    printf("Array 2:\n");
    print_array(array2);

    calculate_average(array1, array2);

    return 0;
}
