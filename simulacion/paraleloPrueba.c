#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <string.h>
#include <omp.h>

// ============================================================================
// DEFINICIÓN DE CONSTANTES PARA INTERSECCIÓN
// ============================================================================

#define ENTRADA_NORTE 0
#define ENTRADA_ESTE 1
#define ACTUALIZACION_VEHICULO 2
#define SALIDA_SUR 3
#define SALIDA_OESTE 4

// Direcciones de movimiento
typedef enum {
    NORTE_A_SUR,
    ESTE_A_OESTE
} DireccionCalle;

// Estados de la intersección
typedef enum {
    NORTE_SUR_VERDE,
    ESTE_OESTE_VERDE,
    TRANSICION
} EstadoInterseccion;

// Parámetros configurables de simulación
typedef struct {
    int num_secciones;
    double longitud_seccion;
    double longitud_calle_ns;     // Norte-Sur
    double longitud_calle_eo;     // Este-Oeste
    double posicion_interseccion; // Posición de la intersección en ambas calles
    double paso_simulacion;
    int max_autos_por_calle;
    double intervalo_entrada_vehiculos;
    double tiempo_limite_simulacion;
    double ancho_interseccion;    // Tamaño de la zona de conflicto
} ConfiguracionSimulacion;

// Configuración por defecto
ConfiguracionSimulacion config = {
    .num_secciones = 20,
    .longitud_seccion = 10.0,
    .longitud_calle_ns = 200.0,
    .longitud_calle_eo = 200.0,
    .posicion_interseccion = 100.0,
    .paso_simulacion = 0.05,
    .max_autos_por_calle = 25,
    .intervalo_entrada_vehiculos = 2.0,
    .tiempo_limite_simulacion = 0.0,
    .ancho_interseccion = 8.0
};

// ============================================================================
// ENUMERACIONES Y ESTRUCTURAS MEJORADAS PARA INTERSECCIÓN
// ============================================================================

typedef enum {
    ENTRANDO,
    ACELERANDO,
    VELOCIDAD_CONSTANTE,
    DESACELERANDO,
    FRENANDO,
    DETENIDO,
    ESPERANDO_PASO,
    CRUZANDO_INTERSECCION,
    SALIENDO
} EstadoVehiculo;

typedef struct {
    int id;
    DireccionCalle direccion;
    double posicion;
    int seccion;
    double velocidad;
    double aceleracion;
    EstadoVehiculo estado;
    EstadoVehiculo estado_anterior;
    
    // Métricas detalladas
    double tiempo_entrada;
    double tiempo_en_estado;
    double tiempo_total_frenado;
    double tiempo_total_detenido;
    double tiempo_esperando_interseccion;
    double tiempo_cruzando;
    double tiempo_lento;
    int eventos_reanudacion;
    double tiempo_llegada_interseccion;
    double tiempo_salida;
    double velocidad_promedio;
    double distancia_recorrida;
    
    // Campos para debugging y paralelismo
    int actualizaciones_count;
    double ultimo_cambio_estado;
    int thread_id;  // ID del thread que procesa este vehículo
} Vehiculo;

typedef struct {
    double tiempo;
    int tipo;
    int id_auto;
    DireccionCalle direccion;
    Vehiculo* vehiculo;
    int prioridad;
} Evento;

typedef struct Nodo {
    Evento evento;
    struct Nodo* siguiente;
    struct Nodo* anterior;
} Nodo;

typedef struct {
    Nodo* inicio;
    Nodo* fin;
    int size;
    int max_size_alcanzado;
    omp_lock_t lock; // Lock para acceso concurrente
} ColaEventos;

// Sistema escalable con arrays dinámicos para ambas calles
typedef struct {
    Vehiculo** vehiculos_norte_sur;
    Vehiculo** vehiculos_este_oeste;
    int num_vehiculos_ns;
    int num_vehiculos_eo;
    int capacidad_vehiculos_ns;
    int capacidad_vehiculos_eo;
    double tiempo_actual;
    
    // Estadísticas del sistema por calle
    int total_vehiculos_creados_ns;
    int total_vehiculos_creados_eo;
    int total_vehiculos_completados_ns;
    int total_vehiculos_completados_eo;
    double tiempo_promedio_recorrido_ns;
    double tiempo_promedio_recorrido_eo;
    
    // Control de intersección
    int vehiculos_en_interseccion;
    DireccionCalle direccion_actual_cruzando;
    double ultimo_cambio_interseccion;
    
    // Control de memoria y paralelismo
    size_t memoria_utilizada;
    omp_lock_t lock_sistema;
    omp_lock_t lock_interseccion;
} SistemaInterseccion;

typedef struct {
    double velocidad_maxima;
    double aceleracion_maxima;
    double desaceleracion_maxima;
    double desaceleracion_suave;
    double distancia_seguridad_min;
    double longitud_vehiculo;
    double tiempo_reaccion;
    double factor_congestion;
    double tiempo_minimo_cruce; // Tiempo mínimo para cruzar intersección
} ParametrosSimulacion;

typedef struct {
    double ultimo_cambio;
    EstadoInterseccion estado;
    double duracion_ns_verde;
    double duracion_eo_verde;
    double duracion_transicion;
    int ciclos_completados;
    omp_lock_t lock; // Lock para el semáforo
} ControlInterseccion;

// ============================================================================
// VARIABLES GLOBALES MEJORADAS CON LOCKS
// ============================================================================

SistemaInterseccion interseccion = {0};
ParametrosSimulacion params = {
    .velocidad_maxima = 10.0,
    .aceleracion_maxima = 2.5,
    .desaceleracion_maxima = -4.0,
    .desaceleracion_suave = -2.0,
    .distancia_seguridad_min = 4.0,
    .longitud_vehiculo = 5.0,
    .tiempo_reaccion = 1.0,
    .factor_congestion = 0.8,
    .tiempo_minimo_cruce = 2.0
};

ControlInterseccion semaforo = {
    .ultimo_cambio = 0.0,
    .estado = NORTE_SUR_VERDE,
    .duracion_ns_verde = 30.0,
    .duracion_eo_verde = 25.0,
    .duracion_transicion = 3.0,
    .ciclos_completados = 0
};

// Variables globales para estadísticas thread-safe
int eventos_procesados = 0;
omp_lock_t lock_eventos;

// ============================================================================
// FUNCIONES DE GESTIÓN DE MEMORIA Y PARALELISMO
// ============================================================================

void inicializar_locks() {
    omp_init_lock(&interseccion.lock_sistema);
    omp_init_lock(&interseccion.lock_interseccion);
    omp_init_lock(&semaforo.lock);
    omp_init_lock(&lock_eventos);
}

void destruir_locks() {
    omp_destroy_lock(&interseccion.lock_sistema);
    omp_destroy_lock(&interseccion.lock_interseccion);
    omp_destroy_lock(&semaforo.lock);
    omp_destroy_lock(&lock_eventos);
}

void inicializar_sistema() {
    interseccion.capacidad_vehiculos_ns = config.max_autos_por_calle + 10;
    interseccion.capacidad_vehiculos_eo = config.max_autos_por_calle + 10;
    
    interseccion.vehiculos_norte_sur = (Vehiculo**)calloc(interseccion.capacidad_vehiculos_ns, sizeof(Vehiculo*));
    interseccion.vehiculos_este_oeste = (Vehiculo**)calloc(interseccion.capacidad_vehiculos_eo, sizeof(Vehiculo*));
    
    if (!interseccion.vehiculos_norte_sur || !interseccion.vehiculos_este_oeste) {
        fprintf(stderr, "ERROR: No se pudo asignar memoria para vehículos\n");
        exit(1);
    }
    
    interseccion.memoria_utilizada = sizeof(SistemaInterseccion) + 
                                    (interseccion.capacidad_vehiculos_ns + interseccion.capacidad_vehiculos_eo) * sizeof(Vehiculo*);
    
    inicializar_locks();
    
    printf("Sistema de intersección inicializado:\n");
    printf("- Capacidad Norte-Sur: %d vehículos\n", interseccion.capacidad_vehiculos_ns);
    printf("- Capacidad Este-Oeste: %d vehículos\n", interseccion.capacidad_vehiculos_eo);
    printf("- Memoria: %zu bytes\n", interseccion.memoria_utilizada);
    printf("- Threads disponibles: %d\n", omp_get_max_threads());
}

void limpiar_sistema() {
    destruir_locks();
    
    if (interseccion.vehiculos_norte_sur) {
        free(interseccion.vehiculos_norte_sur);
        interseccion.vehiculos_norte_sur = NULL;
    }
    if (interseccion.vehiculos_este_oeste) {
        free(interseccion.vehiculos_este_oeste);
        interseccion.vehiculos_este_oeste = NULL;
    }
    printf("Sistema de intersección limpiado.\n");
}

// ============================================================================
// FUNCIONES DE CONTROL DE TRÁFICO PARA INTERSECCIÓN
// ============================================================================

Vehiculo* encontrar_vehiculo_adelante_interseccion(double posicion, DireccionCalle direccion, Vehiculo* vehiculo_actual) {
    Vehiculo* mas_cercano = NULL;
    double distancia_minima = (direccion == NORTE_A_SUR) ? config.longitud_calle_ns : config.longitud_calle_eo;
    double rango_busqueda = 50.0;
    
    Vehiculo** vehiculos_calle;
    int num_vehiculos;
    
    if (direccion == NORTE_A_SUR) {
        vehiculos_calle = interseccion.vehiculos_norte_sur;
        num_vehiculos = interseccion.num_vehiculos_ns;
    } else {
        vehiculos_calle = interseccion.vehiculos_este_oeste;
        num_vehiculos = interseccion.num_vehiculos_eo;
    }
    
    for (int i = 0; i < num_vehiculos; i++) {
        Vehiculo* v = vehiculos_calle[i];
        
        if (v == vehiculo_actual || !v || v->direccion != direccion) continue;
        
        double diferencia_pos = v->posicion - posicion;
        if (diferencia_pos > 0 && diferencia_pos <= rango_busqueda) {
            if (diferencia_pos < distancia_minima) {
                distancia_minima = diferencia_pos;
                mas_cercano = v;
            }
        }
    }
    
    return mas_cercano;
}

int puede_cruzar_interseccion(Vehiculo* vehiculo) {
    omp_set_lock(&interseccion.lock_interseccion);
    
    int puede_cruzar = 0;
    double pos_interseccion = config.posicion_interseccion;
    double inicio_interseccion = pos_interseccion - config.ancho_interseccion/2;
    double fin_interseccion = pos_interseccion + config.ancho_interseccion/2;
    
    // Verificar si el vehículo está cerca de la intersección
    if (vehiculo->posicion >= inicio_interseccion - 5.0 && vehiculo->posicion <= inicio_interseccion) {
        // Verificar semáforo
        omp_set_lock(&semaforo.lock);
        EstadoInterseccion estado_semaforo = semaforo.estado;
        omp_unset_lock(&semaforo.lock);
        
        if ((vehiculo->direccion == NORTE_A_SUR && estado_semaforo == NORTE_SUR_VERDE) ||
            (vehiculo->direccion == ESTE_A_OESTE && estado_semaforo == ESTE_OESTE_VERDE)) {
            
            // Verificar conflictos con vehículos en intersección
            if (interseccion.vehiculos_en_interseccion == 0 || 
                interseccion.direccion_actual_cruzando == vehiculo->direccion) {
                puede_cruzar = 1;
            }
        }
    }
    
    omp_unset_lock(&interseccion.lock_interseccion);
    return puede_cruzar;
}

void entrar_interseccion(Vehiculo* vehiculo) {
    omp_set_lock(&interseccion.lock_interseccion);
    
    if (interseccion.vehiculos_en_interseccion == 0) {
        interseccion.direccion_actual_cruzando = vehiculo->direccion;
    }
    
    interseccion.vehiculos_en_interseccion++;
    vehiculo->estado = CRUZANDO_INTERSECCION;
    vehiculo->tiempo_llegada_interseccion = interseccion.tiempo_actual;
    
    omp_unset_lock(&interseccion.lock_interseccion);
}

void salir_interseccion(Vehiculo* vehiculo) {
    omp_set_lock(&interseccion.lock_interseccion);
    
    interseccion.vehiculos_en_interseccion--;
    vehiculo->tiempo_cruzando += interseccion.tiempo_actual - vehiculo->tiempo_llegada_interseccion;
    
    if (interseccion.vehiculos_en_interseccion == 0) {
        interseccion.ultimo_cambio_interseccion = interseccion.tiempo_actual;
    }
    
    omp_unset_lock(&interseccion.lock_interseccion);
}

// ============================================================================
// CONTROL DEL SEMÁFORO INTELIGENTE
// ============================================================================

void actualizar_semaforo_interseccion(double tiempo_actual) {
    omp_set_lock(&semaforo.lock);
    
    double ciclo_total = semaforo.duracion_ns_verde + semaforo.duracion_eo_verde + 2 * semaforo.duracion_transicion;
    double t_ciclo = fmod(tiempo_actual, ciclo_total);
    
    EstadoInterseccion estado_anterior = semaforo.estado;
    
    if (t_ciclo < semaforo.duracion_ns_verde) {
        semaforo.estado = NORTE_SUR_VERDE;
    } else if (t_ciclo < semaforo.duracion_ns_verde + semaforo.duracion_transicion) {
        semaforo.estado = TRANSICION;
    } else if (t_ciclo < semaforo.duracion_ns_verde + semaforo.duracion_transicion + semaforo.duracion_eo_verde) {
        semaforo.estado = ESTE_OESTE_VERDE;
    } else {
        semaforo.estado = TRANSICION;
    }
    
    if (estado_anterior != semaforo.estado) {
        semaforo.ultimo_cambio = tiempo_actual;
        if (semaforo.estado == NORTE_SUR_VERDE && estado_anterior != NORTE_SUR_VERDE) {
            semaforo.ciclos_completados++;
        }
    }
    
    omp_unset_lock(&semaforo.lock);
}

// ============================================================================
// GESTIÓN DE EVENTOS THREAD-SAFE
// ============================================================================

void inicializar_cola_eventos(ColaEventos* cola) {
    omp_init_lock(&cola->lock);
    cola->inicio = NULL;
    cola->fin = NULL;
    cola->size = 0;
    cola->max_size_alcanzado = 0;
}

void destruir_cola_eventos(ColaEventos* cola) {
    omp_destroy_lock(&cola->lock);
}

void insertar_evento_thread_safe(ColaEventos* cola, Evento evento) {
    Nodo* nuevo = (Nodo*)malloc(sizeof(Nodo));
    if (!nuevo) return;
    
    nuevo->evento = evento;
    nuevo->siguiente = nuevo->anterior = NULL;
    
    omp_set_lock(&cola->lock);
    
    if (!cola->inicio) {
        cola->inicio = cola->fin = nuevo;
    } else if (evento.tiempo >= cola->fin->evento.tiempo) {
        nuevo->anterior = cola->fin;
        if (cola->fin) cola->fin->siguiente = nuevo;
        cola->fin = nuevo;
        if (!cola->inicio) cola->inicio = nuevo;
    } else if (evento.tiempo < cola->inicio->evento.tiempo) {
        nuevo->siguiente = cola->inicio;
        cola->inicio->anterior = nuevo;
        cola->inicio = nuevo;
    } else {
        Nodo* actual = cola->fin;
        while (actual && actual->evento.tiempo > evento.tiempo) {
            actual = actual->anterior;
        }
        
        nuevo->siguiente = actual->siguiente;
        nuevo->anterior = actual;
        if (actual->siguiente) actual->siguiente->anterior = nuevo;
        else cola->fin = nuevo;
        actual->siguiente = nuevo;
    }
    
    cola->size++;
    if (cola->size > cola->max_size_alcanzado) {
        cola->max_size_alcanzado = cola->size;
    }
    
    omp_unset_lock(&cola->lock);
}

Evento* obtener_siguiente_evento_thread_safe(ColaEventos* cola) {
    omp_set_lock(&cola->lock);
    
    if (!cola->inicio) {
        omp_unset_lock(&cola->lock);
        return NULL;
    }
    
    Nodo* nodo = cola->inicio;
    Evento* evento = (Evento*)malloc(sizeof(Evento));
    if (!evento) {
        omp_unset_lock(&cola->lock);
        return NULL;
    }
    
    *evento = nodo->evento;
    
    cola->inicio = nodo->siguiente;
    if (cola->inicio) {
        cola->inicio->anterior = NULL;
    } else {
        cola->fin = NULL;
    }
    
    free(nodo);
    cola->size--;
    
    omp_unset_lock(&cola->lock);
    return evento;
}

// ============================================================================
// FUNCIONES DE REPORTES PARA INTERSECCIÓN
// ============================================================================

const char* estado_str(EstadoVehiculo estado) {
    switch (estado) {
        case ENTRANDO: return "ENTRANDO";
        case ACELERANDO: return "ACELERANDO";
        case VELOCIDAD_CONSTANTE: return "VEL_CONST";
        case DESACELERANDO: return "DESACELERANDO";
        case FRENANDO: return "FRENANDO";
        case DETENIDO: return "DETENIDO";
        case ESPERANDO_PASO: return "ESPERANDO";
        case CRUZANDO_INTERSECCION: return "CRUZANDO";
        case SALIENDO: return "SALIENDO";
        default: return "DESCONOCIDO";
    }
}

const char* estado_interseccion_str(EstadoInterseccion estado) {
    switch (estado) {
        case NORTE_SUR_VERDE: return "NS_VERDE";
        case ESTE_OESTE_VERDE: return "EO_VERDE";
        case TRANSICION: return "TRANSICION";
        default: return "DESCONOCIDO";
    }
}

void imprimir_estado_interseccion() {
    omp_set_lock(&semaforo.lock);
    EstadoInterseccion estado_sem = semaforo.estado;
    omp_unset_lock(&semaforo.lock);
    
    omp_set_lock(&interseccion.lock_sistema);
    printf("\n=== INTERSECCIÓN t=%.2f | %s | NS:%d EO:%d | En cruce:%d ===\n", 
           interseccion.tiempo_actual, estado_interseccion_str(estado_sem),
           interseccion.num_vehiculos_ns, interseccion.num_vehiculos_eo,
           interseccion.vehiculos_en_interseccion);
    
    // Mostrar algunos vehículos de cada calle
    printf("NORTE-SUR:\n");
    int mostrar_ns = fmin(interseccion.num_vehiculos_ns, 4);
    for (int i = 0; i < mostrar_ns; i++) {
        Vehiculo* v = interseccion.vehiculos_norte_sur[i];
        if (v) {
            printf("  ID:%2d Pos=%6.1fm Vel=%5.2fm/s %s T%d\n", 
                   v->id, v->posicion, v->velocidad, estado_str(v->estado), v->thread_id);
        }
    }
    
    printf("ESTE-OESTE:\n");
    int mostrar_eo = fmin(interseccion.num_vehiculos_eo, 4);
    for (int i = 0; i < mostrar_eo; i++) {
        Vehiculo* v = interseccion.vehiculos_este_oeste[i];
        if (v) {
            printf("  ID:%2d Pos=%6.1fm Vel=%5.2fm/s %s T%d\n", 
                   v->id, v->posicion, v->velocidad, estado_str(v->estado), v->thread_id);
        }
    }
    omp_unset_lock(&interseccion.lock_sistema);
    printf("\n");
}

// ============================================================================
// ARCHIVOS CSV THREAD-SAFE
// ============================================================================

FILE *csv_estados = NULL;
FILE *csv_interseccion = NULL;
omp_lock_t lock_csv;

void inicializar_csv_estados() {
    omp_init_lock(&lock_csv);
    
    // Obtener directorio del código fuente usando __FILE__
    char dir_path[1024];
    const char* file_path = __FILE__;
    
    // Copiar la ruta y buscar el último separador
    strncpy(dir_path, file_path, sizeof(dir_path) - 1);
    dir_path[sizeof(dir_path) - 1] = '\0';
    
    // Buscar el último / o \ para extraer solo el directorio
    char *last_sep = strrchr(dir_path, '\\');
    if (!last_sep) last_sep = strrchr(dir_path, '/');
    
    if (last_sep) {
        *last_sep = '\0';  // Truncar en el último separador
    } else {
        strcpy(dir_path, ".");  // Si no hay separador, usar directorio actual
    }
    
    char nombre_estados[512], nombre_interseccion[512];
    time_t ahora = time(NULL);
    struct tm *t = localtime(&ahora);
    
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", t);
    
    // Construir rutas completas
    snprintf(nombre_estados, sizeof(nombre_estados), 
             "%s/estados_interseccion_%s.csv", dir_path, timestamp);
    snprintf(nombre_interseccion, sizeof(nombre_interseccion), 
             "%s/interseccion_%s.csv", dir_path, timestamp);
    
    printf("Directorio de código fuente: %s\n", dir_path);
    
    csv_estados = fopen(nombre_estados, "w");
    csv_interseccion = fopen(nombre_interseccion, "w");
    
    if (csv_estados) {
        fprintf(csv_estados, "Tiempo,ID,Direccion,Posicion,Velocidad,Estado,Semaforo,Thread\n");
        fflush(csv_estados);
        printf("✓ CSV estados creado: %s\n", nombre_estados);
    } else {
        fprintf(stderr, "ERROR: No se pudo crear %s\n", nombre_estados);
        perror("Razón");
    }
    
    if (csv_interseccion) {
        fprintf(csv_interseccion, "Tiempo,Estado,VehiculosNS,VehiculosEO,EnCruce,DireccionCruzando\n");
        fflush(csv_interseccion);
        printf("✓ CSV intersección creado: %s\n", nombre_interseccion);
    } else {
        fprintf(stderr, "ERROR: No se pudo crear %s\n", nombre_interseccion);
        perror("Razón");
    }
}

void registrar_estado_vehiculo_thread_safe(Vehiculo* v) {
    if (!csv_estados || !v) return;
    
    omp_set_lock(&lock_csv);
    omp_set_lock(&semaforo.lock);
    EstadoInterseccion estado_sem = semaforo.estado;
    omp_unset_lock(&semaforo.lock);
    
    fprintf(csv_estados, "%.2f,%d,%s,%.2f,%.2f,%s,%s,%d\n",
            interseccion.tiempo_actual, v->id,
            (v->direccion == NORTE_A_SUR) ? "NS" : "EO",
            v->posicion, v->velocidad, estado_str(v->estado),
            estado_interseccion_str(estado_sem), v->thread_id);
    fflush(csv_estados);
    
    omp_unset_lock(&lock_csv);
}

void registrar_estado_interseccion() {
    if (!csv_interseccion) return;
    
    omp_set_lock(&lock_csv);
    omp_set_lock(&semaforo.lock);
    EstadoInterseccion estado_sem = semaforo.estado;
    omp_unset_lock(&semaforo.lock);
    
    omp_set_lock(&interseccion.lock_interseccion);
    fprintf(csv_interseccion, "%.2f,%s,%d,%d,%d,%s\n",
            interseccion.tiempo_actual,
            estado_interseccion_str(estado_sem),
            interseccion.num_vehiculos_ns,
            interseccion.num_vehiculos_eo,
            interseccion.vehiculos_en_interseccion,
            (interseccion.vehiculos_en_interseccion > 0) ? 
                ((interseccion.direccion_actual_cruzando == NORTE_A_SUR) ? "NS" : "EO") : "NINGUNA");
    fflush(csv_interseccion);
    omp_unset_lock(&interseccion.lock_interseccion);
    
    omp_unset_lock(&lock_csv);
}

void cerrar_csv_estados() {
    omp_destroy_lock(&lock_csv);
    if (csv_estados) {
        fclose(csv_estados);
        csv_estados = NULL;
    }
    if (csv_interseccion) {
        fclose(csv_interseccion);
        csv_interseccion = NULL;
    }
    printf("Archivos CSV cerrados.\n");
}

// ============================================================================
// DECLARACIONES DE FUNCIONES
// ============================================================================

void generar_estadisticas_interseccion(Vehiculo** vehiculos_ns, Vehiculo** vehiculos_eo);
void procesar_eventos_interseccion_paralelo(void);
void configurar_interseccion(void);

// ============================================================================
// ACTUALIZACIÓN DE VEHÍCULOS CON PARALELISMO
// ============================================================================

void actualizar_vehiculo_interseccion(Vehiculo* v, double dt) {
    if (!v || v->estado == SALIENDO) return;
    
    v->actualizaciones_count++;
    v->thread_id = omp_get_thread_num();
    
    double velocidad_anterior = v->velocidad;
    EstadoVehiculo estado_anterior = v->estado;
    
    double longitud_calle = (v->direccion == NORTE_A_SUR) ? config.longitud_calle_ns : config.longitud_calle_eo;
    double pos_interseccion = config.posicion_interseccion;
    double inicio_interseccion = pos_interseccion - config.ancho_interseccion/2;
    double fin_interseccion = pos_interseccion + config.ancho_interseccion/2;
    
    // Lógica específica para intersección
    if (v->posicion < inicio_interseccion - 10.0) {
        // Comportamiento normal antes de la intersección
        if (v->velocidad < params.velocidad_maxima - 0.2) {
            v->estado = ACELERANDO;
            v->aceleracion = params.aceleracion_maxima;
        } else {
            v->estado = VELOCIDAD_CONSTANTE;
            v->aceleracion = 0.0;
        }
        
        // Verificar vehículo adelante
        Vehiculo* adelante = encontrar_vehiculo_adelante_interseccion(v->posicion, v->direccion, v);
        if (adelante) {
            double distancia = adelante->posicion - v->posicion - params.longitud_vehiculo;
            if (distancia < params.distancia_seguridad_min * 1.5) {
                v->estado = DESACELERANDO;
                v->aceleracion = params.desaceleracion_suave;
            }
        }
        
    } else if (v->posicion >= inicio_interseccion - 10.0 && v->posicion < inicio_interseccion) {
        // Aproximándose a la intersección
        if (puede_cruzar_interseccion(v)) {
            v->estado = ACELERANDO;
            v->aceleracion = params.aceleracion_maxima * 0.8;
        } else {
            v->estado = ESPERANDO_PASO;
            v->aceleracion = params.desaceleracion_maxima;
            v->tiempo_esperando_interseccion += dt;
            
            if (v->velocidad <= 0.1) {
                v->velocidad = 0.0;
                v->estado = DETENIDO;
                v->aceleracion = 0.0;
                v->tiempo_total_detenido += dt;
            }
        }
        
    } else if (v->posicion >= inicio_interseccion && v->posicion <= fin_interseccion) {
        // En la intersección
        if (v->estado != CRUZANDO_INTERSECCION) {
            entrar_interseccion(v);
        }
        v->aceleracion = params.aceleracion_maxima * 0.6; // Velocidad controlada en intersección
        
    } else if (v->posicion > fin_interseccion) {
        // Después de la intersección
        if (v->estado == CRUZANDO_INTERSECCION) {
            salir_interseccion(v);
        }
        
        if (v->velocidad < params.velocidad_maxima - 0.2) {
            v->estado = ACELERANDO;
            v->aceleracion = params.aceleracion_maxima;
        } else {
            v->estado = VELOCIDAD_CONSTANTE;
            v->aceleracion = 0.0;
        }
    }
    
    // Actualizar física
    double nueva_velocidad = v->velocidad + v->aceleracion * dt;
    if (nueva_velocidad < 0.0) nueva_velocidad = 0.0;
    if (nueva_velocidad > params.velocidad_maxima) nueva_velocidad = params.velocidad_maxima;
    
    if (nueva_velocidad < 0.1 && v->aceleracion < 0) {
        nueva_velocidad = 0.0;
    }
    
    double nueva_posicion = v->posicion + nueva_velocidad * dt;
    
    // Verificar colisiones
    Vehiculo* adelante = encontrar_vehiculo_adelante_interseccion(v->posicion, v->direccion, v);
    int puede_avanzar = 1;
    
    if (adelante) {
        double distancia_resultante = adelante->posicion - nueva_posicion - params.longitud_vehiculo;
        if (distancia_resultante < params.distancia_seguridad_min * 0.8) {
            puede_avanzar = 0;
        }
    }
    
    // Actualizar posición y velocidad
    if (puede_avanzar) {
        v->velocidad = nueva_velocidad;
        v->posicion = nueva_posicion;
        v->distancia_recorrida += nueva_velocidad * dt;
    } else {
        v->velocidad = 0.0;
        v->estado = DETENIDO;
        v->aceleracion = 0.0;
        v->tiempo_total_detenido += dt;
    }
    
    // Registrar cambios de estado
    if (estado_anterior != v->estado) {
        v->tiempo_en_estado = 0.0;
        v->ultimo_cambio_estado = interseccion.tiempo_actual;
    } else {
        v->tiempo_en_estado += dt;
    }
    
    // Estadísticas
    if (v->estado == DETENIDO) v->tiempo_total_detenido += dt;
    if (v->velocidad < params.velocidad_maxima / 2.0) v->tiempo_lento += dt;
    if (v->velocidad > 0 && velocidad_anterior == 0) {
        v->eventos_reanudacion++;
    }
    
    // Calcular velocidad promedio
    if (v->actualizaciones_count > 0) {
        double tiempo_transcurrido = interseccion.tiempo_actual - v->tiempo_entrada;
        if (tiempo_transcurrido > 0) {
            v->velocidad_promedio = v->distancia_recorrida / tiempo_transcurrido;
        }
    }
}

// ============================================================================
// FUNCIÓN PRINCIPAL DE SIMULACIÓN PARALELA
// ============================================================================

void procesar_eventos_interseccion_paralelo() {
    ColaEventos cola;
    inicializar_cola_eventos(&cola);
    
    // Arrays para todos los vehículos (para estadísticas finales)
    Vehiculo** todos_vehiculos_ns = (Vehiculo**)calloc(config.max_autos_por_calle, sizeof(Vehiculo*));
    Vehiculo** todos_vehiculos_eo = (Vehiculo**)calloc(config.max_autos_por_calle, sizeof(Vehiculo*));
    
    if (!todos_vehiculos_ns || !todos_vehiculos_eo) {
        fprintf(stderr, "ERROR: No se pudo asignar memoria para arrays de vehículos\n");
        return;
    }
    
    // Eventos iniciales
    int id_auto_ns = 1, id_auto_eo = 1001; // IDs diferentes para cada calle
    
    Evento entrada_ns = {0.0, ENTRADA_NORTE, id_auto_ns, NORTE_A_SUR, NULL, 1};
    Evento entrada_eo = {0.5, ENTRADA_ESTE, id_auto_eo, ESTE_A_OESTE, NULL, 1};
    
    insertar_evento_thread_safe(&cola, entrada_ns);
    insertar_evento_thread_safe(&cola, entrada_eo);
    
    double ultimo_reporte = 0.0;
    int max_vehiculos_total = config.max_autos_por_calle * 2;
    
    printf("Iniciando simulación paralela de intersección...\n");
    printf("Threads configurados: %d\n", omp_get_max_threads());
    
    // BUCLE PRINCIPAL PARALELO
    while ((cola.size > 0 || interseccion.num_vehiculos_ns > 0 || interseccion.num_vehiculos_eo > 0) &&
           (interseccion.total_vehiculos_completados_ns + interseccion.total_vehiculos_completados_eo) < max_vehiculos_total) {
        
        Evento* e = obtener_siguiente_evento_thread_safe(&cola);
        
        if (!e) {
            // Verificar si hay vehículos activos
            if (interseccion.num_vehiculos_ns > 0 || interseccion.num_vehiculos_eo > 0) {
                printf("ADVERTENCIA: Sin eventos pero vehículos activos en t=%.2f\n", interseccion.tiempo_actual);
            }
            break;
        }
        
        omp_set_lock(&interseccion.lock_sistema);
        interseccion.tiempo_actual = e->tiempo;
        omp_unset_lock(&interseccion.lock_sistema);
        
        actualizar_semaforo_interseccion(interseccion.tiempo_actual);
        
        omp_set_lock(&lock_eventos);
        eventos_procesados++;
        omp_unset_lock(&lock_eventos);
        
        // Protección contra bucles infinitos
        if (eventos_procesados > max_vehiculos_total * 10000) {
            printf("ADVERTENCIA: Demasiados eventos procesados. Verificando progreso...\n");
            break;
        }
        
        // PROCESAR ENTRADA NORTE-SUR
        if (e->tipo == ENTRADA_NORTE && interseccion.total_vehiculos_creados_ns < config.max_autos_por_calle) {
            // Verificar espacio
            int puede_entrar = 1;
            omp_set_lock(&interseccion.lock_sistema);
            for (int i = 0; i < interseccion.num_vehiculos_ns; i++) {
                Vehiculo* v = interseccion.vehiculos_norte_sur[i];
                if (v && v->posicion < params.longitud_vehiculo + params.distancia_seguridad_min) {
                    puede_entrar = 0;
                    break;
                }
            }
            omp_unset_lock(&interseccion.lock_sistema);
            
            if (puede_entrar) {
                Vehiculo* v = (Vehiculo*)calloc(1, sizeof(Vehiculo));
                if (v) {
                    v->id = id_auto_ns;
                    v->direccion = NORTE_A_SUR;
                    v->estado = ENTRANDO;
                    v->aceleracion = params.aceleracion_maxima;
                    v->tiempo_entrada = e->tiempo;
                    v->ultimo_cambio_estado = e->tiempo;
                    v->thread_id = omp_get_thread_num();
                    
                    omp_set_lock(&interseccion.lock_sistema);
                    todos_vehiculos_ns[interseccion.total_vehiculos_creados_ns] = v;
                    interseccion.vehiculos_norte_sur[interseccion.num_vehiculos_ns++] = v;
                    interseccion.total_vehiculos_creados_ns++;
                    omp_unset_lock(&interseccion.lock_sistema);
                    
                    // Programar actualización
                    Evento act = {e->tiempo + config.paso_simulacion, ACTUALIZACION_VEHICULO, v->id, NORTE_A_SUR, v, 0};
                    insertar_evento_thread_safe(&cola, act);
                    
                    id_auto_ns++;
                    
                    // Programar siguiente entrada
                    if (interseccion.total_vehiculos_creados_ns < config.max_autos_por_calle) {
                        Evento sig = {e->tiempo + config.intervalo_entrada_vehiculos, ENTRADA_NORTE, id_auto_ns, NORTE_A_SUR, NULL, 1};
                        insertar_evento_thread_safe(&cola, sig);
                    }
                }
            } else {
                // Reintentar entrada
                if (interseccion.total_vehiculos_creados_ns < config.max_autos_por_calle) {
                    Evento reintento = {e->tiempo + 1.0, ENTRADA_NORTE, id_auto_ns, NORTE_A_SUR, NULL, 1};
                    insertar_evento_thread_safe(&cola, reintento);
                }
            }
        }
        
        // PROCESAR ENTRADA ESTE-OESTE
        else if (e->tipo == ENTRADA_ESTE && interseccion.total_vehiculos_creados_eo < config.max_autos_por_calle) {
            // Verificar espacio
            int puede_entrar = 1;
            omp_set_lock(&interseccion.lock_sistema);
            for (int i = 0; i < interseccion.num_vehiculos_eo; i++) {
                Vehiculo* v = interseccion.vehiculos_este_oeste[i];
                if (v && v->posicion < params.longitud_vehiculo + params.distancia_seguridad_min) {
                    puede_entrar = 0;
                    break;
                }
            }
            omp_unset_lock(&interseccion.lock_sistema);
            
            if (puede_entrar) {
                Vehiculo* v = (Vehiculo*)calloc(1, sizeof(Vehiculo));
                if (v) {
                    v->id = id_auto_eo;
                    v->direccion = ESTE_A_OESTE;
                    v->estado = ENTRANDO;
                    v->aceleracion = params.aceleracion_maxima;
                    v->tiempo_entrada = e->tiempo;
                    v->ultimo_cambio_estado = e->tiempo;
                    v->thread_id = omp_get_thread_num();
                    
                    omp_set_lock(&interseccion.lock_sistema);
                    todos_vehiculos_eo[interseccion.total_vehiculos_creados_eo] = v;
                    interseccion.vehiculos_este_oeste[interseccion.num_vehiculos_eo++] = v;
                    interseccion.total_vehiculos_creados_eo++;
                    omp_unset_lock(&interseccion.lock_sistema);
                    
                    // Programar actualización
                    Evento act = {e->tiempo + config.paso_simulacion, ACTUALIZACION_VEHICULO, v->id, ESTE_A_OESTE, v, 0};
                    insertar_evento_thread_safe(&cola, act);
                    
                    id_auto_eo++;
                    
                    // Programar siguiente entrada
                    if (interseccion.total_vehiculos_creados_eo < config.max_autos_por_calle) {
                        Evento sig = {e->tiempo + config.intervalo_entrada_vehiculos, ENTRADA_ESTE, id_auto_eo, ESTE_A_OESTE, NULL, 1};
                        insertar_evento_thread_safe(&cola, sig);
                    }
                }
            } else {
                // Reintentar entrada
                if (interseccion.total_vehiculos_creados_eo < config.max_autos_por_calle) {
                    Evento reintento = {e->tiempo + 1.0, ENTRADA_ESTE, id_auto_eo, ESTE_A_OESTE, NULL, 1};
                    insertar_evento_thread_safe(&cola, reintento);
                }
            }
        }
        
        // PROCESAR ACTUALIZACIÓN DE VEHÍCULO
        else if (e->tipo == ACTUALIZACION_VEHICULO) {
            Vehiculo* v = e->vehiculo;
            
            if (v && v->estado != SALIENDO) {
                double dt = config.paso_simulacion;
                
                // Actualizar vehículo usando paralelismo
                #pragma omp task firstprivate(v, dt)
                {
                    actualizar_vehiculo_interseccion(v, dt);
                    registrar_estado_vehiculo_thread_safe(v);
                }
                #pragma omp taskwait
                
                // Verificar salida del sistema
                double longitud_calle = (v->direccion == NORTE_A_SUR) ? config.longitud_calle_ns : config.longitud_calle_eo;
                
                if (v->posicion >= longitud_calle) {
                    v->tiempo_salida = interseccion.tiempo_actual;
                    v->estado = SALIENDO;
                    
                    double tiempo_total = v->tiempo_salida - v->tiempo_entrada;
                    printf("[%.2f] Vehículo %d (%s) completa recorrido en %.2fs\n",
                           interseccion.tiempo_actual, v->id,
                           (v->direccion == NORTE_A_SUR) ? "NS" : "EO", tiempo_total);
                    
                    // Remover de vehículos activos
                    omp_set_lock(&interseccion.lock_sistema);
                    if (v->direccion == NORTE_A_SUR) {
                        for (int i = 0; i < interseccion.num_vehiculos_ns; i++) {
                            if (interseccion.vehiculos_norte_sur[i] == v) {
                                interseccion.vehiculos_norte_sur[i] = interseccion.vehiculos_norte_sur[interseccion.num_vehiculos_ns - 1];
                                interseccion.vehiculos_norte_sur[interseccion.num_vehiculos_ns - 1] = NULL;
                                interseccion.num_vehiculos_ns--;
                                interseccion.total_vehiculos_completados_ns++;
                                break;
                            }
                        }
                    } else {
                        for (int i = 0; i < interseccion.num_vehiculos_eo; i++) {
                            if (interseccion.vehiculos_este_oeste[i] == v) {
                                interseccion.vehiculos_este_oeste[i] = interseccion.vehiculos_este_oeste[interseccion.num_vehiculos_eo - 1];
                                interseccion.vehiculos_este_oeste[interseccion.num_vehiculos_eo - 1] = NULL;
                                interseccion.num_vehiculos_eo--;
                                interseccion.total_vehiculos_completados_eo++;
                                break;
                            }
                        }
                    }
                    omp_unset_lock(&interseccion.lock_sistema);
                } else {
                    // Programar siguiente actualización
                    Evento siguiente = {e->tiempo + config.paso_simulacion, ACTUALIZACION_VEHICULO, v->id, v->direccion, v, 0};
                    insertar_evento_thread_safe(&cola, siguiente);
                }
            }
        }
        
        // REPORTES PERIÓDICOS
        if (interseccion.tiempo_actual - ultimo_reporte >= 20.0) {
            imprimir_estado_interseccion();
            registrar_estado_interseccion();
            ultimo_reporte = interseccion.tiempo_actual;
            
            int completados_total = interseccion.total_vehiculos_completados_ns + interseccion.total_vehiculos_completados_eo;
            double porcentaje = (double)completados_total / max_vehiculos_total * 100.0;
            printf(">>> PROGRESO: %.1f%% completado (%d/%d vehículos) - Tiempo: %.1fs <<<\n\n", 
                   porcentaje, completados_total, max_vehiculos_total, interseccion.tiempo_actual);
        }
        
        free(e);
    }
    
    // REPORTES FINALES
    printf("\n=== SIMULACIÓN DE INTERSECCIÓN COMPLETADA ===\n");
    printf("Eventos procesados: %d\n", eventos_procesados);
    printf("Tiempo total: %.2f segundos\n", interseccion.tiempo_actual);
    printf("Vehículos Norte-Sur: %d creados, %d completados\n", 
           interseccion.total_vehiculos_creados_ns, interseccion.total_vehiculos_completados_ns);
    printf("Vehículos Este-Oeste: %d creados, %d completados\n", 
           interseccion.total_vehiculos_creados_eo, interseccion.total_vehiculos_completados_eo);
    printf("Ciclos de semáforo: %d\n", semaforo.ciclos_completados);
    
    // Generar estadísticas finales
    generar_estadisticas_interseccion(todos_vehiculos_ns, todos_vehiculos_eo);
    
    // Limpieza
    destruir_cola_eventos(&cola);
    
    for (int i = 0; i < interseccion.total_vehiculos_creados_ns; i++) {
        if (todos_vehiculos_ns[i]) free(todos_vehiculos_ns[i]);
    }
    for (int i = 0; i < interseccion.total_vehiculos_creados_eo; i++) {
        if (todos_vehiculos_eo[i]) free(todos_vehiculos_eo[i]);
    }
    
    free(todos_vehiculos_ns);
    free(todos_vehiculos_eo);
}

// ============================================================================
// ESTADÍSTICAS FINALES PARA INTERSECCIÓN
// ============================================================================

void generar_estadisticas_interseccion(Vehiculo** vehiculos_ns, Vehiculo** vehiculos_eo) {
    printf("\n==== ESTADÍSTICAS FINALES DE INTERSECCIÓN ====\n");
    
    // Obtener directorio del código fuente usando __FILE__
    char dir_path[1024];
    const char* file_path = __FILE__;
    
    strncpy(dir_path, file_path, sizeof(dir_path) - 1);
    dir_path[sizeof(dir_path) - 1] = '\0';
    
    char *last_sep = strrchr(dir_path, '\\');
    if (!last_sep) last_sep = strrchr(dir_path, '/');
    
    if (last_sep) {
        *last_sep = '\0';
    } else {
        strcpy(dir_path, ".");
    }
    
    char nombre_archivo[512];
    time_t ahora = time(NULL);
    struct tm *t = localtime(&ahora);
    
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", t);
    
    // Construir ruta completa
    snprintf(nombre_archivo, sizeof(nombre_archivo), 
             "%s/resultados_interseccion_%s.csv", dir_path, timestamp);
    
    FILE *csv = fopen(nombre_archivo, "w");
    if (!csv) {
        printf("ERROR: No se pudo crear archivo CSV: %s\n", nombre_archivo);
        perror("Razón");
        return;
    }
    
    printf("Creando archivo de resultados: %s\n", nombre_archivo);
    
    // ... resto del código igual
    
    // Encabezado
    fprintf(csv, "# ESTADISTICAS_INTERSECCION\n");
    fprintf(csv, "TiempoTotal,%.2f\n", interseccion.tiempo_actual);
    fprintf(csv, "VehiculosNS,%d,%d\n", interseccion.total_vehiculos_creados_ns, interseccion.total_vehiculos_completados_ns);
    fprintf(csv, "VehiculosEO,%d,%d\n", interseccion.total_vehiculos_creados_eo, interseccion.total_vehiculos_completados_eo);
    fprintf(csv, "CiclosSemaforo,%d\n", semaforo.ciclos_completados);
    fprintf(csv, "# DATOS_VEHICULOS\n");
    fprintf(csv, "ID,Direccion,Entrada,Interseccion,Salida,TiempoEspera,TiempoCruce,VelProm\n");
    
    // Procesar vehículos Norte-Sur
    if (interseccion.total_vehiculos_completados_ns > 0) {
        double tiempo_promedio_ns = 0.0, velocidad_promedio_ns = 0.0;
        double tiempo_espera_promedio_ns = 0.0, tiempo_cruce_promedio_ns = 0.0;
        
        for (int i = 0; i < interseccion.total_vehiculos_creados_ns; i++) {
            if (vehiculos_ns[i] && vehiculos_ns[i]->estado == SALIENDO) {
                Vehiculo* v = vehiculos_ns[i];
                double tiempo_recorrido = v->tiempo_salida - v->tiempo_entrada;
                tiempo_promedio_ns += tiempo_recorrido;
                
                if (tiempo_recorrido > 0) {
                    velocidad_promedio_ns += config.longitud_calle_ns / tiempo_recorrido;
                }
                
                tiempo_espera_promedio_ns += v->tiempo_esperando_interseccion;
                tiempo_cruce_promedio_ns += v->tiempo_cruzando;
                
                // Guardar en CSV
                fprintf(csv, "%d,NS,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f\n",
                        v->id, v->tiempo_entrada, v->tiempo_llegada_interseccion,
                        v->tiempo_salida, v->tiempo_esperando_interseccion,
                        v->tiempo_cruzando, v->velocidad_promedio);
            }
        }
        
        tiempo_promedio_ns /= interseccion.total_vehiculos_completados_ns;
        velocidad_promedio_ns /= interseccion.total_vehiculos_completados_ns;
        tiempo_espera_promedio_ns /= interseccion.total_vehiculos_completados_ns;
        tiempo_cruce_promedio_ns /= interseccion.total_vehiculos_completados_ns;
        
        printf("NORTE-SUR:\n");
        printf("  Tiempo promedio: %.2f s\n", tiempo_promedio_ns);
        printf("  Velocidad promedio: %.2f m/s (%.1f km/h)\n", velocidad_promedio_ns, velocidad_promedio_ns * 3.6);
        printf("  Tiempo espera en intersección: %.2f s\n", tiempo_espera_promedio_ns);
        printf("  Tiempo cruzando: %.2f s\n", tiempo_cruce_promedio_ns);
    }
    
    // Procesar vehículos Este-Oeste
    if (interseccion.total_vehiculos_completados_eo > 0) {
        double tiempo_promedio_eo = 0.0, velocidad_promedio_eo = 0.0;
        double tiempo_espera_promedio_eo = 0.0, tiempo_cruce_promedio_eo = 0.0;
        
        for (int i = 0; i < interseccion.total_vehiculos_creados_eo; i++) {
            if (vehiculos_eo[i] && vehiculos_eo[i]->estado == SALIENDO) {
                Vehiculo* v = vehiculos_eo[i];
                double tiempo_recorrido = v->tiempo_salida - v->tiempo_entrada;
                tiempo_promedio_eo += tiempo_recorrido;
                
                if (tiempo_recorrido > 0) {
                    velocidad_promedio_eo += config.longitud_calle_eo / tiempo_recorrido;
                }
                
                tiempo_espera_promedio_eo += v->tiempo_esperando_interseccion;
                tiempo_cruce_promedio_eo += v->tiempo_cruzando;
                
                // Guardar en CSV
                fprintf(csv, "%d,EO,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f\n",
                        v->id, v->tiempo_entrada, v->tiempo_llegada_interseccion,
                        v->tiempo_salida, v->tiempo_esperando_interseccion,
                        v->tiempo_cruzando, v->velocidad_promedio);
            }
        }
        
        tiempo_promedio_eo /= interseccion.total_vehiculos_completados_eo;
        velocidad_promedio_eo /= interseccion.total_vehiculos_completados_eo;
        tiempo_espera_promedio_eo /= interseccion.total_vehiculos_completados_eo;
        tiempo_cruce_promedio_eo /= interseccion.total_vehiculos_completados_eo;
        
        printf("ESTE-OESTE:\n");
        printf("  Tiempo promedio: %.2f s\n", tiempo_promedio_eo);
        printf("  Velocidad promedio: %.2f m/s (%.1f km/h)\n", velocidad_promedio_eo, velocidad_promedio_eo * 3.6);
        printf("  Tiempo espera en intersección: %.2f s\n", tiempo_espera_promedio_eo);
        printf("  Tiempo cruzando: %.2f s\n", tiempo_cruce_promedio_eo);
    }
    
    fclose(csv);
    printf("\nResultados guardados en: %s\n", nombre_archivo);
}

// ============================================================================
// FUNCIONES DE VALIDACIÓN Y CONFIGURACIÓN SEGURA
// ============================================================================

void limpiar_buffer() {
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}

int leer_entero_validado_interseccion(const char* mensaje, int valor_actual, int min, int max) {
    int valor, resultado;
    char buffer[100];
    
    do {
        printf("%s (actual: %d, rango: %d-%d): ", mensaje, valor_actual, min, max);
        
        if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
            printf("Error de lectura. Usando valor actual.\n");
            return valor_actual;
        }
        
        if (buffer[0] == '\n') {
            return valor_actual;
        }
        
        resultado = sscanf(buffer, "%d", &valor);
        
        if (resultado != 1) {
            printf("ERROR: Debe ingresar un número entero válido.\n");
        } else if (valor < min || valor > max) {
            printf("ERROR: El valor debe estar entre %d y %d.\n", min, max);
        } else {
            return valor;
        }
    } while (1);
}

double leer_double_validado_interseccion(const char* mensaje, double valor_actual, double min, double max) {
    double valor;
    int resultado;
    char buffer[100];
    
    do {
        printf("%s (actual: %.2f, rango: %.2f-%.2f): ", mensaje, valor_actual, min, max);
        
        if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
            printf("Error de lectura. Usando valor actual.\n");
            return valor_actual;
        }
        
        if (buffer[0] == '\n') {
            return valor_actual;
        }
        
        resultado = sscanf(buffer, "%lf", &valor);
        
        if (resultado != 1) {
            printf("ERROR: Debe ingresar un número decimal válido.\n");
        } else if (valor < min || valor > max) {
            printf("ERROR: El valor debe estar entre %.2f y %.2f.\n", min, max);
        } else {
            return valor;
        }
    } while (1);
}

char leer_si_no_interseccion(const char* mensaje) {
    char respuesta;
    char buffer[100];
    
    do {
        printf("%s (s/n): ", mensaje);
        
        if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
            printf("Error de lectura. Usando 'n' por defecto.\n");
            return 'n';
        }
        
        respuesta = buffer[0];
        
        if (respuesta == 's' || respuesta == 'S' || 
            respuesta == 'n' || respuesta == 'N') {
            return respuesta;
        } else {
            printf("ERROR: Debe ingresar 's' para sí o 'n' para no.\n");
        }
    } while (1);
}

int validar_configuracion_interseccion() {
    int errores = 0;
    
    printf("\n=== VALIDACIÓN DE CONFIGURACIÓN ===\n");
    
    // Validar vehículos por calle
    if (config.max_autos_por_calle <= 0 || config.max_autos_por_calle > 1000) {
        fprintf(stderr, "ERROR: max_autos_por_calle debe estar entre 1 y 1000 (actual: %d)\n", 
                config.max_autos_por_calle);
        errores++;
    }
    
    // Validar longitudes de calles
    if (config.longitud_calle_ns < 50.0 || config.longitud_calle_ns > 2000.0) {
        fprintf(stderr, "ERROR: longitud_calle_ns debe estar entre 50 y 2000m (actual: %.1f)\n", 
                config.longitud_calle_ns);
        errores++;
    }
    
    if (config.longitud_calle_eo < 50.0 || config.longitud_calle_eo > 2000.0) {
        fprintf(stderr, "ERROR: longitud_calle_eo debe estar entre 50 y 2000m (actual: %.1f)\n", 
                config.longitud_calle_eo);
        errores++;
    }
    
    // Validar posición de intersección
    double margen_min = fmin(config.longitud_calle_ns, config.longitud_calle_eo) * 0.2;
    double min_interseccion_ns = margen_min;
    double max_interseccion_ns = config.longitud_calle_ns - margen_min;
    double min_interseccion_eo = margen_min;
    double max_interseccion_eo = config.longitud_calle_eo - margen_min;
    
    if (config.posicion_interseccion < min_interseccion_ns || 
        config.posicion_interseccion > max_interseccion_ns) {
        fprintf(stderr, "ERROR: posicion_interseccion para calle NS debe estar entre %.1f y %.1f (actual: %.1f)\n", 
                min_interseccion_ns, max_interseccion_ns, config.posicion_interseccion);
        errores++;
    }
    
    if (config.posicion_interseccion < min_interseccion_eo || 
        config.posicion_interseccion > max_interseccion_eo) {
        fprintf(stderr, "ERROR: posicion_interseccion para calle EO debe estar entre %.1f y %.1f (actual: %.1f)\n", 
                min_interseccion_eo, max_interseccion_eo, config.posicion_interseccion);
        errores++;
    }
    
    // Validar ancho de intersección
    if (config.ancho_interseccion < 4.0 || config.ancho_interseccion > 20.0) {
        fprintf(stderr, "ERROR: ancho_interseccion debe estar entre 4 y 20m (actual: %.1f)\n", 
                config.ancho_interseccion);
        errores++;
    }
    
    // Validar parámetros de simulación
    if (config.paso_simulacion <= 0.001 || config.paso_simulacion > 0.5) {
        fprintf(stderr, "ERROR: paso_simulacion debe estar entre 0.001 y 0.5 (actual: %.3f)\n", 
                config.paso_simulacion);
        errores++;
    }
    
    if (config.intervalo_entrada_vehiculos <= 0.1 || config.intervalo_entrada_vehiculos > 10.0) {
        fprintf(stderr, "ERROR: intervalo_entrada_vehiculos debe estar entre 0.1 y 10.0s (actual: %.2f)\n", 
                config.intervalo_entrada_vehiculos);
        errores++;
    }
    
    // Validar parámetros físicos
    if (params.velocidad_maxima <= 0 || params.velocidad_maxima > 30.0) {
        fprintf(stderr, "ERROR: velocidad_maxima debe estar entre 1 y 30 m/s (actual: %.1f)\n", 
                params.velocidad_maxima);
        errores++;
    }
    
    // Validar duraciones de semáforo
    if (semaforo.duracion_ns_verde < 10.0 || semaforo.duracion_ns_verde > 120.0) {
        fprintf(stderr, "ERROR: duracion_ns_verde debe estar entre 10 y 120s (actual: %.1f)\n", 
                semaforo.duracion_ns_verde);
        errores++;
    }
    
    if (semaforo.duracion_eo_verde < 10.0 || semaforo.duracion_eo_verde > 120.0) {
        fprintf(stderr, "ERROR: duracion_eo_verde debe estar entre 10 y 120s (actual: %.1f)\n", 
                semaforo.duracion_eo_verde);
        errores++;
    }
    
    if (semaforo.duracion_transicion < 2.0 || semaforo.duracion_transicion > 10.0) {
        fprintf(stderr, "ERROR: duracion_transicion debe estar entre 2 y 10s (actual: %.1f)\n", 
                semaforo.duracion_transicion);
        errores++;
    }
    
    // Validaciones de coherencia
    if (fabs(config.longitud_calle_ns - config.longitud_calle_eo) > 500.0) {
        printf("ADVERTENCIA: Gran diferencia entre longitudes de calles (%.1fm vs %.1fm). Puede afectar el equilibrio.\n", 
               config.longitud_calle_ns, config.longitud_calle_eo);
    }
    
    double distancia_frenado = (params.velocidad_maxima * params.velocidad_maxima) / 
                              (2.0 * fabs(params.desaceleracion_maxima));
    double distancia_disponible_ns = config.posicion_interseccion - config.ancho_interseccion/2;
    double distancia_disponible_eo = config.posicion_interseccion - config.ancho_interseccion/2;
    
    if (distancia_frenado > distancia_disponible_ns * 0.8) {
        printf("ADVERTENCIA: Distancia de frenado (%.1fm) muy grande para aproximación NS (%.1fm disponibles)\n", 
               distancia_frenado, distancia_disponible_ns);
    }
    
    if (distancia_frenado > distancia_disponible_eo * 0.8) {
        printf("ADVERTENCIA: Distancia de frenado (%.1fm) muy grande para aproximación EO (%.1fm disponibles)\n", 
               distancia_frenado, distancia_disponible_eo);
    }
    
    // Estimación de memoria
    size_t memoria_estimada = config.max_autos_por_calle * 2 * sizeof(Vehiculo) + 
                             config.max_autos_por_calle * 2 * sizeof(Vehiculo*) + 
                             sizeof(SistemaInterseccion);
    double memoria_mb = memoria_estimada / (1024.0 * 1024.0);
    
    if (memoria_mb > 200.0) {
        printf("ADVERTENCIA: Memoria estimada: %.1f MB. Simulación puede ser lenta.\n", memoria_mb);
    } else {
        printf("Memoria estimada: %.2f MB\n", memoria_mb);
    }
    
    if (errores > 0) {
        fprintf(stderr, "\nSe encontraron %d errores en la configuración.\n", errores);
        return 0;
    } else {
        printf("✓ Configuración válida\n");
        return 1;
    }
}

void configurar_interseccion() {
    printf("=== CONFIGURACIÓN DE INTERSECCIÓN ===\n");
    printf("Instrucciones:\n");
    printf("- Presione Enter para mantener el valor actual\n");
    printf("- Los valores deben estar dentro de los rangos permitidos\n");
    printf("- Use punto decimal (.) para números decimales\n\n");
    
    char respuesta = leer_si_no_interseccion("¿Desea usar configuración personalizada?");
    
    if (respuesta == 's' || respuesta == 'S') {
        printf("\n--- CONFIGURACIÓN DE VEHÍCULOS ---\n");
        config.max_autos_por_calle = leer_entero_validado_interseccion(
            "Número máximo de autos por calle", 
            config.max_autos_por_calle, 1, 1000
        );
        
        config.intervalo_entrada_vehiculos = leer_double_validado_interseccion(
            "Intervalo entre entradas de vehículos (segundos)",
            config.intervalo_entrada_vehiculos, 0.1, 10.0
        );
        
        printf("\n--- CONFIGURACIÓN DE CALLES ---\n");
        config.longitud_calle_ns = leer_double_validado_interseccion(
            "Longitud de la calle Norte-Sur (metros)",
            config.longitud_calle_ns, 50.0, 2000.0
        );
        
        config.longitud_calle_eo = leer_double_validado_interseccion(
            "Longitud de la calle Este-Oeste (metros)",
            config.longitud_calle_eo, 50.0, 2000.0
        );
        
        // Calcular límites válidos para la intersección
        double margen_min = fmin(config.longitud_calle_ns, config.longitud_calle_eo) * 0.2;
        double min_interseccion = fmax(margen_min, fmax(config.longitud_calle_ns, config.longitud_calle_eo) * 0.2);
        double max_interseccion = fmin(config.longitud_calle_ns - margen_min, config.longitud_calle_eo - margen_min);
        
        if (config.posicion_interseccion < min_interseccion || config.posicion_interseccion > max_interseccion) {
            config.posicion_interseccion = (min_interseccion + max_interseccion) / 2.0;
            printf("Ajustando posición de intersección a %.1fm\n", config.posicion_interseccion);
        }
        
        config.posicion_interseccion = leer_double_validado_interseccion(
            "Posición de la intersección (metros desde el inicio)",
            config.posicion_interseccion, min_interseccion, max_interseccion
        );
        
        config.ancho_interseccion = leer_double_validado_interseccion(
            "Ancho de la zona de intersección (metros)",
            config.ancho_interseccion, 4.0, 20.0
        );
        
        printf("\n--- CONFIGURACIÓN DE VELOCIDAD ---\n");
        params.velocidad_maxima = leer_double_validado_interseccion(
            "Velocidad máxima (m/s)",
            params.velocidad_maxima, 1.0, 30.0
        );
        
        printf("    (%.1f m/s = %.1f km/h)\n", 
               params.velocidad_maxima, params.velocidad_maxima * 3.6);
        
        printf("\n--- CONFIGURACIÓN DEL SEMÁFORO ---\n");
        semaforo.duracion_ns_verde = leer_double_validado_interseccion(
            "Duración de luz verde Norte-Sur (segundos)",
            semaforo.duracion_ns_verde, 10.0, 120.0
        );
        
        semaforo.duracion_eo_verde = leer_double_validado_interseccion(
            "Duración de luz verde Este-Oeste (segundos)",
            semaforo.duracion_eo_verde, 10.0, 120.0
        );
        
        semaforo.duracion_transicion = leer_double_validado_interseccion(
            "Duración de transición/amarillo (segundos)",
            semaforo.duracion_transicion, 2.0, 10.0
        );
        
        printf("\n--- CONFIGURACIÓN AVANZADA ---\n");
        char config_avanzada = leer_si_no_interseccion("¿Configurar parámetros avanzados?");
        
        if (config_avanzada == 's' || config_avanzada == 'S') {
            config.paso_simulacion = leer_double_validado_interseccion(
                "Paso de simulación (segundos)",
                config.paso_simulacion, 0.01, 0.5
            );
            
            params.aceleracion_maxima = leer_double_validado_interseccion(
                "Aceleración máxima (m/s²)",
                params.aceleracion_maxima, 0.5, 8.0
            );
            
            params.desaceleracion_maxima = leer_double_validado_interseccion(
                "Desaceleración máxima (m/s²) - valor positivo",
                fabs(params.desaceleracion_maxima), 1.0, 12.0
            );
            params.desaceleracion_maxima = -fabs(params.desaceleracion_maxima);
            
            params.distancia_seguridad_min = leer_double_validado_interseccion(
                "Distancia mínima de seguridad (metros)",
                params.distancia_seguridad_min, 1.0, 15.0
            );
        }
        
        printf("\n--- VALIDANDO CONFIGURACIÓN ---\n");
        
        // Ajustar paso de simulación si es necesario
        double paso_max_recomendado = config.intervalo_entrada_vehiculos / 10.0;
        if (config.paso_simulacion > paso_max_recomendado) {
            printf("Ajustando paso de simulación a %.3f por coherencia\n", paso_max_recomendado);
            config.paso_simulacion = paso_max_recomendado;
        }
        
        printf("Configuración personalizada aplicada.\n");
    } else {
        printf("Usando configuración por defecto.\n");
    }
    
    // Mostrar configuración final
    printf("\n=== CONFIGURACIÓN FINAL ===\n");
    printf("Vehículos por calle: %d\n", config.max_autos_por_calle);
    printf("Longitud Norte-Sur: %.1f m\n", config.longitud_calle_ns);
    printf("Longitud Este-Oeste: %.1f m\n", config.longitud_calle_eo);
    printf("Posición intersección: %.1f m\n", config.posicion_interseccion);
    printf("Ancho intersección: %.1f m\n", config.ancho_interseccion);
    printf("Velocidad máxima: %.1f m/s (%.1f km/h)\n", 
           params.velocidad_maxima, params.velocidad_maxima * 3.6);
    printf("Semáforo NS: %.1fs, EO: %.1fs, Transición: %.1fs\n", 
           semaforo.duracion_ns_verde, semaforo.duracion_eo_verde, semaforo.duracion_transicion);
    printf("Intervalo entrada: %.1f s\n", config.intervalo_entrada_vehiculos);
    printf("Paso simulación: %.3f s\n", config.paso_simulacion);
    printf("Threads OpenMP: %d\n", omp_get_max_threads());
    printf("===============================\n\n");
}

// ============================================================================
// FUNCIÓN PRINCIPAL
// ============================================================================

int main() {
    srand((unsigned int)time(NULL));
    
    printf("=== SIMULACIÓN DE INTERSECCIÓN CON PARALELISMO OpenMP ===\n");
    printf("Características:\n");
    printf("- Intersección de dos calles (Norte-Sur y Este-Oeste)\n");
    printf("- Paralelismo con OpenMP\n");
    printf("- Control inteligente de semáforos\n");
    printf("- Detección de conflictos en intersección\n");
    printf("- Estadísticas detalladas por calle\n");
    printf("- Archivos CSV con datos completos\n\n");
    
    // Configurar OpenMP
    int num_threads = fmin(4, omp_get_max_threads());
    omp_set_num_threads(num_threads);
    
    printf("Configurando OpenMP con %d threads\n", num_threads);
    
    // Configuración y validación
    configurar_interseccion();
    
    if (!validar_configuracion_interseccion()) {
        fprintf(stderr, "Configuración inválida. Terminando.\n");
        return 1;
    }
    
    // Inicializar sistema
    inicializar_sistema();
    inicializar_csv_estados();
    
    // Ejecutar simulación de forma secuencial con paralelismo controlado
    printf("Iniciando simulación de intersección...\n\n");
    clock_t inicio = clock();
    
    // Ejecutar sin pragma omp parallel
    procesar_eventos_interseccion_paralelo();
    
    clock_t fin = clock();
    double tiempo_ejecucion = ((double)(fin - inicio)) / CLOCKS_PER_SEC;
    
    // Cerrar archivos
    cerrar_csv_estados();
    
    printf("\n=== MÉTRICAS DE RENDIMIENTO ===\n");
    printf("Tiempo de ejecución: %.3f segundos\n", tiempo_ejecucion);
    printf("Tiempo simulado: %.2f segundos\n", interseccion.tiempo_actual);
    printf("Factor de aceleración: %.1fx\n", interseccion.tiempo_actual / tiempo_ejecucion);
    printf("Eventos procesados: %d\n", eventos_procesados);
    printf("Throughput total: %.1f vehículos/segundo\n", 
           (double)(interseccion.total_vehiculos_completados_ns + interseccion.total_vehiculos_completados_eo) / interseccion.tiempo_actual);
    
    // Limpiar sistema
    limpiar_sistema();
    
    printf("\nSimulación de intersección completada exitosamente.\n");
    
    return 0;
}