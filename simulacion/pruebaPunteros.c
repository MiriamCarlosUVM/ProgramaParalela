#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define ENTRADA 0
#define AVANCE 1
#define SALIDA 2

// Configuración de la calle
#define LONGITUD_CALLE 100  // metros
#define NUM_SECCIONES 10    // número de secciones
#define LONGITUD_SECCION (LONGITUD_CALLE / NUM_SECCIONES)  // 10 metros por sección
#define MAX_AUTOS 100       // máximo número de autos en el sistema

// Estados posibles de un auto
typedef enum {
    AUTO_ESPERANDO_ENTRADA,
    AUTO_EN_CALLE,
    AUTO_ESPERANDO_AVANCE,
    AUTO_SALIDO
} EstadoAuto;

typedef struct {
    double tiempo;   // Momento en el que ocurre el evento en segundos
    int tipo;        // entrada(0), avance(1), salida(2)
    int id_auto;     
    int seccion;     // Sección objetivo del evento
} Evento;

// Estructura para representar un auto individual
typedef struct {
    int id;
    int seccion_actual;        // -1 si no está en la calle
    EstadoAuto estado;
    double tiempo_entrada;     // Cuándo entró a la calle
    double tiempo_ultima_accion; // Último movimiento
    int secciones_recorridas;  // Contador de secciones atravesadas
    double tiempo_total_calle; // Tiempo total en la calle
} Auto;

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

// Estructura para representar el estado de la calle
typedef struct {
    int ocupada[NUM_SECCIONES];  // ID del auto que ocupa cada sección (0 = libre)
    int total_autos_calle;       // Número total de autos en la calle
    double tiempo_promedio_transito; // Tiempo promedio de tránsito
    int autos_completados;       // Autos que han salido completamente
} EstadoCalle;

// Estructura para manejar todos los autos del sistema
typedef struct {
    Auto autos[MAX_AUTOS];
    int num_autos_creados;
    int num_autos_activos;
} SistemaAutos;

// Inicializa la cola de eventos
void inicializar_cola(ColaEventos* cola) {
    cola->inicio = NULL;
    cola->fin = NULL;
    cola->size = 0;
}

// Inicializa el estado de la calle
void inicializar_calle(EstadoCalle* calle) {
    for (int i = 0; i < NUM_SECCIONES; i++) {
        calle->ocupada[i] = 0;
    }
    calle->total_autos_calle = 0;
    calle->tiempo_promedio_transito = 0.0;
    calle->autos_completados = 0;
}

// Inicializa el sistema de autos
void inicializar_sistema_autos(SistemaAutos* sistema) {
    for (int i = 0; i < MAX_AUTOS; i++) {
        sistema->autos[i].id = 0;
        sistema->autos[i].seccion_actual = -1;
        sistema->autos[i].estado = AUTO_ESPERANDO_ENTRADA;
        sistema->autos[i].tiempo_entrada = 0.0;
        sistema->autos[i].tiempo_ultima_accion = 0.0;
        sistema->autos[i].secciones_recorridas = 0;
        sistema->autos[i].tiempo_total_calle = 0.0;
    }
    sistema->num_autos_creados = 0;
    sistema->num_autos_activos = 0;
}

// Crea un nuevo auto en el sistema
Auto* crear_auto(SistemaAutos* sistema, int id) {
    if (sistema->num_autos_creados >= MAX_AUTOS) {
        return NULL;
    }
    
    Auto* auto_nuevo = &sistema->autos[sistema->num_autos_creados];
    auto_nuevo->id = id;
    auto_nuevo->seccion_actual = -1;
    auto_nuevo->estado = AUTO_ESPERANDO_ENTRADA;
    auto_nuevo->tiempo_entrada = 0.0;
    auto_nuevo->tiempo_ultima_accion = 0.0;
    auto_nuevo->secciones_recorridas = 0;
    auto_nuevo->tiempo_total_calle = 0.0;
    
    sistema->num_autos_creados++;
    sistema->num_autos_activos++;
    
    return auto_nuevo;
}

// Busca un auto por ID
Auto* buscar_auto_por_id(SistemaAutos* sistema, int id) {
    for (int i = 0; i < sistema->num_autos_creados; i++) {
        if (sistema->autos[i].id == id) {
            return &sistema->autos[i];
        }
    }
    return NULL;
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
    
    if (cola->inicio == NULL) {
        cola->inicio = nuevo_nodo;
        cola->fin = nuevo_nodo;
    }
    else if (evento.tiempo < cola->inicio->evento.tiempo) {
        nuevo_nodo->siguiente = cola->inicio;
        cola->inicio->anterior = nuevo_nodo;
        cola->inicio = nuevo_nodo;
    }
    else if (evento.tiempo >= cola->fin->evento.tiempo) {
        nuevo_nodo->anterior = cola->fin;
        cola->fin->siguiente = nuevo_nodo;
        cola->fin = nuevo_nodo;
    }
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

// Obtiene y elimina el siguiente evento
Evento* obtener_siguiente_evento(ColaEventos* cola) {
    if (cola->inicio == NULL) {
        return NULL;
    }
    
    Nodo* nodo_a_eliminar = cola->inicio;
    Evento* evento = (Evento*)malloc(sizeof(Evento));
    if (evento == NULL) {
        fprintf(stderr, "Error: No se pudo asignar memoria para el evento\n");
        exit(1);
    }
    
    *evento = nodo_a_eliminar->evento;
    
    cola->inicio = nodo_a_eliminar->siguiente;
    
    if (cola->inicio != NULL) {
        cola->inicio->anterior = NULL;
    } else {
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

// Genera tiempo aleatorio entre llegadas (2-4 segundos)
double generar_tiempo_entre_llegadas() {
    return 2.0 + ((double)rand() / RAND_MAX) * 2.0;
}

// Genera tiempo aleatorio para avanzar una sección (1-2 segundos)
double generar_tiempo_avance_seccion() {
    return 1.0 + ((double)rand() / RAND_MAX) * 1.0;
}

// Crea un evento
Evento crear_evento(double tiempo, int tipo, int id_auto, int seccion) {
    Evento evento;
    evento.tiempo = tiempo;
    evento.tipo = tipo;
    evento.id_auto = id_auto;
    evento.seccion = seccion;
    return evento;
}

// Verifica si una sección está libre (usando estado de calle)
int seccion_libre(EstadoCalle* calle, int seccion) {
    return calle->ocupada[seccion] == 0;
}

// Ocupa una sección (actualiza tanto calle como auto)
void ocupar_seccion(EstadoCalle* calle, SistemaAutos* sistema, int seccion, int id_auto, double tiempo) {
    calle->ocupada[seccion] = id_auto;
    
    Auto* auto_ptr = buscar_auto_por_id(sistema, id_auto);
    if (auto_ptr != NULL) {
        auto_ptr->seccion_actual = seccion;
        auto_ptr->tiempo_ultima_accion = tiempo;
        auto_ptr->secciones_recorridas++;
    }
}

// Libera una sección (actualiza tanto calle como auto)
void liberar_seccion(EstadoCalle* calle, SistemaAutos* sistema, int seccion) {
    int id_auto = calle->ocupada[seccion];
    calle->ocupada[seccion] = 0;
    
    Auto* auto_ptr = buscar_auto_por_id(sistema, id_auto);
    if (auto_ptr != NULL) {
        auto_ptr->seccion_actual = -1;
    }
}

// Actualiza el estado de un auto
void actualizar_estado_auto(SistemaAutos* sistema, int id_auto, EstadoAuto nuevo_estado, double tiempo) {
    Auto* auto_ptr = buscar_auto_por_id(sistema, id_auto);
    if (auto_ptr != NULL) {
        auto_ptr->estado = nuevo_estado;
        auto_ptr->tiempo_ultima_accion = tiempo;
    }
}

// Registra la entrada de un auto a la calle
void registrar_entrada_auto(SistemaAutos* sistema, EstadoCalle* calle, int id_auto, double tiempo) {
    Auto* auto_ptr = buscar_auto_por_id(sistema, id_auto);
    if (auto_ptr != NULL) {
        auto_ptr->tiempo_entrada = tiempo;
        auto_ptr->estado = AUTO_EN_CALLE;
        calle->total_autos_calle++;
    }
}

// Registra la salida de un auto de la calle
void registrar_salida_auto(SistemaAutos* sistema, EstadoCalle* calle, int id_auto, double tiempo) {
    Auto* auto_ptr = buscar_auto_por_id(sistema, id_auto);
    if (auto_ptr != NULL) {
        auto_ptr->tiempo_total_calle = tiempo - auto_ptr->tiempo_entrada;
        auto_ptr->estado = AUTO_SALIDO;
        calle->total_autos_calle--;
        calle->autos_completados++;
        sistema->num_autos_activos--;
        
        // Actualizar tiempo promedio
        calle->tiempo_promedio_transito = 
            ((calle->tiempo_promedio_transito * (calle->autos_completados - 1)) + 
             auto_ptr->tiempo_total_calle) / calle->autos_completados;
    }
}

// Convierte estado de auto a string
const char* estado_auto_a_string(EstadoAuto estado) {
    switch (estado) {
        case AUTO_ESPERANDO_ENTRADA: return "ESPERANDO_ENTRADA";
        case AUTO_EN_CALLE: return "EN_CALLE";
        case AUTO_ESPERANDO_AVANCE: return "ESPERANDO_AVANCE";
        case AUTO_SALIDO: return "SALIDO";
        default: return "DESCONOCIDO";
    }
}

// Convierte tipo de evento a string
const char* tipo_evento_a_string(int tipo) {
    switch (tipo) {
        case ENTRADA: return "ENTRADA";
        case AVANCE: return "AVANCE";
        case SALIDA: return "SALIDA";
        default: return "DESCONOCIDO";
    }
}

// Imprime información detallada de un auto
void imprimir_info_auto(SistemaAutos* sistema, int id_auto) {
    Auto* auto_ptr = buscar_auto_por_id(sistema, id_auto);
    if (auto_ptr != NULL) {
        printf("  Auto %d: Sección=%d, Estado=%s, Secciones recorridas=%d\n",
               auto_ptr->id,
               auto_ptr->seccion_actual,
               estado_auto_a_string(auto_ptr->estado),
               auto_ptr->secciones_recorridas);
    }
}

// Imprime el estado completo del sistema
void imprimir_estado_sistema(EstadoCalle* calle, SistemaAutos* sistema, double tiempo) {
    printf("\n=== ESTADO DEL SISTEMA EN t=%.2f ===\n", tiempo);
    
    // Estado de la calle
    printf("Calle: ");
    for (int i = 0; i < NUM_SECCIONES; i++) {
        if (calle->ocupada[i] == 0) {
            printf("[ - ] ");
        } else {
            printf("[%2d] ", calle->ocupada[i]);
        }
    }
    printf("\n");
    
    // Métricas
    printf("Autos en calle: %d | Autos completados: %d | Tiempo promedio: %.2f seg\n",
           calle->total_autos_calle, calle->autos_completados, calle->tiempo_promedio_transito);
    
    // Estado de autos activos
    printf("Autos activos:\n");
    for (int i = 0; i < sistema->num_autos_creados; i++) {
        if (sistema->autos[i].estado != AUTO_SALIDO) {
            imprimir_info_auto(sistema, sistema->autos[i].id);
        }
    }
    printf("\n");
}

int main() {
    srand((unsigned int)time(NULL));
    
    ColaEventos cola;
    EstadoCalle calle;
    SistemaAutos sistema;
    
    inicializar_cola(&cola);
    inicializar_calle(&calle);
    inicializar_sistema_autos(&sistema);
    
    double reloj = 0.0;
    int id = 1;
    const int MAX_AUTOS_SIMULAR = 15;

    // Crear el primer auto y evento
    crear_auto(&sistema, id);
    Evento primer_evento = crear_evento(0.0, ENTRADA, id++, 0);
    insertar_evento(&cola, primer_evento);

    printf("=== SIMULACIÓN HÍBRIDA DE CALLE CON SECCIONES ===\n");
    printf("Configuración: %d metros, %d secciones de %d metros\n\n", 
           LONGITUD_CALLE, NUM_SECCIONES, LONGITUD_SECCION);

    int eventos_procesados = 0;
    
    while (!cola_vacia(&cola) && eventos_procesados < 100) {
        Evento* evento_actual = obtener_siguiente_evento(&cola);
        if (evento_actual == NULL) break;
        
        reloj = evento_actual->tiempo;
        Auto* auto_ptr = buscar_auto_por_id(&sistema, evento_actual->id_auto);

        printf("EVENTO %d: Auto %d - %s (t=%.2f)\n", 
               eventos_procesados + 1, evento_actual->id_auto, 
               tipo_evento_a_string(evento_actual->tipo), reloj);

        if (evento_actual->tipo == ENTRADA) {
            if (seccion_libre(&calle, 0)) {
                ocupar_seccion(&calle, &sistema, 0, evento_actual->id_auto, reloj);
                registrar_entrada_auto(&sistema, &calle, evento_actual->id_auto, reloj);
                
                printf("  → Auto %d ENTRA a la calle (sección 0)\n", evento_actual->id_auto);
                
                // Programar avance
                double tiempo_avance = reloj + generar_tiempo_avance_seccion();
                Evento evento_avance = crear_evento(tiempo_avance, AVANCE, evento_actual->id_auto, 1);
                insertar_evento(&cola, evento_avance);
                
                // Programar siguiente entrada
                if (id <= MAX_AUTOS_SIMULAR) {
                    crear_auto(&sistema, id);
                    double tiempo_nueva_entrada = reloj + generar_tiempo_entre_llegadas();
                    Evento nueva_entrada = crear_evento(tiempo_nueva_entrada, ENTRADA, id++, 0);
                    insertar_evento(&cola, nueva_entrada);
                }
            } else {
                printf("  → Auto %d NO PUEDE ENTRAR (sección 0 ocupada)\n", evento_actual->id_auto);
                actualizar_estado_auto(&sistema, evento_actual->id_auto, AUTO_ESPERANDO_ENTRADA, reloj);
                
                double tiempo_reintento = reloj + 0.5;
                Evento reintento = crear_evento(tiempo_reintento, ENTRADA, evento_actual->id_auto, 0);
                insertar_evento(&cola, reintento);
            }

        } else if (evento_actual->tipo == AVANCE) {
            int seccion_actual = auto_ptr ? auto_ptr->seccion_actual : -1;
            int seccion_destino = evento_actual->seccion;
            
            if (seccion_destino >= NUM_SECCIONES) {
                // Salir de la calle
                if (seccion_actual >= 0) {
                    liberar_seccion(&calle, &sistema, seccion_actual);
                }
                registrar_salida_auto(&sistema, &calle, evento_actual->id_auto, reloj);
                printf("  → Auto %d SALE de la calle (%.2f seg total)\n", 
                       evento_actual->id_auto, auto_ptr ? auto_ptr->tiempo_total_calle : 0.0);
            } else {
                if (seccion_libre(&calle, seccion_destino)) {
                    if (seccion_actual >= 0) {
                        liberar_seccion(&calle, &sistema, seccion_actual);
                    }
                    ocupar_seccion(&calle, &sistema, seccion_destino, evento_actual->id_auto, reloj);
                    actualizar_estado_auto(&sistema, evento_actual->id_auto, AUTO_EN_CALLE, reloj);
                    
                    printf("  → Auto %d AVANZA a sección %d\n", evento_actual->id_auto, seccion_destino);
                    
                    // Programar siguiente avance
                    double tiempo_siguiente = reloj + generar_tiempo_avance_seccion();
                    Evento siguiente = crear_evento(tiempo_siguiente, AVANCE, 
                                                   evento_actual->id_auto, seccion_destino + 1);
                    insertar_evento(&cola, siguiente);
                } else {
                    printf("  → Auto %d NO PUEDE AVANZAR a sección %d (ocupada)\n", 
                           evento_actual->id_auto, seccion_destino);
                    actualizar_estado_auto(&sistema, evento_actual->id_auto, AUTO_ESPERANDO_AVANCE, reloj);
                    
                    double tiempo_reintento = reloj + 0.5;
                    Evento reintento = crear_evento(tiempo_reintento, AVANCE, 
                                                   evento_actual->id_auto, seccion_destino);
                    insertar_evento(&cola, reintento);
                }
            }
        }
        
        // Mostrar estado cada 5 eventos para no saturar la salida
        if (eventos_procesados % 5 == 0) {
            imprimir_estado_sistema(&calle, &sistema, reloj);
        }
        
        free(evento_actual);
        eventos_procesados++;
    }

    // Reporte final
    printf("\n=== REPORTE FINAL ===\n");
    printf("Eventos procesados: %d\n", eventos_procesados);
    printf("Tiempo total: %.2f segundos\n", reloj);
    printf("Autos que completaron el trayecto: %d\n", calle.autos_completados);
    printf("Tiempo promedio de tránsito: %.2f segundos\n", calle.tiempo_promedio_transito);
    printf("Autos restantes en calle: %d\n", calle.total_autos_calle);
    
    liberar_cola(&cola);
    return 0;
}
