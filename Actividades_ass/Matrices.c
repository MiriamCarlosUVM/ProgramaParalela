#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main() {
    // Medición de tiempo 
    clock_t start_time = clock();
    
    int i, j, k, c = 1;
    int n = 350; 
    int m = 350; 
    int p = 350; 

    int A[n][m], B[m][p], C[n][p];

    // Inicialización de la matriz A
    printf("Matriz A:\n");
    for (i = 0; i < n; i++) {
        for (j = 0; j < m; j++) {
            A[i][j] = c++;
            printf("%d ", A[i][j]);
        }
        printf("\n");
    }

    // Inicializacion de la matriz B
    printf("\nMatriz B:\n");
    c = 1; 
    for (i = 0; i < m; i++) {
        for (j = 0; j < p; j++) {
            B[i][j] = c++;
            printf("%d ", B[i][j]);
        }
        printf("\n");
    }

    // Multiplicación de matrices 
    printf("\nMatriz C (A x B) - Secuencial:\n");
    for (i = 0; i < n; i++) {
        for (j = 0; j < p; j++) {
            C[i][j] = 0; 
            for (k = 0; k < m; k++) {
                C[i][j] += A[i][k] * B[k][j];
            }
            printf(" %d ", C[i][j]);
        }
        printf("\n");
    }
    
    // Medicion de tiempo final
    clock_t end_time = clock();
    
    // tiempo transcurrido total
    double cpu_time_used = ((double) (end_time - start_time)) / CLOCKS_PER_SEC;
    
    printf("\nTiempo de ejecucion total secuencial: %f segundos\n", cpu_time_used);
    printf("Tiempo en microsegundos: %f\n", cpu_time_used * 1000000);
    printf("Numero de hilos utilizados: 1\n");

    return 0;
}