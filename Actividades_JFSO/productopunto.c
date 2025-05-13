#include "stdio.h"
#include "stdlib.h"
#include "omp.h"

int arreglo1 [10];
int arreglo2 [10];
 
void aumentar(){
for (int i = 0; i<10; i++) {
 
arreglo1 [i] = 1;
 
arreglo2 [i] = 2;
}
}
int main(){
    int  mult = 0;
    int sum = 0;
    aumentar();

# pragma omp parallel
{

    #pragma omp for reduction(+:sum)
    for (int i = 0; i<10; i++){
 
   sum += arreglo1 [i] * arreglo2 [i];
 
}

}
 
printf("Produto punto: %d", sum);


}
 