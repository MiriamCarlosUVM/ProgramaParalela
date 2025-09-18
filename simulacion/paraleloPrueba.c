#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <string.h>
#include <omp.h>

// ============================================================================
// DEFINICIÓN DE CONSTANTES
// ============================================================================

#define ENTRADA 0
#define SALIDA 1
#define ACTUALIZACION_VEHICULO 2

// Solo 2 direcciones de tráfico
typedef enum {
    NORTE_SUR = 0,    // Calle vertical: Norte → Sur
    OESTE_ESTE = 1    // Calle horizontal: Oeste → Este
} DireccionCalle;

// Configuración de intersección
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
    
    // Inicializar cada calle (solo 2)
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

// ============================================================================
// CONTROL DE INTERSECCIÓN
// ============================================================================

int verificar_colision_interseccion(Vehiculo* v) {
    omp_set_lock(&interseccion.lock_interseccion);
    
    int colision = 0;
    for (int i = 0; i < interseccion.num_en_interseccion; i++) {
        Vehiculo* otro = interseccion.vehiculos_en_interseccion[i];
        if (otro && otro != v) {
            double dist = sqrt(pow(v->coord_x - otro->coord_x, 2) + 
                              pow(v->coord_y - otro->coord_y, 2));
            if (dist < params.distancia_seguridad_min) {
                colision = 1;
                break;
            }
        }
    }
    
    omp_unset_lock(&interseccion.lock_interseccion);
    return colision;
}

void agregar_a_interseccion(Vehiculo* v) {
    omp_set_lock(&interseccion.lock_interseccion);
    
    if (interseccion.num_en_interseccion < interseccion.capacidad_interseccion) {
        interseccion.vehiculos_en_interseccion[interseccion.num_en_interseccion++] = v;
        v->estado = EN_INTERSECCION;
    }
    
    omp_unset_lock(&interseccion.lock_interseccion);
}

void remover_de_interseccion(Vehiculo* v) {
    omp_set_lock(&interseccion.lock_interseccion);
    
    for (int i = 0; i < interseccion.num_en_interseccion; i++) {
        if (interseccion.vehiculos_en_interseccion[i] == v) {
            interseccion.vehiculos_en_interseccion[i] = 
                interseccion.vehiculos_en_interseccion[interseccion.num_en_interseccion - 1];
            interseccion.vehiculos_en_interseccion[interseccion.num_en_interseccion - 1] = NULL;
            interseccion.num_en_interseccion--;
            break;
        }
    }
    
    omp_unset_lock(&interseccion.lock_interseccion);
}

// ============================================================================
// LÓGICA DE MOVIMIENTO
// ============================================================================

Vehiculo* encontrar_vehiculo_adelante(DireccionCalle calle_dir, double posicion, Vehiculo* vehiculo_actual) {
    SistemaCalle* calle = &interseccion.calles[calle_dir];
    Vehiculo* mas_cercano = NULL;
    double distancia_minima = 1000.0;
    
    omp_set_lock(&calle->lock);
    
    for (int i = 0; i < calle->num_vehiculos_activos; i++) {
        Vehiculo* v = calle->vehiculos[i];
        if (v && v != vehiculo_actual) {
            double diferencia = v->posicion - posicion;
            if (diferencia > 0 && diferencia < distancia_minima) {
                distancia_minima = diferencia;
                mas_cercano = v;
            }
        }
    }
    
    omp_unset_lock(&calle->lock);
    return mas_cercano;
}

void actualizar_vehiculo(Vehiculo* v, double dt) {
    // Calcular coordenadas absolutas
    calcular_coordenadas_absolutas(v);
    
    // Verificar si está en intersección
    int en_interseccion = esta_en_interseccion(v);
    
    // Lógica de comportamiento según estado
    if (v->estado == EN_INTERSECCION) {
        if (!en_interseccion) {
            // Saliendo de intersección
            remover_de_interseccion(v);
            v->estado = ACELERANDO;
        } else {
            // Mantener velocidad en intersección
            v->aceleracion = 0.0;
            if (v->velocidad < params.velocidad_maxima * 0.8) {
                v->aceleracion = params.aceleracion_maxima * 0.3;
            }
        }
    } else if (en_interseccion && v->estado != EN_INTERSECCION) {
        // Intentando entrar a intersección
        if (!verificar_colision_interseccion(v)) {
            agregar_a_interseccion(v);
        } else {
            // Esperar - no puede entrar
            v->estado = DETENIDO;
            v->velocidad = 0.0;
            v->aceleracion = 0.0;
            v->tiempo_total_detenido += dt;
        }
    } else {
        // Lógica normal de tráfico
        
        // 1. Verificar vehículo adelante
        Vehiculo* adelante = encontrar_vehiculo_adelante(v->calle, v->posicion, v);
        if (adelante) {
            double distancia = adelante->posicion - v->posicion - params.longitud_vehiculo;
            if (distancia < params.distancia_seguridad_min * 1.5) {
                v->estado = FRENANDO;
                v->aceleracion = params.desaceleracion_maxima;
            } else if (distancia < params.distancia_seguridad_min * 3.0) {
                v->estado = DESACELERANDO;
                v->aceleracion = params.desaceleracion_maxima * 0.5;
            }
        }
        
        // 2. Verificar semáforo
        double dist_semaforo = distancia_al_semaforo(v);
        if (dist_semaforo > 0 && dist_semaforo < 40.0) {
            if (!puede_pasar_semaforo(v)) {
                double distancia_frenado = (v->velocidad * v->velocidad) / 
                                          (2.0 * fabs(params.desaceleracion_maxima));
                if (distancia_frenado >= dist_semaforo - 2.0) {
                    v->estado = FRENANDO;
                    v->aceleracion = params.desaceleracion_maxima;
                }
            }
        }
        
        // 3. Acelerar si no hay restricciones
        if (v->estado != FRENANDO && v->estado != DESACELERANDO && 
            v->estado != DETENIDO && v->velocidad < params.velocidad_maxima) {
            v->estado = ACELERANDO;
            v->aceleracion = params.aceleracion_maxima;
        }
    }
    
    // Actualizar física
    double nueva_velocidad = v->velocidad + v->aceleracion * dt;
    if (nueva_velocidad < 0.0) nueva_velocidad = 0.0;
    if (nueva_velocidad > params.velocidad_maxima) nueva_velocidad = params.velocidad_maxima;
    
    if (nueva_velocidad < 0.1 && v->aceleracion < 0) {
        nueva_velocidad = 0.0;
        v->estado = DETENIDO;
        v->tiempo_total_detenido += dt;
    }
    
    v->velocidad = nueva_velocidad;
    v->posicion += v->velocidad * dt;
    v->distancia_recorrida += v->velocidad * dt;
    v->actualizaciones_count++;
    
    // Verificar salida del sistema
    double longitud_total = (v->calle == NORTE_SUR) ? 
                           config.longitud_calle_ns : config.longitud_calle_oe;
    
    if (v->posicion >= longitud_total) {
        v->estado = SALIENDO;
        v->tiempo_salida = interseccion.tiempo_actual;
    }
}

// ============================================================================
// CREACIÓN Y GESTIÓN DE VEHÍCULOS
// ============================================================================

Vehiculo* crear_vehiculo(DireccionCalle calle_dir, int id) {
    Vehiculo* v = (Vehiculo*)calloc(1, sizeof(Vehiculo));
    if (!v) return NULL;
    
    v->id = id;
    v->calle = calle_dir;
    v->posicion = 0.0;
    v->velocidad = 0.0;
    v->estado = ENTRANDO;
    v->aceleracion = params.aceleracion_maxima;
    v->tiempo_entrada = interseccion.tiempo_actual;
    
    return v;
}

void procesar_calle(DireccionCalle calle_dir) {
    SistemaCalle* calle = &interseccion.calles[calle_dir];
    static int contador_ids[2] = {1, 1001}; // IDs únicos por calle
    static double ultimo_vehiculo[2] = {0.0, 0.0};
    
    double dt = config.paso_simulacion;
    
    // Crear nuevos vehículos si es necesario
    if (calle->total_vehiculos_creados < config.max_autos_por_calle) {
        if (interseccion.tiempo_actual - ultimo_vehiculo[calle_dir] >= 
            config.intervalo_entrada_vehiculos) {
            
            // Verificar espacio para entrada
            int puede_entrar = 1;
            omp_set_lock(&calle->lock);
            for (int i = 0; i < calle->num_vehiculos_activos; i++) {
                if (calle->vehiculos[i] && calle->vehiculos[i]->posicion < params.longitud_vehiculo + 3.0) {
                    puede_entrar = 0;
                    break;
                }
            }
            omp_unset_lock(&calle->lock);
            
            if (puede_entrar) {
                Vehiculo* nuevo = crear_vehiculo(calle_dir, contador_ids[calle_dir]++);
                if (nuevo) {
                    omp_set_lock(&calle->lock);
                    if (calle->num_vehiculos_activos < calle->capacidad_vehiculos) {
                        calle->vehiculos[calle->num_vehiculos_activos++] = nuevo;
                        calle->total_vehiculos_creados++;
                        ultimo_vehiculo[calle_dir] = interseccion.tiempo_actual;
                    }
                    omp_unset_lock(&calle->lock);
                }
            }
        }
    }
    
    // Actualizar vehículos existentes
    omp_set_lock(&calle->lock);
    for (int i = 0; i < calle->num_vehiculos_activos; i++) {
        Vehiculo* v = calle->vehiculos[i];
        if (v) {
            omp_unset_lock(&calle->lock); // Liberar para la actualización
            actualizar_vehiculo(v, dt);
            omp_set_lock(&calle->lock);   // Volver a adquirir
            
            // Verificar si el vehículo completó el recorrido
            if (v->estado == SALIENDO) {
                calle->total_vehiculos_completados++;
                
                // Remover del array activo
                calle->vehiculos[i] = calle->vehiculos[calle->num_vehiculos_activos - 1];
                calle->vehiculos[calle->num_vehiculos_activos - 1] = NULL;
                calle->num_vehiculos_activos--;
                i--; // Ajustar índice
                
                free(v); // Liberar memoria del vehículo
            }
        }
    }
    omp_unset_lock(&calle->lock);
}

// ============================================================================
// SIMULACIÓN PRINCIPAL
// ============================================================================

void ejecutar_simulacion_paralela() {
    printf("Iniciando simulación de 2 calles en paralelo...\n");
    
    double tiempo_final = 0.0;
    int total_vehiculos = config.max_autos_por_calle * 2;
    
    // Estimar tiempo final
    if (config.tiempo_limite_simulacion == 0.0) {
        double tiempo_por_vehiculo = fmax(config.longitud_calle_ns, config.longitud_calle_oe) / 
                                    params.velocidad_maxima + 15.0; // Buffer para semáforos
        tiempo_final = tiempo_por_vehiculo * config.max_autos_por_calle + 
                      config.intervalo_entrada_vehiculos * config.max_autos_por_calle;
    } else {
        tiempo_final = config.tiempo_limite_simulacion;
    }
    
    printf("Simulación estimada: %.1f segundos con %d vehículos total\n", 
           tiempo_final, total_vehiculos);
    
    double ultimo_reporte = 0.0;
    
    // BUCLE PRINCIPAL PARALELO
    while (interseccion.tiempo_actual < tiempo_final) {
        
        // Actualizar semáforo (un solo thread)
        #pragma omp single
        {
            actualizar_semaforo(interseccion.tiempo_actual);
        }
        
        // Procesar cada calle en paralelo (2 threads)
        #pragma omp parallel sections
        {
            #pragma omp section
            {
                procesar_calle(NORTE_SUR);
            }
            
            #pragma omp section  
            {
                procesar_calle(OESTE_ESTE);
            }
        }
        
        // Avanzar tiempo (un solo thread)
        #pragma omp single
        {
            interseccion.tiempo_actual += config.paso_simulacion;
            
            // Reportes periódicos
            if (interseccion.tiempo_actual - ultimo_reporte >= 15.0) {
                int total_activos = interseccion.calles[0].num_vehiculos_activos + 
                                   interseccion.calles[1].num_vehiculos_activos;
                int total_completados = interseccion.calles[0].total_vehiculos_completados + 
                                       interseccion.calles[1].total_vehiculos_completados;
                
                printf("\n=== T=%.1fs | Sem:%s | NS:%d/%d | OE:%d/%d | Intersección:%d ===\n",
                       interseccion.tiempo_actual,
                       (semaforo.estado == NS_VERDE) ? "NS-Verde" : "OE-Verde",
                       interseccion.calles[0].num_vehiculos_activos,
                       interseccion.calles[0].total_vehiculos_completados,
                       interseccion.calles[1].num_vehiculos_activos,
                       interseccion.calles[1].total_vehiculos_completados,
                       interseccion.num_en_interseccion);
                
                ultimo_reporte = interseccion.tiempo_actual;
            }
        }
        
        // Verificar condición de terminación temprana
        if (config.tiempo_limite_simulacion == 0.0) {
            int todos_completados = 1;
            #pragma omp single
            {
                if (interseccion.calles[0].total_vehiculos_completados < config.max_autos_por_calle ||
                    interseccion.calles[1].total_vehiculos_completados < config.max_autos_por_calle) {
                    todos_completados = 0;
                }
            }
            if (todos_completados) break;
        }
    }
    
    printf("Simulación completada en %.2f segundos\n", interseccion.tiempo_actual);
}

// ============================================================================
// REPORTES Y ESTADÍSTICAS
// ============================================================================

const char* calle_str(DireccionCalle calle) {
    return (calle == NORTE_SUR) ? "Norte→Sur" : "Oeste→Este";
}

void imprimir_estadisticas_finales() {
    printf("\n==== ESTADÍSTICAS FINALES ====\n");
    printf("Tiempo total: %.2f segundos\n", interseccion.tiempo_actual);
    printf("Ciclos de semáforo: %d\n", semaforo.ciclos_completados);
    
    int total_creados = 0;
    int total_completados = 0;
    
    printf("\n=== ESTADÍSTICAS POR CALLE ===\n");
    printf("Calle        | Creados | Completados | Eficiencia\n");
    printf("-------------|---------|-------------|----------\n");
    
    for (int c = 0; c < 2; c++) {
        SistemaCalle* calle = &interseccion.calles[c];
        total_creados += calle->total_vehiculos_creados;
        total_completados += calle->total_vehiculos_completados;
        
        double eficiencia = (calle->total_vehiculos_creados > 0) ? 
                           ((double)calle->total_vehiculos_completados / calle->total_vehiculos_creados) * 100.0 : 0.0;
        
        printf("%-12s | %7d | %11d | %9.1f%%\n",
               calle_str((DireccionCalle)c),
               calle->total_vehiculos_creados,
               calle->total_vehiculos_completados,
               eficiencia);
    }
    
    printf("-------------|---------|-------------|----------\n");
    double eficiencia_total = (total_creados > 0) ? 
                             ((double)total_completados / total_creados) * 100.0 : 0.0;
    printf("TOTAL        | %7d | %11d | %9.1f%%\n",
           total_creados, total_completados, eficiencia_total);
    
    printf("\nThroughput total: %.2f vehículos/segundo\n", 
           (double)total_completados / interseccion.tiempo_actual);
    
    // Guardar en CSV
    char nombre_archivo[128];
    time_t ahora = time(NULL);
    struct tm *t = localtime(&ahora);
    strftime(nombre_archivo, sizeof(nombre_archivo), "interseccion_simple_%Y%m%d_%H%M%S.csv", t);
    
    FILE *csv = fopen(nombre_archivo, "w");
    if (csv) {
        fprintf(csv, "# SIMULACION_2_CALLES\n");
        fprintf(csv, "TiempoTotal,%.2f\n", interseccion.tiempo_actual);
        fprintf(csv, "CiclosSemaforo,%d\n", semaforo.ciclos_completados);
        fprintf(csv, "VehiculosCreados,%d\n", total_creados);
        fprintf(csv, "VehiculosCompletados,%d\n", total_completados);
        fprintf(csv, "EficienciaTotal,%.1f\n", eficiencia_total);
        fprintf(csv, "Throughput,%.2f\n", (double)total_completados / interseccion.tiempo_actual);
        fprintf(csv, "\nCalle,Creados,Completados,Eficiencia\n");
        
        for (int c = 0; c < 2; c++) {
            SistemaCalle* calle = &interseccion.calles[c];
            double efic = (calle->total_vehiculos_creados > 0) ? 
                         ((double)calle->total_vehiculos_completados / calle->total_vehiculos_creados) * 100.0 : 0.0;
            fprintf(csv, "%s,%d,%d,%.1f\n", 
                    calle_str((DireccionCalle)c),
                    calle->total_vehiculos_creados,
                    calle->total_vehiculos_completados,
                    efic);
        }
        
        fclose(csv);
        printf("Resultados guardados en: %s\n", nombre_archivo);
    } else {
        printf("ERROR: No se pudo crear archivo CSV\n");
    }
}

// ============================================================================
// CONFIGURACIÓN
// ============================================================================

void configurar_simulacion() {
    printf("=== CONFIGURACIÓN DE INTERSECCIÓN DE 2 CALLES ===\n");
    printf("Calle 1: Norte → Sur (vertical)\n");
    printf("Calle 2: Oeste → Este (horizontal)\n");
    printf("Se cruzan en una intersección con semáforo\n\n");
    
    char usar_personalizada;
    printf("¿Usar configuración personalizada? (s/n): ");
    scanf(" %c", &usar_personalizada);
    
    if (usar_personalizada == 's' || usar_personalizada == 'S') {
        printf("\n--- CONFIGURACIÓN BÁSICA ---\n");
        
        printf("Vehículos por calle (actual: %d): ", config.max_autos_por_calle);
        int temp;
        if (scanf("%d", &temp) == 1 && temp > 0 && temp <= 100) {
            config.max_autos_por_calle = temp;
        }
        
        printf("Intervalo de entrada (seg, actual: %.1f): ", config.intervalo_entrada_vehiculos);
        double temp_d;
        if (scanf("%lf", &temp_d) == 1 && temp_d > 0.5 && temp_d < 10.0) {
            config.intervalo_entrada_vehiculos = temp_d;
        }
        
        printf("\n--- CONFIGURACIÓN DE CALLES ---\n");
        printf("Longitud calle Norte-Sur (m, actual: %.1f): ", config.longitud_calle_ns);
        if (scanf("%lf", &temp_d) == 1 && temp_d > 100.0 && temp_d < 1000.0) {
            config.longitud_calle_ns = temp_d;
        }
        
        printf("Longitud calle Oeste-Este (m, actual: %.1f): ", config.longitud_calle_oe);
        if (scanf("%lf", &temp_d) == 1 && temp_d > 100.0 && temp_d < 1000.0) {
            config.longitud_calle_oe = temp_d;
        }
        
        printf("Ancho de intersección (m, actual: %.1f): ", config.ancho_interseccion);
        if (scanf("%lf", &temp_d) == 1 && temp_d > 5.0 && temp_d < 50.0) {
            config.ancho_interseccion = temp_d;
        }
        
        printf("\n--- CONFIGURACIÓN DE SEMÁFOROS ---\n");
        printf("Duración verde Norte-Sur (seg, actual: %.1f): ", semaforo.duracion_ns_verde);
        if (scanf("%lf", &temp_d) == 1 && temp_d > 10.0 && temp_d < 120.0) {
            semaforo.duracion_ns_verde = temp_d;
        }
        
        printf("Duración verde Oeste-Este (seg, actual: %.1f): ", semaforo.duracion_oe_verde);
        if (scanf("%lf", &temp_d) == 1 && temp_d > 10.0 && temp_d < 120.0) {
            semaforo.duracion_oe_verde = temp_d;
        }
        
        printf("Duración amarillo (seg, actual: %.1f): ", semaforo.duracion_amarillo);
        if (scanf("%lf", &temp_d) == 1 && temp_d > 2.0 && temp_d < 10.0) {
            semaforo.duracion_amarillo = temp_d;
        }
        
        printf("\n--- CONFIGURACIÓN AVANZADA ---\n");
        printf("¿Configurar parámetros avanzados? (s/n): ");
        char avanzado;
        scanf(" %c", &avanzado);
        
        if (avanzado == 's' || avanzado == 'S') {
            printf("Velocidad máxima (m/s, actual: %.1f): ", params.velocidad_maxima);
            if (scanf("%lf", &temp_d) == 1 && temp_d > 3.0 && temp_d < 30.0) {
                params.velocidad_maxima = temp_d;
            }
            
            printf("Paso de simulación (seg, actual: %.3f): ", config.paso_simulacion);
            if (scanf("%lf", &temp_d) == 1 && temp_d > 0.01 && temp_d < 0.2) {
                config.paso_simulacion = temp_d;
            }
            
            printf("Distancia seguridad (m, actual: %.1f): ", params.distancia_seguridad_min);
            if (scanf("%lf", &temp_d) == 1 && temp_d > 2.0 && temp_d < 15.0) {
                params.distancia_seguridad_min = temp_d;
            }
        }
    }
    
    // Ajustar posiciones de semáforos automáticamente
    config.posicion_semaforo_ns = config.longitud_calle_ns / 2.0 - config.ancho_interseccion / 2.0 - 5.0;
    config.posicion_semaforo_oe = config.longitud_calle_oe / 2.0 - config.ancho_interseccion / 2.0 - 5.0;
    
    printf("\n=== CONFIGURACIÓN FINAL ===\n");
    printf("Vehículos por calle: %d (Total: %d)\n", 
           config.max_autos_por_calle, config.max_autos_por_calle * 2);
    printf("Calle Norte-Sur: %.1fm\n", config.longitud_calle_ns);
    printf("Calle Oeste-Este: %.1fm\n", config.longitud_calle_oe);
    printf("Intersección: %.1fm x %.1fm\n", 
           config.ancho_interseccion, config.ancho_interseccion);
    printf("Semáforo NS: %.1fs verde, OE: %.1fs verde, Amarillo: %.1fs\n", 
           semaforo.duracion_ns_verde, semaforo.duracion_oe_verde, semaforo.duracion_amarillo);
    printf("Velocidad máxima: %.1f m/s (%.1f km/h)\n", 
           params.velocidad_maxima, params.velocidad_maxima * 3.6);
    printf("OpenMP threads: %d\n", omp_get_max_threads());
    printf("===============================\n\n");
}

// ============================================================================
// FUNCIÓN MAIN
// ============================================================================

int main() {
    printf("=== SIMULADOR DE INTERSECCIÓN SIMPLE CON OPENMP ===\n");
    printf("Simula 2 calles que se cruzan:\n");
    printf("• Calle Norte → Sur (solo una dirección)\n");
    printf("• Calle Oeste → Este (solo una dirección)\n");
    printf("• Intersección controlada por semáforo\n");
    printf("• Procesamiento paralelo con OpenMP (2 threads)\n\n");
    
    // Configurar OpenMP para 2 threads
    omp_set_num_threads(2);
    printf("OpenMP configurado con %d threads\n\n", omp_get_max_threads());
    
    // Configuración
    configurar_simulacion();
    
    // Validación básica
    if (config.max_autos_por_calle <= 0 || config.max_autos_por_calle > 200) {
        fprintf(stderr, "ERROR: Número de vehículos por calle inválido\n");
        return 1;
    }
    
    if (config.longitud_calle_ns <= config.ancho_interseccion || 
        config.longitud_calle_oe <= config.ancho_interseccion) {
        fprintf(stderr, "ERROR: Las calles deben ser más largas que la intersección\n");
        return 1;
    }
    
    // Inicializar sistema
    inicializar_sistema();
    
    // Ejecutar simulación
    printf("Iniciando simulación paralela de 2 calles...\n");
    
    clock_t inicio = clock();
    
    #pragma omp parallel
    {
        ejecutar_simulacion_paralela();
    }
    
    clock_t fin = clock();
    double tiempo_ejecucion = ((double)(fin - inicio)) / CLOCKS_PER_SEC;
    
    // Mostrar resultados
    imprimir_estadisticas_finales();
    
    printf("\n=== RENDIMIENTO ===\n");
    printf("Tiempo de ejecución: %.3f segundos\n", tiempo_ejecucion);
    printf("Tiempo simulado: %.2f segundos\n", interseccion.tiempo_actual);
    printf("Factor de aceleración: %.1fx\n", interseccion.tiempo_actual / tiempo_ejecucion);
    
    int total_vehiculos = interseccion.calles[0].total_vehiculos_completados + 
                         interseccion.calles[1].total_vehiculos_completados;
    printf("Vehículos completados: %d\n", total_vehiculos);
    printf("Throughput: %.1f vehículos/segundo real\n", 
           (double)total_vehiculos / tiempo_ejecucion);
    
    // Análisis de eficiencia de intersección
    double ciclo_total = semaforo.duracion_ns_verde + semaforo.duracion_amarillo +
                        semaforo.duracion_oe_verde + semaforo.duracion_amarillo;
    double utilizacion_ns = (semaforo.duracion_ns_verde + semaforo.duracion_amarillo) / ciclo_total * 100.0;
    double utilizacion_oe = (semaforo.duracion_oe_verde + semaforo.duracion_amarillo) / ciclo_total * 100.0;
    
    printf("\nEficiencia de semáforo:\n");
    printf("• Norte-Sur utiliza %.1f%% del tiempo\n", utilizacion_ns);
    printf("• Oeste-Este utiliza %.1f%% del tiempo\n", utilizacion_oe);
    printf("• Ciclos completados: %d\n", semaforo.ciclos_completados);
    
    // Limpiar sistema
    limpiar_sistema();
    
    printf("\nSimulación completada exitosamente.\n");
    return 0;
}