#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define ENTRADA 0
#define SALIDA 1

typedef struct {
    double tiempo;   // Momento en el que ocurre el evento en segundos
    int tipo;        // entrada(0) o salida(1)
    int id_auto;     
} Evento;

// Nodo de la lista doblemente enlazada
typedef struct Nodo {
    Evento evento;
    struct Nodo* siguiente;
    struct Nodo* anterior;
} Nodo;

// Cola de eventos implementada como lista doblemente enlazada
typedef struct {
    Nodo* inicio;    // Primer evento (menor tiempo)
    Nodo* fin;       // Último evento (mayor tiempo)
    int size;        // Número de eventos en la cola
} ColaEventos;

// Inicializa la cola de eventos
void inicializar_cola(ColaEventos* cola) {
    cola->inicio = NULL;
    cola->fin = NULL;
    cola->size = 0;
}

// Crea un nuevo nodo con el evento dado
Nodo* crear_nodo(Evento evento) {
    Nodo* nuevo_nodo = (Nodo*)malloc(sizeof(Nodo));
    if (nuevo_nodo == NULL) {
        fprintf(stderr, "Error: No se pudo asignar memoria para el nodo\n");
        exit(1);
    }
    nuevo_nodo->evento = evento;
    nuevo_nodo->siguiente = NULL;
    nuevo_nodo->anterior = NULL;
    return nuevo_nodo;
}

// Inserta un evento en la cola manteniendo el orden por tiempo (menor a mayor)
void insertar_evento(ColaEventos* cola, Evento evento) {
    Nodo* nuevo_nodo = crear_nodo(evento);
    
    // Si la cola está vacía
    if (cola->inicio == NULL) {
        cola->inicio = nuevo_nodo;
        cola->fin = nuevo_nodo;
    }
    // Si el nuevo evento debe ir al principio
    else if (evento.tiempo < cola->inicio->evento.tiempo) {
        nuevo_nodo->siguiente = cola->inicio;
        cola->inicio->anterior = nuevo_nodo;
        cola->inicio = nuevo_nodo;
    }
    // Si el nuevo evento debe ir al final
    else if (evento.tiempo >= cola->fin->evento.tiempo) {
        nuevo_nodo->anterior = cola->fin;
        cola->fin->siguiente = nuevo_nodo;
        cola->fin = nuevo_nodo;
    }
    // Insertar en el medio (búsqueda desde el inicio)
    else {
        Nodo* actual = cola->inicio;
        while (actual->siguiente != NULL && actual->siguiente->evento.tiempo < evento.tiempo) {
            actual = actual->siguiente;
        }
        
        nuevo_nodo->siguiente = actual->siguiente;
        nuevo_nodo->anterior = actual;
        
        if (actual->siguiente != NULL) {
            actual->siguiente->anterior = nuevo_nodo;
        }
        actual->siguiente = nuevo_nodo;
    }
    
    cola->size++;
}

// Obtiene y elimina el siguiente evento (el de menor tiempo)
Evento* obtener_siguiente_evento(ColaEventos* cola) {
    if (cola->inicio == NULL) {
        return NULL; // Cola vacía
    }
    
    Nodo* nodo_a_eliminar = cola->inicio;
    Evento* evento = (Evento*)malloc(sizeof(Evento));
    if (evento == NULL) {
        fprintf(stderr, "Error: No se pudo asignar memoria para el evento\n");
        exit(1);
    }
    
    *evento = nodo_a_eliminar->evento;
    
    // Actualizar los punteros de la cola
    cola->inicio = nodo_a_eliminar->siguiente;
    
    if (cola->inicio != NULL) {
        cola->inicio->anterior = NULL;
    } else {
        // Si era el último nodo, actualizar fin también
        cola->fin = NULL;
    }
    
    free(nodo_a_eliminar);
    cola->size--;
    
    return evento;
}

// Libera toda la memoria de la cola
void liberar_cola(ColaEventos* cola) {
    while (cola->inicio != NULL) {
        Nodo* temp = cola->inicio;
        cola->inicio = cola->inicio->siguiente;
        free(temp);
    }
    cola->fin = NULL;
    cola->size = 0;
}

// Verifica si la cola está vacía
int cola_vacia(ColaEventos* cola) {
    return cola->inicio == NULL;
}

// Obtiene el tamaño de la cola
int obtener_size_cola(ColaEventos* cola) {
    return cola->size;
}

// Genera tiempo aleatorio entre llegadas (2-4 segundos)
double generar_tiempo_entre_llegadas() {
    return 2.0 + ((double)rand() / RAND_MAX) * 2.0; // entre 2.0 y 4.0 segundos
}

// Genera tiempo aleatorio de permanencia (5-6 segundos)
double generar_tiempo_de_salida() {
    return 5.0 + ((double)rand() / RAND_MAX) * 1.0; // entre 5.0 y 6.0 segundos
}

// Crea un evento con los parámetros dados
Evento crear_evento(double tiempo, int tipo, int id_auto) {
    Evento evento;
    evento.tiempo = tiempo;
    evento.tipo = tipo;
    evento.id_auto = id_auto;
    return evento;
}

// Imprime el estado actual de la cola (para debugging)
void imprimir_cola(ColaEventos* cola) {
    printf("Estado de la cola (%d eventos):\n", cola->size);
    Nodo* actual = cola->inicio;
    while (actual != NULL) {
        printf("  Auto %d, %s en t=%.2f\n", 
               actual->evento.id_auto,
               actual->evento.tipo == ENTRADA ? "ENTRADA" : "SALIDA",
               actual->evento.tiempo);
        actual = actual->siguiente;
    }
    printf("\n");
}

int main() {
    // Inicializar generador de números aleatorios
    srand((unsigned int)time(NULL));
    
    ColaEventos cola;
    inicializar_cola(&cola);
    
    double reloj = 0.0;
    int id = 1;
    const int MAX_AUTOS = 10;

    // Crear y agregar el primer evento de entrada
    Evento primer_evento = crear_evento(0.0, ENTRADA, id++);
    insertar_evento(&cola, primer_evento);

    printf("=== SIMULACIÓN DE COLA DE EVENTOS ===\n");
    printf("Procesando hasta %d autos...\n\n", MAX_AUTOS);

    int eventos_procesados = 0;
    
    // Bucle principal de simulación
    while (!cola_vacia(&cola) && eventos_procesados < 50) { // Límite de seguridad
        //printf("DEBUG: Inicio de iteración %d, cola tiene %d eventos\n", 
        //       eventos_procesados + 1, obtener_size_cola(&cola));
        
        Evento* evento_actual = obtener_siguiente_evento(&cola);
        if (evento_actual == NULL) {
           // printf("DEBUG: evento_actual es NULL, saliendo\n");
            break;
        }
        
        reloj = evento_actual->tiempo;
        //printf("DEBUG: Procesando evento tipo %d, auto %d, tiempo %.2f\n", 
        //       evento_actual->tipo, evento_actual->id_auto, reloj);

        if (evento_actual->tipo == ENTRADA) {
            printf("Auto %d ENTRA en t=%.2f\n", evento_actual->id_auto, reloj);

            // Programar la salida de este auto
            double tiempo_salida = reloj + generar_tiempo_de_salida();
            //printf("DEBUG: Programando salida en t=%.2f\n", tiempo_salida);
            
            Evento evento_salida = crear_evento(tiempo_salida, SALIDA, evento_actual->id_auto);
            insertar_evento(&cola, evento_salida);

            // Generar la siguiente entrada si no hemos llegado al límite
            if (id <= MAX_AUTOS) {
                double tiempo_nueva_entrada = reloj + generar_tiempo_entre_llegadas();
                // printf("DEBUG: Programando nueva entrada (auto %d) en t=%.2f\n", id, tiempo_nueva_entrada);
                
                Evento nueva_entrada = crear_evento(tiempo_nueva_entrada, ENTRADA, id++);
                insertar_evento(&cola, nueva_entrada);
            } else {
                //printf("DEBUG: Ya se programaron todos los autos (límite %d alcanzado)\n", MAX_AUTOS);
            }

        } else if (evento_actual->tipo == SALIDA) {
            printf("Auto %d SALE en t=%.2f\n", evento_actual->id_auto, reloj);
        }
        
        //printf("DEBUG: Cola después del procesamiento: %d eventos\n\n", obtener_size_cola(&cola));
        
        free(evento_actual); // Liberar memoria del evento procesado
        eventos_procesados++;
    }

    printf("\n=== SIMULACIÓN COMPLETADA ===\n");
    printf("Eventos procesados: %d\n", eventos_procesados);
    printf("Tiempo total de simulación: %.2f segundos\n", reloj);
    
    // Limpiar memoria restante
    liberar_cola(&cola);
    
    return 0;
}