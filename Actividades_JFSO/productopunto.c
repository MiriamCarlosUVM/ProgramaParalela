#include "stdio.h"
#include "stdlib.h"
#include "omp.h"

#define MILLION 1000000

int arreglo1 [MILLION];
int arreglo2 [MILLION];
 
void aumentar(){
for (int i = 0; i<MILLION; i++) {
 
arreglo1 [i] = 1;
 
arreglo2 [i] = 1;

}
}
int main(){
    int  mult = 0;
    int sum = 0;
    aumentar();

# pragma omp parallel
{

    #pragma omp for reduction(+:sum)
    for (int i = 0; i<MILLION; i++){
 
   sum += arreglo1 [i] * arreglo2 [i];
 
}

}
 
printf("Produto punto: %d", sum);


}
 