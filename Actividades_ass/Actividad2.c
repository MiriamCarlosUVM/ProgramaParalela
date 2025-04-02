/*Pregunta 10

Dado el siguiente código:      */

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

/*Decide si las siguientes expresiones son correctas y en caso de que lo sean escribe a que datos se acceden
 Expresión	                        Correcta	                    Valor
data1.c		                       Es correcta                       30 
data_ptr->c		                   Es correcta                       30                    
data_ptr.c	                       No es correcta                   no compila	
data1.next->b		               Es correcta                       20
data_ptr->next->b		           Es correcta                       20 
data_ptr.next.b                    No es correcta                   No compila 		
data_ptr->next.b		           No es correcta                   No compila
(*(data_ptr->next)).b		       Es correcta                       20
data1.next->next->a                Es correcta                       10		
data_ptr->next->next.a		       No es correcta                   No compila  
data_ptr->next->next->a		       Es correcta                       10
data_ptr->next->a		           No es correcta                   No compila
data_ptr->next->next->b		       No es correcta                   No compila */

/*Pregunta 11:
Supongamos que se escriben las siguientes declaraciones y asignaciones en un programa:*/

info_celda c;
info_celda_ptr c_ptr = &c;
info_operador op;
info_operador_ptr op_ptr = &op;

/*

La estructura “c” contiene el campo “ptr_operador” 
precisamente para almacenar la información relativa al operador. 

¿Qué expresión hay que usar en el código para guardar la información del operador “op” 
como parte de la estructura “c”? */
c_var.ptr_operador = &op;


/*Teniendo en cuenta los valores que se asignan en las declaraciones, 
escribe cuatro versiones equivalentes de esta expresión 
(utiliza “c”, “c_ptr”, “op” y “op_ptr”).*/

//Usando variables directas
c_var.ptr_operador = &op;

//Usando el puntero de c 
c_ptr->ptr_operador = &op;

//Puntero a OP
c_var.ptr_operador = op_ptr;

//Punteros de c y op
c_ptr->ptr_operador = op_ptr;

/*Pregunta 12
Supón ahora que la aplicación en la que se usan estas estructuras 
necesita almacenar la información para un máximo de 10 celdas. 
¿Qué estructura de datos definirías?

-Si el numero de celdas es fijo podria usarse un arreglo*/
struct c celdas[10];

//Esto perime acceder a cada celda por su indice 
celdas[0].ptr_operador = &op;



/*Pregunta 13 
Escribe un bucle con la variable declarada en el ejercicio anterior 
que asigne al campo ptr_operador el valor vacío*/

for (int i = 0; i < 10; i++) {
    celdas[i].ptr_operador = NULL;
}

/*Pregunta 14 
La información sobre las celdas que se almacena en la estructura del ejercicio anterior 
la debe utilizar la aplicación para recordar cuál de ellas es la más próxima. 
Esta información puede cambiar a lo largo del tiempo. 
¿Qué tipo de datos sugieres para almacenar esta información? Ofrece dos alternativas.  */

/*
Alternativa 1:
Definir un puntero que almacena la direccion de la celda mas proxima*/

struct c *celda_mas_proxima = NULL;
//Para actualizar a la mas proxima 

celda_mas_proxima = &celdas[indice];  // Donde 'indice' es la posición de la celda más próxima
//Para acceder 
if (celda_mas_proxima != NULL) {
    printf("Celda más próxima tiene dato: %d\n", celda_mas_proxima->dato);
}

/*Alternativa 2:
Definir una variable entera que almacene la posicion en el arreglo de la celda mas proxima*/

int indice_mas_proximo = -1;  // -1 indica que no hay celda seleccionada aún

//Para actualizar 
indice_mas_proximo = 3;  // Si la celda más próxima está en celdas[3]

//Para acceder 
if (indice_mas_proximo != -1) {
    printf("Celda más próxima tiene dato: %d\n", celdas[indice_mas_proximo].dato);
}

