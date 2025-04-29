#include <stdio.h>
#include <string.h>

#define SIZE 4
#define SIZE_NAME 20

/*Pregunta 10

Dado el siguiente código: */

struct pack3 {
    int a;
};

struct pack2 {
    int b;
    struct pack3 *next;
};

struct pack1 {
    int c;
    struct pack2 *next;
};

void pregunta10() {
    struct pack1 data1, *data_ptr;
    struct pack2 data2;
    struct pack3 data3;

    data1.c = 30;
    data2.b = 20;
    data3.a = 10;
    data_ptr = &data1;
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
}


/*Pregunta 11:
Supongamos que se escriben las siguientes declaraciones y asignaciones en un programa:*/

struct info_operador {
    int id;
    char nombre[20];
};

struct info_celda {
    int dato;
    unsigned int identificador;
    float calidad_senal;
    struct info_operador *ptr_operador;
};

typedef struct info_celda info_celda;
typedef struct info_celda* info_celda_ptr;
typedef struct info_operador* info_operador_ptr;

void preguntall() {
    // Definiciones necesarias (deberían estar arriba en el código)
    typedef struct info_operador {
        int id;
        char nombre[20];
    } info_operador, *info_operador_ptr;
    
    typedef struct info_celda {
        unsigned int identificador;
        float calidad_senal;
        info_operador *ptr_operador;
    } info_celda, *info_celda_ptr;

    // Cuerpo de la función
    info_celda c_var;
    info_celda_ptr c_ptr = &c_var;
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
}


/*Pregunta 12
Supón ahora que la aplicación en la que se usan estas estructuras 
necesita almacenar la información para un máximo de 10 celdas. 
¿Qué estructura de datos definirías?

-Si el numero de celdas es fijo podria usarse un arreglo*/
struct info_celda celdas[10];

//Esto permite acceder a cada celda por su indice 
// celdas[0].ptr_operador = &op;


/*Pregunta 13 
Escribe un bucle con la variable declarada en el ejercicio anterior 
que asigne al campo ptr_operador el valor vacío*/

void pregunta13() {
    for (int i = 0; i < 10; i++) {
        celdas[i].ptr_operador = NULL;
    }
}


/*Pregunta 14 
La información sobre las celdas que se almacena en la estructura del ejercicio anterior 
la debe utilizar la aplicación para recordar cuál de ellas es la más próxima. 
Esta información puede cambiar a lo largo del tiempo. 
¿Qué tipo de datos sugieres para almacenar esta información? Ofrece dos alternativas.  */

/*
Alternativa 1:
Definir un puntero que almacena la direccion de la celda mas proxima*/

struct info_celda *celda_mas_proxima = NULL;
//Para actualizar a la mas proxima 
// celda_mas_proxima = &celdas[indice];  // Donde 'indice' es la posición de la celda más próxima
//Para acceder 
// if (celda_mas_proxima != NULL) {
//     printf("Celda más próxima tiene dato: %d\n", celda_mas_proxima->dato);
// }

/*
Alternativa 2:
Definir una variable entera que almacene la posicion en el arreglo de la celda mas proxima*/

int indice_mas_proximo = -1;  // -1 indica que no hay celda seleccionada aún

//Para actualizar 
// indice_mas_proximo = 3;  // Si la celda más próxima está en celdas[3]

//Para acceder 
// if (indice_mas_proximo != -1) {
//     printf("Celda más próxima tiene dato: %d\n", celdas[indice_mas_proximo].dato);
// }


/*Pregunta 15 
Se dispone de una estructura de tipo “info_celda c” que a su vez, en el campo “ptr_operador” tiene un puntero a una estructura 
“info_operador cr”. 
¿Qué tamaño tiene la estructura “c”? ¿Qué tamaño total ocupa la información incluyendo la información sobre el operador?*/

//Es probable que se agreguen 4 bytes de padding, dejando el tamaño final en 16 bytes en 64 bits 

// struct info_operador {
//     int id;             // 4 bytes
//     char nombre[20];    // 20 bytes
//                         //4 padding
// };

// struct info_celda {
//     int dato;                               //   4 bytes
//     struct info_operador *ptr_operador;     //   8 bytes 
//                                             // 12 bytes totales 
// };

/*¿Qué tamaño total ocupa la información incluyendo la información sobre el operador?
 44 bytes

Si ptr_operador apunta a una instancia dinámica de info_operador, la memoria ocupada incluye ambas estructuras */


/*
Pregunta 16 
Escribe el cuerpo de la siguiente función:*/

void fill_in(info_celda_ptr dato, unsigned int id, float sq, info_operador_ptr op_ptr) {
    dato->identificador = id;
    dato->calidad_senal = sq;
    dato->ptr_operador = op_ptr;
}

//La funcion no devuelve valor porque modifica direcamente la estructura apuntada por dato
// La funcion fill_in asigna valores a los campos sin necesidad de devolver un resultado


/*
Pregunta 17 
La versión 1 del programa imprime el valor 10 por pantalla, y la versión 2 imprime el valor 20. Explica por qué.
      version 1*/

struct package {
    int q;
};

void set_value(struct package data, int value) {
    data.q = value;     // Se modifica la COPIA de la estructura, no la original
}

void set_value_ptr(struct package *d_ptr, int value) {
    d_ptr->q = value;    // Se modifica directamente la estructura original
}


/*Pregunta 18 
¿Qué cantidad de memoria ocupan estas dos estructuras? ¿Cuál es su diferencia?*/

info_celda t[SIZE];
//La memoria total ocupada depende del tamaño de info_celda y la cantidad de elementos SIZE
//Si sizeof(info_celda) = 32 bytes y SIZE = 10, el array ocuparía:
//32×10=320 bytes

info_celda *tp[SIZE];

//Al ser un array de puntero a estructura, recordemos que los punteros valen 
//8 bytes en un sistema de 64 bits por lo que si el size sigue siendo 10 serian 
// 80 bytes en total 
//8 × 10= 80 bytes


/*Pregunta 19
Una aplicación de gestión de fotografías en tu móvil tiene definido el catálogo de fotos de la siguiente forma:*/

struct picture_info {
    char name[SIZE_NAME];
    int date_time;
} pictures[SIZE];

/*¿Qué tamaño tiene esta estructura de datos?
R= Tenemos un array de tamaño SIZE_NAME que ocupa size_name bytes
mas el entero de tamaño de 4 bytes, sumando un posible padding de 4 bytes
dando un total de = SIZE_NAME bytes + 4 bytes + 4 bytes */

//Declaracion de la segunda tabla 
struct picture_info *picture_ptrs [SIZE];

//Llenamos la tabla 
void llenar_picture_ptrs() {
    for (int i = 0; i < SIZE; i++) {
        picture_ptrs[i] = &pictures[i];
    }
}


/* Pregunta 20 
Se dispone de la siguiente definición de datos: */

struct coordinates {
    int latitude;
    int longitude;
} places[SIZE];

//Valores iniciales
void llenar_places() {
    places[0].latitude  = 200;
    places[0].longitude = 300;
    places[1].latitude  = 400;
    places[1].longitude = 100;
    places[2].latitude  = 100;
    places[2].longitude = 400;
    places[3].latitude  = 300;
    places[3].longitude = 200;
}
/*La tabla almacena cuatro puntos obtenidos del GPS de tu móvil, cada uno de ellos con su latitud y longitud que son números enteros. 
Hay una aplicación que ha obtenido estos puntos en este orden, pero necesita acceder a los datos de tres formas distintas.
La primera es en el orden en que han sido obtenidos, y por tanto, tal y como está la tabla. 
El segundo es ordenados crecientemente por latitud y el tercero ordenados crecientemente por longitud.
El acceso a estos datos en base a estos tres órdenes es continuo. 
Una primera solución podría ser reordenar los elementos de la tabla cada vez que se cambia de orden 
(por ejemplo, llega una petición de datos ordenados por latitud, pero la anterior se ordenaron por longitud, y entonces se reordena la tabla).
Pero es extremadamente ineficiente.
¿Por qué? 

-Si constantemente cambiamos entre estos ordenes, estaremos ordenando la tabla una y otra vez con un algoritmo de ordenacion para cada uno,
ademas, si SIZE es grande, ordenar los datos cada vez que cambia la consulta consume tiempo

¿Crees que utilizando punteros puedes ofrecer una alternativa más eficiente que evite reordenar los datos continuamente?*/

//Claro, en lugar de reordenar los datos originales se pueden crear arreglos de punteros que apunten a los elementos de places
//cada uno ordenado según el criterio deseado, así solo ordenas los punteros una vez para cada criterio. Esto reduce procesamiento y evita copias


int main() {
    // Aquí podrías llamar funciones de test si lo necesitas
    return 0;
}
