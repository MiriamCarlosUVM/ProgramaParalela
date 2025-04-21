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
