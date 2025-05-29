#include "stdlib.h"
#include "stdio.h"
#include "omp.h"

#define N 100

int A[N][N];
int B[N][N];
int C[N][N];

void llenado(){

    #pragma omp parallel for collapse(2)
    for(int i = 0; i < N; i ++){
        for (int j = 0; j<N; j++){
            int n = i * N + j + 1;
            A[i][j] = n;
            B[i][j] = n;
        }
    }
}


int main(){
    llenado();

    #pragma omp parallel for collapse(2)
        for(int i = 0; i < N; i++){
            for(int j = 0; j < N; j++){
                C[i][j] = 0;
                for(int k = 0; k < N; k++){
                C[i][j] += A[i][k] * B[k][j];
            }
        }
    }

for(int i = 0; i < N; i ++){
        for (int j = 0; j<N; j++){
        printf("[%d]", C[i][j]);
        }
        printf("\n");
    }

return 0;
}