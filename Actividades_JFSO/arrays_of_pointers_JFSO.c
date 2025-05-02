#include <stdlib.h>
#include <stdio.h>

#define SIZE 7

char *str[SIZE] = {"Lunes",
                    "Marters",
                    "Miercoles",
                    "Jueves",
                    "Viernes",
                    "Sabado",
                    "Domingo"};

void print_strings(char **str){
    for(int i = 0; i < SIZE; i++){
        printf("Día %s\n", str[i]);
    }
}

int main(){
    
    print_strings(str);
    
    return 0;
}