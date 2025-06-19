#include "stdlib.h"
#include "stdio.h"

#define N 200

int i = 0;
int j = 0;
int k = 0;

int A[N][N];
int B[N][N];
int C[N][N];
int n = 1;

void llenado(){

    for(i = 0; i < N; i ++){
        for (j = 0; j<N; j++){
            A[i][j] = n;
            B[i][j] = n;
            n++;
        }
    }
}


int main(){
    llenado();

    for(i = 0; i < N; i++){
        for(j = 0; j < N; j++){
            C[i][j] = 0;
            for(k = 0; k < N; k++){
            C[i][j] += A[i][k] * B[k][j];
        }
        printf("[%d]", C[i][j]);
    }
    printf("\n");
}

return 0;

}