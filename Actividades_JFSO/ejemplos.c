
#include <stdio.h>
int main(int argc, char** argv) 
{
    int num1, num2;
    int *ptr1, *ptr2;

    ptr1 = &num1;
    ptr2 = &num2;

    num1 = 10;
    num2 = 20;

    *ptr1 = 30;
    *ptr2 = 40;

    printf ("El valor de *ptr1 es %d\n", *ptr1);
    printf ("El valor de *ptr2 es %d\n", *ptr2);

    *ptr2 = *ptr1;

    printf ("El valor de *ptr2 es %d\n", *ptr2);
    printf ("El valor de ptr2 es %d\n", ptr2);

    return 0;
}