#define SIZE 100
/* Informacion sobre la celda */
struct informacion_celda 
{
    char nombre[SIZE];                /* Nombre de la celda */                           //ocupa el valor asignado en size (100 bytes)
    unsigned int identificador;      /* Numero identificador */                         //(4 bytes)
    float calidad_senal;              /* Calidad de la senal (entre 0 y 100) */         // (4 bytes)
    struct informacion_operador *ptr_operador; /* Puntero a una segunda estructura */   // Un puntero ocupa 8 bytes
};

/* Informacion sobre el operador */
struct informacion_operador 
{
    char nombre[SIZE];           /* Cadena de texto con el nombre */
    unsigned int prioridad;     /* Prioridad de conexion */
    unsigned int ultima_comprob; /* Fecha de la oltima comprobacion */ 
};

/*Pregunta 1
¿Qué tamaño en bytes ocupa una variable de tipo struct informacion_celda en memoria?

  /* Al trabajar en un sistema de 64 bits tenemos que: 
  los punteros ocupan 8 bytes y que los enteros int y float necesitan un padding que ocupa 4 bytes, por lo tanto seria:
  nombre[100]: 100 bytes
  identifiacdor(int): 4 bytes
  padding: 4 bytes
  calidad_senal (float) 4 bytes
  ptr_operador 8 bytes
              total de bytes: 120 bytes    */ 


  /* Pregunta 2 
  Las siguientes dos líneas declaran dos variables. ¿Cuál de ellas ocupa más espacio en memoria?*/

struct informacion_celda a;
struct informacion_celda *b;

/*R: La variable que ocupa mas espacio en memoria es a, que es una estructura de tipo struct informacion_celda.*/


/*Pregunta 3
¿Que tamaños tienen las siguientes variables*/
struct informacion_celda *ptr1, *ptr2;
struct informacion_operador *i1, *i2;

/*R: Al tratarse de un sistema de 64 bits, los 4 punteros ocupan 8 bytes lo que serian 32 bytes en conjunto */


/* Pregunta 4
Si una variable de tipo struct informacion_celda está almacenada en la posición de memoria 100, 
¿qué dirección tienen cada uno de sus campos? */

/* variable --- direccion (base 100)
   Nombre    -     100-199
   identificador - 200-203
   padding       - 204-207
   calidad_senal - 208-211
   ptr_operador  - 212-220
*/

/* Pregunta 5
Si una variable de tipo struct informacion_celda* está almacenada en la posición de memoria 100, 
¿qué dirección tiene cada uno de sus campos?
R: Al tratarse de un puntero que solamente esta almacenado en la direccion 100 pero la estructura real esta en otro lugar de memoria 
-- Ocuparia 8 bytes en la direccion 100-107    */


/*Pregunta 6:
¿Qué cambios debes hacer en las definiciones de la parte izquierda para que sean equivalentes a las descripciones de la parte derecha?

struct informacion_celda c;                       // variable de tipo estructura informacion_celda
struct informacion_celda **c_ptr;                 // puntero a estructura informacion_celda;

struct informacion_celda c;                       // variable de tipo estructura informacion_celda
struct informacion_celda *c_ptr;                 // puntero a estructura informacion_celda;
*/

/*Pregunta 7
¿Se pueden hacer las siguientes asignaciones? ¿Qué declara exactamente la linea 3??*/

struct informacion_celda c;                //  Declara una variable de tipo struct informacion_celda
struct informacion_celda *c_ptr = &c;      //  Apunta el puntero c_ptr a la dirección de c
struct informacion_celda d;                //  Declara una nueva variable de tipo struct informacion_celda
struct informacion_celda *d_ptr = c_ptr;   //  Asigna c_ptr a d_ptr

/*Pregunta 8
Considera la siguiente declaración y asignación:*/
struct informacion_celda c;
struct informacion_celda *c_ptr;

c_ptr = *c;
/*¿Es correcta? Y si lo es, ¿Qué contiene la variable c_ptr (no se pregunta por lo que apunta, sino su contenido)?
R = No es correcta la asignacion, si fuera correcta ("c_ptr = &c;") contendria la direccion 
de memoria donde esta almacenada c

/*Pregunta 9
Si se declara una variable como “struct informacion_celda c;”, 
¿qué tipo de datos es el que devuelve la expresión “&c.ptr_operador”?
Respuesta = obtiene la direccion de memoria del puntero por lo que su tipo es
struct informacion_operador ** 