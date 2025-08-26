#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <string.h>
#include <omp.h>

// ============================================================================
// DEFINICIÓN DE CONSTANTES PARA INTERSECCIÓN SIMPLE
// ============================================================================

#define ENTRADA 0
#define SALIDA 1
#define ACTUALIZACION_VEHICULO 2

// Solo 2 direcciones de tráfico
typedef enum {
    NORTE_SUR = 0,    // Calle vertical: Norte → Sur
    OESTE_ESTE = 1    // Calle horizontal: Oeste → Este
} DireccionCalle;

// Configuración de intersección simple
typedef struct {
    double longitud_calle_ns;      // Norte-Sur
    double longitud_calle_oe;      // Oeste-Este
    double ancho_interseccion;     // Zona de cruce
    double paso_simulacion;
    int max_autos_por_calle;
    double intervalo_entrada_vehiculos;
    double tiempo_limite_simulacion;
    
    // Posiciones de semáforos (antes de la intersección)
    double posicion_semaforo_ns;   // En calle Norte-Sur
    double posicion_semaforo_oe;   // En calle Oeste-Este
} ConfiguracionInterseccion;

// Configuración por defecto
ConfiguracionInterseccion config = {
    .longitud_calle_ns = 200.0,
    .longitud_calle_oe = 200.0,
    .ancho_interseccion = 20.0,
    .paso_simulacion = 0.05,
    .max_autos_por_calle = 30,
    .intervalo_entrada_vehiculos = 2.5,
    .tiempo_limite_simulacion = 0.0,
    .posicion_semaforo_ns = 80.0,   // 80m desde entrada norte
    .posicion_semaforo_oe = 80.0    // 80m desde entrada oeste
};

// ============================================================================
// ESTRUCTURAS PARA INTERSECCIÓN SIMPLE
// ============================================================================

typedef enum {
    ENTRANDO,
    ACELERANDO,
    VELOCIDAD_CONSTANTE,
    DESACELERANDO,
    FRENANDO,
    DETENIDO,
    EN_INTERSECCION,
    SALIENDO
} EstadoVehiculo;

typedef enum {
    NS_VERDE,    // Norte-Sur puede pasar
    OE_VERDE     // Oeste-Este puede pasar
} EstadoSemaforo;

// Vehículo simplificado
typedef struct {
    int id;
    DireccionCalle calle;        // En qué calle está (NS u OE)
    double posicion;             // Posición en su calle
    double velocidad;
    double aceleracion;
    EstadoVehiculo estado;
    
    // Coordenadas absolutas para detección en intersección
    double coord_x, coord_y;
    
    // Métricas básicas
    double tiempo_entrada;
    double tiempo_llegada_semaforo;
    double tiempo_salida;
    double tiempo_total_detenido;
    double distancia_recorrida;
    
    int actualizaciones_count;
} Vehiculo;

// Sistema para cada calle (solo 2)
typedef struct {
    Vehiculo** vehiculos;
    int num_vehiculos_activos;
    int capacidad_vehiculos;
    int total_vehiculos_creados;
    int total_vehiculos_completados;
    
    // Lock para acceso seguro
    omp_lock_t lock;
} SistemaCalle;

// Sistema de intersección completo
typedef struct {
    SistemaCalle calles[2];  // [0]=Norte-Sur, [1]=Oeste-Este
    double tiempo_actual;
    
    // Control de intersección
    Vehiculo** vehiculos_en_interseccion;
    int num_en_interseccion;
    int capacidad_interseccion;
    omp_lock_t lock_interseccion;
} SistemaInterseccion;

// Control de semáforo simple
typedef struct {
    EstadoSemaforo estado;
    double ultimo_cambio;
    double duracion_ns_verde;      // Tiempo verde para Norte-Sur
    double duracion_oe_verde;      // Tiempo verde para Oeste-Este
    double duracion_amarillo;      // Transición
    int ciclos_completados;
    omp_lock_t lock;
} ControlSemaforo;

// ============================================================================
// VARIABLES GLOBALES
// ============================================================================

SistemaInterseccion interseccion = {0};
ControlSemaforo semaforo = {
    .estado = NS_VERDE,
    .ultimo_cambio = 0.0,
    .duracion_ns_verde = 30.0,
    .duracion_oe_verde = 25.0,
    .duracion_amarillo = 4.0,
    .ciclos_completados = 0
};

typedef struct {
    double velocidad_maxima;
    double aceleracion_maxima;
    double desaceleracion_maxima;
    double distancia_seguridad_min;
    double longitud_vehiculo;
} ParametrosTrafico;

ParametrosTrafico params = {
    .velocidad_maxima = 12.0,
    .aceleracion_maxima = 2.5,
    .desaceleracion_maxima = -4.0,
    .distancia_seguridad_min = 4.0,
    .longitud_vehiculo = 5.0
};

// ============================================================================
// FUNCIONES DE INICIALIZACIÓN
// ============================================================================

void inicializar_sistema() {
    printf("Inicializando sistema de 2 calles con OpenMP...\n");
    
    // Inicializar locks
    omp_init_lock(&semaforo.lock);
    omp_init_lock(&interseccion.lock_interseccion);
    
    // Inicializar cada calle
    for (int c = 0; c < 2; c++) {
        SistemaCalle* calle = &interseccion.calles[c];
        omp_init_lock(&calle->lock);
        
        calle->capacidad_vehiculos = config.max_autos_por_calle + 5;
        calle->vehiculos = (Vehiculo**)calloc(calle->capacidad_vehiculos, sizeof(Vehiculo*));
        
        if (!calle->vehiculos) {
            fprintf(stderr, "ERROR: No se pudo asignar memoria para calle %d\n", c);
            exit(1);
        }
        
        calle->num_vehiculos_activos = 0;
        calle->total_vehiculos_creados = 0;
        calle->total_vehiculos_completados = 0;
    }
    
    // Inicializar zona de intersección
    interseccion.capacidad_interseccion = 10;
    interseccion.vehiculos_en_interseccion = (Vehiculo**)calloc(
        interseccion.capacidad_interseccion, sizeof(Vehiculo*));
    
    if (!interseccion.vehiculos_en_interseccion) {
        fprintf(stderr, "ERROR: No se pudo asignar memoria para intersección\n");
        exit(1);
    }
    
    printf("Sistema inicializado: 2 calles, %d threads\n", omp_get_max_threads());
}

void limpiar_sistema() {
    omp_destroy_lock(&semaforo.lock);
    omp_destroy_lock(&interseccion.lock_interseccion);
    
    for (int c = 0; c < 2; c++) {
        SistemaCalle* calle = &interseccion.calles[c];
        omp_destroy_lock(&calle->lock);
        if (calle->vehiculos) {
            free(calle->vehiculos);
        }
    }
    
    if (interseccion.vehiculos_en_interseccion) {
        free(interseccion.vehiculos_en_interseccion);
    }
    
    printf("Sistema limpiado\n");
}

// ============================================================================
// FUNCIONES DE COORDENADAS
// ============================================================================

void calcular_coordenadas_absolutas(Vehiculo* v) {
    if (v->calle == NORTE_SUR) {
        // Calle vertical: Norte → Sur
        v->coord_x = config.longitud_calle_oe / 2.0;  // En el centro horizontal
        v->coord_y = v->posicion;                     // Posición en Y
    } else {
        // Calle horizontal: Oeste → Este  
        v->coord_x = v->posicion;                     // Posición en X
        v->coord_y = config.longitud_calle_ns / 2.0; // En el centro vertical
    }
}

int esta_en_interseccion(Vehiculo* v) {
    double centro_x = config.longitud_calle_oe / 2.0;
    double centro_y = config.longitud_calle_ns / 2.0;
    double radio = config.ancho_interseccion / 2.0;
    
    return (fabs(v->coord_x - centro_x) <= radio && 
            fabs(v->coord_y - centro_y) <= radio);
}

double distancia_al_semaforo(Vehiculo* v) {
    if (v->calle == NORTE_SUR) {
        return config.posicion_semaforo_ns - v->posicion;
    } else {
        return config.posicion_semaforo_oe - v->posicion;
    }
}

// ============================================================================
// CONTROL DE SEMÁFOROS
// ============================================================================

void actualizar_semaforo(double tiempo) {
    omp_set_lock(&semaforo.lock);
    
    double ciclo_total = semaforo.duracion_ns_verde + semaforo.duracion_amarillo +
                        semaforo.duracion_oe_verde + semaforo.duracion_amarillo;
    double t_ciclo = fmod(tiempo, ciclo_total);
    
    EstadoSemaforo estado_anterior = semaforo.estado;
    
    if (t_ciclo < semaforo.duracion_ns_verde + semaforo.duracion_amarillo) {
        semaforo.estado = NS_VERDE;
    } else {
        semaforo.estado = OE_VERDE;
    }
    
    if (estado_anterior != semaforo.estado) {
        semaforo.ultimo_cambio = tiempo;
        if (semaforo.estado == NS_VERDE) {
            semaforo.ciclos_completados++;
        }
    }
    
    omp_unset_lock(&semaforo.lock);
}

int puede_pasar_semaforo(Vehiculo* v) {
    int puede_pasar = 0;
    
    omp_set_lock(&semaforo.lock);
    if (v->calle == NORTE_SUR) {
        puede_pasar = (semaforo.estado == NS_VERDE);
    } else {
        puede_pasar = (semaforo.estado == OE_VERDE);
    }
    omp_unset_lock(&semaforo.lock);
    
    return puede_pasar;
}

int main(){
    inicializar_sistema();
    return 0;
}