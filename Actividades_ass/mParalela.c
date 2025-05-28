#include <stdio.h>
#include <stdlib.h>
#include <omp.h>

int main() {
    // Medición de tiempo 
    double start_time = omp_get_wtime();
    
    int i, j, k, c = 1;
    int n = 350; 
    int m = 350; 
    int p = 350;

    int A[n][m], B[m][p], C[n][p];

    printf("Matriz A:\n");
    for (i = 0; i < n; i++) {
        for (j = 0; j < m; j++) {
            A[i][j] = c++;
            printf("%d ", A[i][j]);
        }
        printf("\n");
    }

    printf("\nMatriz B:\n");
    c = 1; 
    for (i = 0; i < m; i++) {
        for (j = 0; j < p; j++) {
            B[i][j] = c++;
            printf("%d ", B[i][j]);
        }
        printf("\n");
    }

    printf("\nMatriz C (A x B)  paralela:\n");
    
    // Paralelización 
    #pragma omp parallel for private(j, k) shared(A, B, C, n, p, m)
    for (i = 0; i < n; i++) {
        for (j = 0; j < p; j++) {
            C[i][j] = 0; 
            for (k = 0; k < m; k++) {
                C[i][j] += A[i][k] * B[k][j];
            }
        }
    }
    
    
    for (i = 0; i < n; i++) {
        for (j = 0; j < p; j++) {
            printf(" %d ", C[i][j]);
        }
        printf("\n");
    }
    
   
    double end_time = omp_get_wtime();
    
    // Calcular tiempo transcurrido total
    double execution_time = end_time - start_time;
    
    printf("\nTiempo de ejecucion total paralela: %f segundos\n", execution_time);
    printf("Tiempo en microsegundos: %f\n", execution_time * 1000000);
    omp_get_max_threads();

    return 0;
}