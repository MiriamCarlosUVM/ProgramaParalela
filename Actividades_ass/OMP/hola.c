#include <stdio.h>
#include <omp.h>


int main(void){

    #pragma omp parallel num_threads(5)
    {
    printf("hello desde thread ? de ?.\n");

    }

    return 0;
}