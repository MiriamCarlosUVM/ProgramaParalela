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

/*Pregunta 15 
Se dispone de una estructura de tipo “info_celda c” que a su vez, en el campo “ptr_operador” tiene un puntero a una estructura 
“info_operador cr”. 
¿Qué tamaño tiene la estructura “c”? ¿Qué tamaño total ocupa la información incluyendo la información sobre el operador?*/

//Es probable que se agreguen 4 bytes de padding, dejando el tamaño final en 16 bytes en 64 bits 

struct info_operador {
    int id;             // 4 bytes
    char nombre[20];    // 20 bytes
                        //4 padding
};

struct info_celda {
    int dato;                               //   4 bytes
    struct info_operador *ptr_operador;     //   8 bytes 
                                            // 12 bytes totales 
};

/*¿Qué tamaño total ocupa la información incluyendo la información sobre el operador?
 44 bytes

Si ptr_operador apunta a una instancia dinámica de info_operador, la memoria ocupada incluye ambas estructuras */

/*
Pregunta 16 
Escribe el cuerpo de la siguiente función:

void fill_in(info_celda_ptr dato, unsigned int id, float sq, info_operador_ptr op_ptr)

que asigna el valor de los parámetros “id”, “sq” y “op_ptr” 
a los campos “identificador”, “calidad_senal” y “ptr_operador” 
respectivamente de la estructura apuntada por el parámetro “dato”.

¿Cómo explicas que esta función asigne valores a unos campos y no devuelva resultado?*/

void fill_in(info_celda_ptr dato, unsigned int id, float sq, info_operador_ptr op_ptr) {
    dato->identificador = id;
    dato->calidad_senal = sq;
    dato->ptr_operador = op_ptr;
}

//La funcion no devvulve valor porque modifica direcamente la estructura apuntada por dato
// La funcioon fill_in asigna valores a los campos sin necesidad de devolver un resultado
