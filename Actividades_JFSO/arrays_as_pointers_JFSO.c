#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SIZE 10
//Una función que imprima por pantalla los elementos de un array de enteros: void print_array(int *array);

void print_array(int *array) {
    printf("Elementos del arreglo: \n");
    for (int i = 0; i < SIZE; i++) {
        printf("Elemnto %d: ", i);
        printf("%d \n", *(array + i));
    }
}

/* 
Una segunda función que calcule la media entre cada par de elementos de un array
y que la vaya imprimiendo por pantalla: void calculate_average(int *array1, int *array2);.
Para la primera posición de cada array, sumará los dígitos y los dividirá por dos,
y ese resultado lo sacará por pantalla, y así con el resto de posiciones.
*/

void calculate_average(int *array1, int *array2){
    printf("Medias entre elementos de 2 arreglos: \n");
    for(int i = 0; i < SIZE; i++){
        float avrg = (*(array1 + i) + *(array2 + i)) / 2.0;
        printf("Media  de pocision %d: %.1f \n", i, avrg);
    }

/*
Una función main que declare e inicialice dos arrays de 10 enteros con los dígitos que queráis,
por ejemplo int array1[] = {1,5,7,3,12,...};,
y que haga uso de la primera función para imprimir los arrays y de la segunda para calcular la media.
*/

}
int main(){
    int array1[SIZE] = {11,21,45,32,58,22,32,12,44,36};
    int array2[SIZE] = {31,48,76,42,30,55,18,42,88,21};

    print_array(array1);
    print_array(array2);

    calculate_average(array1, array2);
    

    return 0;
};