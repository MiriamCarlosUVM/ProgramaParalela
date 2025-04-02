Dado el siguiente código:

struct pack3 
{ 
    int a;
};
struct pack2 
{
    int b;
    struct pack3 *next;
};
struct pack1 
{
    int c;
    struct pack2 *next;
};

struct pack1 data1, *data_ptr;
struct pack2 data2;
struct pack3 data3;

data1.c = 30;
data2.b = 20;
data3.a = 10;
dataPtr = &data1;
data1.next = &data2;
data2.next = &data3;

//Decide si las siguientes expresiones son correctas y en caso de que lo sean escribe 
a que datos se acceden.
 Expresión	                        Correcta	                    Valor//
data1.c		
data_ptr->c		
data_ptr.c		
data1.next->b		
data_ptr->next->b		
data_ptr.next.b		
data_ptr->next.b		
(*(data_ptr->next)).b		
data1.next->next->a		
data_ptr->next->next.a		
data_ptr->next->next->a		
data_ptr->next->a		
data_ptr->next->next->b		