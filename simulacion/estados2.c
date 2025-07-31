#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <string.h>

// ============================================================================
// DEFINICIÓN DE CONSTANTES ESCALABLES
// ============================================================================

#define ENTRADA 0
#define SALIDA 1
#define ACTUALIZACION_VEHICULO 2

// Parámetros configurables de simulación
typedef struct {
    int num_secciones;
    double longitud_seccion;
    double longitud_total;
    double posicion_semaforo;
    double paso_simulacion;
    int max_autos;
    double intervalo_entrada_vehiculos;
    double tiempo_limite_simulacion;
} ConfiguracionSimulacion;

// Configuración por defecto
ConfiguracionSimulacion config = {
    .num_secciones = 20,
    .longitud_seccion = 10.0,
    .longitud_total = 200.0,
    .posicion_semaforo = 150.0,
    .paso_simulacion = 0.1,
    .max_autos = 50,
    .intervalo_entrada_vehiculos = 2.0,
    .tiempo_limite_simulacion = 3000.0
};

// ============================================================================
// ENUMERACIONES Y ESTRUCTURAS
// ============================================================================

typedef enum {
    ENTRANDO,
    ACELERANDO,
    VELOCIDAD_CONSTANTE,
    DESACELERANDO,
    FRENANDO,
    DETENIDO,
    SALIENDO
} EstadoVehiculo;

typedef enum {
    VERDE,
    AMARILLO,
    ROJO
} EstadoSemaforo;

typedef struct {
    int id;
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
    double tiempo_lento;
    int eventos_reanudacion;
    double tiempo_llegada_semaforo;
    double tiempo_salida;
    double velocidad_promedio;
    double distancia_recorrida;
    
    // Campos para debugging
    int actualizaciones_count;
    double ultimo_cambio_estado;
} Vehiculo;

typedef struct {
    double tiempo;
    int tipo;
    int id_auto;
    Vehiculo* vehiculo;
    int prioridad; // Para eventos críticos
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
} ColaEventos;

// Sistema escalable con arrays dinámicos
typedef struct {
    Vehiculo** vehiculos_activos;
    int num_vehiculos_activos;
    int capacidad_vehiculos;
    double tiempo_actual;
    
    // Estadísticas del sistema
    int total_vehiculos_creados;
    int total_vehiculos_completados;
    double tiempo_promedio_recorrido;
    double velocidad_promedio_sistema;
    
    // Control de memoria
    size_t memoria_utilizada;
} SistemaCalle;

typedef struct {
    double velocidad_maxima;
    double aceleracion_maxima;
    double desaceleracion_maxima;
    double desaceleracion_suave;
    double distancia_seguridad_min;
    double longitud_vehiculo;
    double tiempo_reaccion;
    double factor_congestion; // Nuevo: reduce velocidad en congestión
} ParametrosSimulacion;

typedef struct {
    double ultimo_cambio;
    EstadoSemaforo estado;
    double duracion_verde;
    double duracion_amarillo;
    double duracion_rojo;
    int ciclos_completados;
} SemaforoControl;

// ============================================================================
// VARIABLES GLOBALES
// ============================================================================

SistemaCalle calle = {0};
ParametrosSimulacion params = {
    .velocidad_maxima = 10.0,
    .aceleracion_maxima = 2.5,
    .desaceleracion_maxima = -4.0,
    .desaceleracion_suave = -2.0,
    .distancia_seguridad_min = 4.0,
    .longitud_vehiculo = 5.0,
    .tiempo_reaccion = 1.0,
    .factor_congestion = 0.8
};

SemaforoControl semaforo = {
    .ultimo_cambio = 0.0,
    .estado = VERDE,
    .duracion_verde = 25.0,
    .duracion_amarillo = 4.0,
    .duracion_rojo = 15.0,
    .ciclos_completados = 0
};

// ============================================================================
// FUNCIONES DE GESTIÓN DE MEMORIA ESCALABLE
// ============================================================================

void inicializar_sistema() {
    calle.capacidad_vehiculos = config.max_autos + 10; // Buffer extra
    calle.vehiculos_activos = (Vehiculo**)calloc(calle.capacidad_vehiculos, sizeof(Vehiculo*));
    calle.num_vehiculos_activos = 0;
    calle.total_vehiculos_creados = 0;
    calle.total_vehiculos_completados = 0;
    calle.memoria_utilizada = sizeof(SistemaCalle) + 
                             calle.capacidad_vehiculos * sizeof(Vehiculo*);
    
    if (!calle.vehiculos_activos) {
        fprintf(stderr, "ERROR: No se pudo asignar memoria para vehículos\n");
        exit(1);
    }
    
    printf("Sistema inicializado: capacidad %d vehículos, memoria: %zu bytes\n", 
           calle.capacidad_vehiculos, calle.memoria_utilizada);
}

void limpiar_sistema() {
    if (calle.vehiculos_activos) {
        free(calle.vehiculos_activos);
        calle.vehiculos_activos = NULL;
    }
    printf("Sistema limpiado. Memoria liberada.\n");
}

int redimensionar_sistema_si_necesario() {
    if (calle.num_vehiculos_activos >= calle.capacidad_vehiculos - 2) {
        int nueva_capacidad = calle.capacidad_vehiculos * 2;
        Vehiculo** nuevos_vehiculos = (Vehiculo**)realloc(calle.vehiculos_activos, 
                                                         nueva_capacidad * sizeof(Vehiculo*));
        
        if (!nuevos_vehiculos) {
            fprintf(stderr, "ERROR: No se pudo redimensionar array de vehículos\n");
            return 0;
        }
        
        // Inicializar nuevos elementos
        for (int i = calle.capacidad_vehiculos; i < nueva_capacidad; i++) {
            nuevos_vehiculos[i] = NULL;
        }
        
        calle.vehiculos_activos = nuevos_vehiculos;
        calle.capacidad_vehiculos = nueva_capacidad;
        
        printf("Sistema redimensionado a capacidad: %d vehículos\n", nueva_capacidad);
    }
    return 1;
}

// ============================================================================
// FUNCIONES DE CONTROL DE TRÁFICO
// ============================================================================

Vehiculo* encontrar_vehiculo_adelante_optimizado(double posicion, Vehiculo* vehiculo_actual) {
    Vehiculo* mas_cercano = NULL;
    double distancia_minima = config.longitud_total;
    
    // Búsqueda optimizada: solo en un rango relevante
    double rango_busqueda = 50.0; // Búsqueda hasta 50m adelante
    
    for (int i = 0; i < calle.num_vehiculos_activos; i++) {
        Vehiculo* v = calle.vehiculos_activos[i];
        
        if (v == vehiculo_actual || !v) continue;
        
        // Solo considerar vehículos en rango relevante
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

double calcular_distancia_seguridad_adaptativa(double velocidad, int densidad_trafico) {
    double base = params.distancia_seguridad_min;
    double reaccion = velocidad * params.tiempo_reaccion;
    double frenado = (velocidad * velocidad) / (2.0 * fabs(params.desaceleracion_maxima));
    
    // Ajustar por densidad de tráfico
    double factor_densidad = 1.0 + (densidad_trafico * 0.1);
    
    return (base + reaccion + frenado * 0.6) * factor_densidad;
}

int calcular_densidad_trafico_local(double posicion) {
    int vehiculos_cercanos = 0;
    double rango_analisis = 30.0;
    
    for (int i = 0; i < calle.num_vehiculos_activos; i++) {
        Vehiculo* v = calle.vehiculos_activos[i];
        if (v && fabs(v->posicion - posicion) <= rango_analisis) {
            vehiculos_cercanos++;
        }
    }
    
    return vehiculos_cercanos;
}

void ajustar_por_trafico_mejorado(Vehiculo* vehiculo, double dt) {
    Vehiculo* adelante = encontrar_vehiculo_adelante_optimizado(vehiculo->posicion, vehiculo);
    
    if (!adelante) return;
    
    double distancia_actual = adelante->posicion - vehiculo->posicion - params.longitud_vehiculo;
    int densidad = calcular_densidad_trafico_local(vehiculo->posicion);
    double distancia_requerida = calcular_distancia_seguridad_adaptativa(vehiculo->velocidad, densidad);
    
    // Lógica de frenado más sofisticada
    if (distancia_actual <= params.distancia_seguridad_min * 1.2) {
        // Frenado de emergencia
        vehiculo->estado = FRENANDO;
        vehiculo->aceleracion = params.desaceleracion_maxima;
        vehiculo->tiempo_total_frenado += dt;
        
        if (distancia_actual <= params.distancia_seguridad_min * 0.8) {
            printf("[%.2f] ALERTA: Vehiculo %d frenado emergencia (dist: %.2fm)\n", 
                   calle.tiempo_actual, vehiculo->id, distancia_actual);
        }
    } else if (distancia_actual <= distancia_requerida) {
        // Ajuste inteligente de velocidad
        double velocidad_objetivo = fmin(adelante->velocidad * 0.95, 
                                       vehiculo->velocidad * params.factor_congestion);
        
        if (vehiculo->velocidad > velocidad_objetivo) {
            vehiculo->estado = DESACELERANDO;
            vehiculo->aceleracion = params.desaceleracion_suave;
        }
    }
}

// ============================================================================
// FUNCIONES DE CONTROL DEL SEMÁFORO
// ============================================================================

void actualizar_semaforo_inteligente(double reloj) {
    double ciclo_total = semaforo.duracion_verde + semaforo.duracion_amarillo + semaforo.duracion_rojo;
    double t_ciclo = fmod(reloj, ciclo_total);
    
    EstadoSemaforo estado_anterior = semaforo.estado;
    
    if (t_ciclo < semaforo.duracion_verde) {
        semaforo.estado = VERDE;
    } else if (t_ciclo < semaforo.duracion_verde + semaforo.duracion_amarillo) {
        semaforo.estado = AMARILLO;
    } else {
        semaforo.estado = ROJO;
    }
    
    if (estado_anterior != semaforo.estado) {
        semaforo.ultimo_cambio = reloj;
        if (semaforo.estado == VERDE) {
            semaforo.ciclos_completados++;
        }
    }
}

// ============================================================================
// FUNCIONES DE GESTIÓN DE EVENTOS
// ============================================================================

void insertar_evento_optimizado(ColaEventos* cola, Evento evento) {
    Nodo* nuevo = (Nodo*)malloc(sizeof(Nodo));
    if (!nuevo) {
        fprintf(stderr, "ERROR: No se pudo asignar memoria para evento\n");
        return;
    }
    
    nuevo->evento = evento;
    nuevo->siguiente = nuevo->anterior = NULL;
    
    // Optimización: inserción al final si el tiempo es mayor que el último
    if (!cola->inicio) {
        cola->inicio = cola->fin = nuevo;
    } else if (!cola->fin || evento.tiempo >= cola->fin->evento.tiempo) {
        // Insertar al final (caso más común)
        nuevo->anterior = cola->fin;
        if (cola->fin) cola->fin->siguiente = nuevo;
        cola->fin = nuevo;
        if (!cola->inicio) cola->inicio = nuevo;
    } else if (evento.tiempo < cola->inicio->evento.tiempo) {
        // Insertar al inicio
        nuevo->siguiente = cola->inicio;
        cola->inicio->anterior = nuevo;
        cola->inicio = nuevo;
    } else {
        // Búsqueda desde el final
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
}

Evento* obtener_siguiente_evento_seguro(ColaEventos* cola) {
    if (!cola || !cola->inicio) return NULL;
    
    Nodo* nodo = cola->inicio;
    Evento* evento = (Evento*)malloc(sizeof(Evento));
    if (!evento) {
        fprintf(stderr, "ERROR: No se pudo asignar memoria para evento\n");
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
    return evento;
}

// ============================================================================
// FUNCIONES DE REPORTES
// ============================================================================

const char* estado_str(EstadoVehiculo estado) {
    switch (estado) {
        case ENTRANDO: return "ENTRANDO";
        case ACELERANDO: return "ACELERANDO";
        case VELOCIDAD_CONSTANTE: return "VEL_CONST";
        case DESACELERANDO: return "DESACELERANDO";
        case FRENANDO: return "FRENANDO";
        case DETENIDO: return "DETENIDO";
        case SALIENDO: return "SALIENDO";
        default: return "DESCONOCIDO";
    }
}

const char* color_semaforo(EstadoSemaforo e) {
    switch (e) {
        case VERDE: return "VERDE";
        case AMARILLO: return "AMARILLO";
        case ROJO: return "ROJO";
        default: return "DESCONOCIDO";
    }
}

void imprimir_estado_detallado() {
    printf("\n=== ESTADO t=%.2f | Semaforo: %s | Activos: %d/%d ===\n", 
           calle.tiempo_actual, color_semaforo(semaforo.estado),
           calle.num_vehiculos_activos, calle.total_vehiculos_creados);
    
    // Mostrar solo algunos vehículos para evitar spam
    int mostrar_hasta = fmin(calle.num_vehiculos_activos, 8);
    
    for (int i = 0; i < mostrar_hasta; i++) {
        Vehiculo* v = calle.vehiculos_activos[i];
        if (!v) continue;
        
        Vehiculo* adelante = encontrar_vehiculo_adelante_optimizado(v->posicion, v);
        double distancia = adelante ? (adelante->posicion - v->posicion - params.longitud_vehiculo) : -1;
        int densidad = calcular_densidad_trafico_local(v->posicion);
        
        printf("ID:%2d Pos=%6.1fm Vel=%5.2fm/s %s", 
               v->id, v->posicion, v->velocidad, estado_str(v->estado));
        
        if (adelante) {
            printf(" [->ID%d: %.1fm]", adelante->id, distancia);
        }
        printf(" Dens:%d\n", densidad);
    }
    
    if (calle.num_vehiculos_activos > mostrar_hasta) {
        printf("... y %d vehículos más\n", calle.num_vehiculos_activos - mostrar_hasta);
    }
    
    printf("Memoria: %zu KB | Cola max: %d eventos\n", 
           calle.memoria_utilizada / 1024, 0); // Actualizar con stats de cola
    printf("\n");
}

void imprimir_estadisticas_finales(Vehiculo** todos_vehiculos, int total) {
    printf("\n==== ESTADÍSTICAS FINALES DE LA SIMULACIÓN ====\n");
    printf("Tiempo total: %.2f segundos\n", calle.tiempo_actual);
    printf("Vehículos creados: %d\n", calle.total_vehiculos_creados);
    printf("Vehículos completados: %d\n", calle.total_vehiculos_completados);
    printf("Ciclos de semáforo: %d\n", semaforo.ciclos_completados);
    
    if (calle.total_vehiculos_completados > 0) {
        double tiempo_promedio = 0.0;
        double velocidad_promedio_total = 0.0;
        double tiempo_detenido_promedio = 0.0;
        
        for (int i = 0; i < total; i++) {
            if (todos_vehiculos[i] && todos_vehiculos[i]->estado == SALIENDO) {
                double tiempo_recorrido = todos_vehiculos[i]->tiempo_salida - todos_vehiculos[i]->tiempo_entrada;
                tiempo_promedio += tiempo_recorrido;
                
                if (tiempo_recorrido > 0) {
                    velocidad_promedio_total += config.longitud_total / tiempo_recorrido;
                }
                
                tiempo_detenido_promedio += todos_vehiculos[i]->tiempo_total_detenido;
            }
        }
        
        tiempo_promedio /= calle.total_vehiculos_completados;
        velocidad_promedio_total /= calle.total_vehiculos_completados;
        tiempo_detenido_promedio /= calle.total_vehiculos_completados;
        
        printf("Tiempo promedio de recorrido: %.2f segundos\n", tiempo_promedio);
        printf("Velocidad promedio: %.2f m/s (%.1f km/h)\n", 
               velocidad_promedio_total, velocidad_promedio_total * 3.6);
        printf("Tiempo promedio detenido: %.2f segundos\n", tiempo_detenido_promedio);
        printf("Eficiencia del sistema: %.1f%%\n", 
               (velocidad_promedio_total / params.velocidad_maxima) * 100.0);
    }
    
    printf("\n=== TABLA DETALLADA (primeros 10 vehículos) ===\n");
    printf("ID | Entrada | Semáforo | Salida | Detenido | Vel.Prom\n");
    printf("---|---------|----------|--------|----------|----------\n");
    
    int mostrar = fmin(total, 10);
    for (int i = 0; i < mostrar; i++) {
        if (todos_vehiculos[i]) {
            Vehiculo* v = todos_vehiculos[i];
            double vel_promedio = 0.0;
            if (v->tiempo_salida > v->tiempo_entrada) {
                vel_promedio = config.longitud_total / (v->tiempo_salida - v->tiempo_entrada);
            }
            
            printf("%2d | %7.2f | %8.2f | %6.2f | %8.2f | %8.2f\n",
                v->id, v->tiempo_entrada, v->tiempo_llegada_semaforo,
                v->tiempo_salida, v->tiempo_total_detenido, vel_promedio);
        }
    }
    printf("\n");
}

// ============================================================================
// FUNCIÓN PRINCIPAL
// ============================================================================

void procesar_eventos_escalable() {
    ColaEventos cola = {0};
    int id_auto = 1;
    Vehiculo** todos_vehiculos = (Vehiculo**)calloc(config.max_autos, sizeof(Vehiculo*));
    
    if (!todos_vehiculos) {
        fprintf(stderr, "ERROR: No se pudo asignar memoria para array de vehículos\n");
        return;
    }
    
    // Evento inicial
    Evento primer_evento = {0.0, ENTRADA, id_auto, NULL, 1};
    insertar_evento_optimizado(&cola, primer_evento);
    
    int eventos_procesados = 0;
    double ultimo_reporte = 0.0;
    
    // BUCLE PRINCIPAL
    while ((cola.inicio || calle.num_vehiculos_activos > 0) && 
           calle.tiempo_actual < config.tiempo_limite_simulacion) {
        
        Evento* e = obtener_siguiente_evento_seguro(&cola);
        
        if (!e) {
            if (calle.num_vehiculos_activos > 0) {
                printf("ADVERTENCIA: Sin eventos pero %d vehículos activos\n", 
                       calle.num_vehiculos_activos);
                break;
            }
            break;
        }
        
        calle.tiempo_actual = e->tiempo;
        actualizar_semaforo_inteligente(calle.tiempo_actual);
        eventos_procesados++;
        
        // PROCESAR ENTRADA DE VEHÍCULO
        if (e->tipo == ENTRADA && calle.total_vehiculos_creados < config.max_autos) {
            // Verificar espacio para entrada
            int puede_entrar = 1;
            for (int i = 0; i < calle.num_vehiculos_activos; i++) {
                Vehiculo* v = calle.vehiculos_activos[i];
                if (v && v->posicion < params.longitud_vehiculo + params.distancia_seguridad_min) {
                    puede_entrar = 0;
                    break;
                }
            }
            
            if (puede_entrar) {
                if (!redimensionar_sistema_si_necesario()) {
                    printf("ERROR: No se pudo redimensionar el sistema\n");
                    break;
                }
                
                Vehiculo* v = (Vehiculo*)calloc(1, sizeof(Vehiculo));
                if (!v) {
                    fprintf(stderr, "ERROR: No se pudo crear vehículo\n");
                    free(e);
                    continue;
                }
                
                // Inicializar vehículo
                v->id = id_auto;
                v->estado = ENTRANDO;
                v->aceleracion = params.aceleracion_maxima;
                v->tiempo_entrada = e->tiempo;
                v->ultimo_cambio_estado = e->tiempo;
                
                // Agregar a arrays
                todos_vehiculos[calle.total_vehiculos_creados] = v;
                calle.vehiculos_activos[calle.num_vehiculos_activos++] = v;
                calle.total_vehiculos_creados++;
                
                printf("[%.2f] Vehículo %d entra (total activos: %d)\n", 
                       calle.tiempo_actual, v->id, calle.num_vehiculos_activos);
                
                // Programar actualización
                Evento act = {e->tiempo + config.paso_simulacion, ACTUALIZACION_VEHICULO, v->id, v, 0};
                insertar_evento_optimizado(&cola, act);
                
                id_auto++;
                
                // Programar siguiente entrada si no hemos alcanzado el límite
                if (calle.total_vehiculos_creados < config.max_autos) {
                    double siguiente_entrada = e->tiempo + config.intervalo_entrada_vehiculos;
                    Evento sig = {siguiente_entrada, ENTRADA, id_auto, NULL, 1};
                    insertar_evento_optimizado(&cola, sig);
                }
            } else {
                // Reintentar entrada más tarde
                if (calle.total_vehiculos_creados < config.max_autos) {
                    Evento reintento = {e->tiempo + 1.0, ENTRADA, id_auto, NULL, 1};
                    insertar_evento_optimizado(&cola, reintento);
                }
            }
        }
        // PROCESAR ACTUALIZACIÓN DE VEHÍCULO
        else if (e->tipo == ACTUALIZACION_VEHICULO) {
            Vehiculo* v = e->vehiculo;
            
            if (!v || v->estado == SALIENDO) {
                free(e);
                continue;
            }
            
            v->actualizaciones_count++;
            double dt = config.paso_simulacion;
            double velocidad_anterior = v->velocidad;
            EstadoVehiculo estado_anterior = v->estado;
            
            // Registrar llegada al semáforo
            if (v->tiempo_llegada_semaforo == 0.0 && v->posicion >= config.posicion_semaforo) {
                v->tiempo_llegada_semaforo = calle.tiempo_actual;
            }
            
            // LÓGICA DE COMPORTAMIENTO MEJORADA
            if (v->velocidad == 0.0) {
                // Vehículo detenido - verificar si puede arrancar
                if (semaforo.estado == ROJO && v->posicion < config.posicion_semaforo) {
                    v->estado = DETENIDO;
                    v->aceleracion = 0.0;
                    v->tiempo_total_detenido += dt;
                } else {
                    Vehiculo* adelante = encontrar_vehiculo_adelante_optimizado(v->posicion, v);
                    if (adelante) {
                        double distancia = adelante->posicion - v->posicion - params.longitud_vehiculo;
                        if (distancia > params.distancia_seguridad_min * 1.5) {
                            v->estado = ACELERANDO;
                            v->aceleracion = params.aceleracion_maxima;
                        } else {
                            v->estado = DETENIDO;
                            v->aceleracion = 0.0;
                            v->tiempo_total_detenido += dt;
                        }
                    } else {
                        v->estado = ACELERANDO;
                        v->aceleracion = params.aceleracion_maxima;
                    }
                }
            } else {
                // Vehículo en movimiento
                ajustar_por_trafico_mejorado(v, dt);
                
                if (v->estado != FRENANDO && v->estado != DESACELERANDO) {
                    // Lógica de semáforo
                    if (semaforo.estado == ROJO && v->posicion < config.posicion_semaforo) {
                        double distancia_semaforo = config.posicion_semaforo - v->posicion;
                        double distancia_frenado = (v->velocidad * v->velocidad) / 
                                                  (2.0 * fabs(params.desaceleracion_maxima));
                        
                        if (distancia_frenado >= distancia_semaforo - 2.0) {
                            v->estado = FRENANDO;
                            v->aceleracion = params.desaceleracion_maxima;
                            v->tiempo_total_frenado += dt;
                        }
                    } else if (semaforo.estado == AMARILLO && v->posicion < config.posicion_semaforo) {
                        double distancia_semaforo = config.posicion_semaforo - v->posicion;
                        if (distancia_semaforo < 10.0) {
                            v->estado = DESACELERANDO;
                            v->aceleracion = params.desaceleracion_suave;
                        }
                    } else if (v->velocidad < params.velocidad_maxima - 0.2) {
                        v->estado = ACELERANDO;
                        v->aceleracion = params.aceleracion_maxima;
                    } else {
                        v->estado = VELOCIDAD_CONSTANTE;
                        v->aceleracion = 0.0;
                    }
                }
            }
            
            // Registrar cambio de estado
            if (estado_anterior != v->estado) {
                v->tiempo_en_estado = 0.0;
                v->ultimo_cambio_estado = calle.tiempo_actual;
            } else {
                v->tiempo_en_estado += dt;
            }
            
            // Actualizar estadísticas
            if (v->estado == DETENIDO) v->tiempo_total_detenido += dt;
            if (v->velocidad < params.velocidad_maxima / 2.0) v->tiempo_lento += dt;
            
            // ACTUALIZAR FÍSICA
            double nueva_velocidad = v->velocidad + v->aceleracion * dt;
            
            // Aplicar límites físicos
            if (nueva_velocidad < 0.0) nueva_velocidad = 0.0;
            if (nueva_velocidad > params.velocidad_maxima) nueva_velocidad = params.velocidad_maxima;
            
            // Detener si velocidad es muy pequeña y está desacelerando
            if (nueva_velocidad < 0.1 && v->aceleracion < 0) {
                nueva_velocidad = 0.0;
            }
            
            // Calcular nueva posición
            double nueva_posicion = v->posicion + nueva_velocidad * dt;
            
            // Verificar colisiones antes de actualizar posición
            Vehiculo* adelante = encontrar_vehiculo_adelante_optimizado(v->posicion, v);
            int puede_avanzar = 1;
            
            if (adelante) {
                double distancia_resultante = adelante->posicion - nueva_posicion - params.longitud_vehiculo;
                if (distancia_resultante < params.distancia_seguridad_min * 0.8) {
                    puede_avanzar = 0;
                }
            }
            
            if (puede_avanzar) {
                v->velocidad = nueva_velocidad;
                v->posicion = nueva_posicion;
                v->distancia_recorrida += nueva_velocidad * dt;
            } else {
                // Frenado de emergencia por proximidad
                v->velocidad = 0.0;
                v->estado = DETENIDO;
                v->aceleracion = 0.0;
                v->tiempo_total_detenido += dt;
            }
            
            // Detener cerca del semáforo si es necesario
            if (v->posicion >= config.posicion_semaforo - 3.0 && 
                v->posicion < config.posicion_semaforo && 
                v->velocidad < 1.0 && semaforo.estado == ROJO) {
                v->velocidad = 0.0;
                v->estado = DETENIDO;
                v->posicion = fmin(v->posicion, config.posicion_semaforo - 1.0);
            }
            
            // Contar reanudaciones
            if (v->velocidad > 0 && velocidad_anterior == 0) {
                v->eventos_reanudacion++;
            }
            
            // Calcular velocidad promedio
            if (v->actualizaciones_count > 0) {
                double tiempo_transcurrido = calle.tiempo_actual - v->tiempo_entrada;
                if (tiempo_transcurrido > 0) {
                    v->velocidad_promedio = v->distancia_recorrida / tiempo_transcurrido;
                }
            }
            
            // VERIFICAR SALIDA DEL SISTEMA
            if (v->posicion >= config.longitud_total) {
                v->tiempo_salida = calle.tiempo_actual;
                v->estado = SALIENDO;
                calle.total_vehiculos_completados++;
                
                double tiempo_total = v->tiempo_salida - v->tiempo_entrada;
                printf("[%.2f] Vehículo %d completa recorrido en %.2fs (vel.prom: %.2fm/s)\n",
                       calle.tiempo_actual, v->id, tiempo_total, v->velocidad_promedio);
                
                // Remover de vehículos activos
                for (int i = 0; i < calle.num_vehiculos_activos; i++) {
                    if (calle.vehiculos_activos[i] == v) {
                        calle.vehiculos_activos[i] = calle.vehiculos_activos[calle.num_vehiculos_activos - 1];
                        calle.vehiculos_activos[calle.num_vehiculos_activos - 1] = NULL;
                        calle.num_vehiculos_activos--;
                        break;
                    }
                }
            } else {
                // Programar siguiente actualización
                Evento siguiente = {e->tiempo + config.paso_simulacion, ACTUALIZACION_VEHICULO, v->id, v, 0};
                insertar_evento_optimizado(&cola, siguiente);
            }
        }
        
        // REPORTES PERIÓDICOS (cada 10 segundos)
        if (calle.tiempo_actual - ultimo_reporte >= 10.0) {
            imprimir_estado_detallado();
            ultimo_reporte = calle.tiempo_actual;
            
            // Verificar si la simulación se está colgando
            if (eventos_procesados > config.max_autos * 10000) {
                printf("ADVERTENCIA: Demasiados eventos procesados (%d). Terminando simulación.\n", 
                       eventos_procesados);
                break;
            }
        }
        
        free(e);
    }
    
    // LIMPIEZA Y REPORTES FINALES
    printf("\n=== SIMULACIÓN COMPLETADA ===\n");
    printf("Eventos procesados: %d\n", eventos_procesados);
    printf("Tiempo límite alcanzado: %s\n", 
           (calle.tiempo_actual >= config.tiempo_limite_simulacion) ? "SÍ" : "NO");
    
    imprimir_estadisticas_finales(todos_vehiculos, calle.total_vehiculos_creados);
    
    // Liberar memoria de vehículos restantes en la cola
    while (cola.inicio) {
        Evento* e = obtener_siguiente_evento_seguro(&cola);
        if (e) free(e);
    }
    
    // Liberar memoria de todos los vehículos
    for (int i = 0; i < calle.total_vehiculos_creados; i++) {
        if (todos_vehiculos[i]) {
            free(todos_vehiculos[i]);
            todos_vehiculos[i] = NULL;
        }
    }
    
    free(todos_vehiculos);
    printf("Memoria de vehículos liberada correctamente.\n");
}

// ============================================================================
// FUNCIONES DE CONFIGURACIÓN Y VALIDACIÓN
// ============================================================================

int validar_configuracion() {
    if (config.max_autos <= 0 || config.max_autos > 10000) {
        fprintf(stderr, "ERROR: max_autos debe estar entre 1 y 10000\n");
        return 0;
    }
    
    if (config.paso_simulacion <= 0.001 || config.paso_simulacion > 1.0) {
        fprintf(stderr, "ERROR: paso_simulacion debe estar entre 0.001 y 1.0\n");
        return 0;
    }
    
    if (config.longitud_total <= 0 || config.posicion_semaforo >= config.longitud_total) {
        fprintf(stderr, "ERROR: Configuración de longitudes inválida\n");
        return 0;
    }
    
    if (params.velocidad_maxima <= 0 || params.aceleracion_maxima <= 0) {
        fprintf(stderr, "ERROR: Parámetros físicos inválidos\n");
        return 0;
    }
    
    return 1;
}

void configurar_simulacion_personalizada() {
    printf("=== CONFIGURACIÓN DE SIMULACIÓN ===\n");
    printf("¿Desea usar configuración personalizada? (s/n): ");
    
    char respuesta;
    scanf(" %c", &respuesta);
    
    if (respuesta == 's' || respuesta == 'S') {
        printf("Número máximo de autos (actual: %d): ", config.max_autos);
        scanf("%d", &config.max_autos);
        
        printf("Longitud total de la calle en metros (actual: %.1f): ", config.longitud_total);
        scanf("%lf", &config.longitud_total);
        
        printf("Posición del semáforo en metros (actual: %.1f): ", config.posicion_semaforo);
        scanf("%lf", &config.posicion_semaforo);
        
        printf("Intervalo entre entradas de vehículos en segundos (actual: %.1f): ", 
               config.intervalo_entrada_vehiculos);
        scanf("%lf", &config.intervalo_entrada_vehiculos);
        
        printf("Velocidad máxima en m/s (actual: %.1f): ", params.velocidad_maxima);
        scanf("%lf", &params.velocidad_maxima);
        
        printf("Duración de luz verde en segundos (actual: %.1f): ", semaforo.duracion_verde);
        scanf("%lf", &semaforo.duracion_verde);
        
        printf("Duración de luz roja en segundos (actual: %.1f): ", semaforo.duracion_rojo);
        scanf("%lf", &semaforo.duracion_rojo);
    }
    
    // Mostrar configuración final
    printf("\n=== CONFIGURACIÓN FINAL ===\n");
    printf("Máximo autos: %d\n", config.max_autos);
    printf("Longitud calle: %.1f m\n", config.longitud_total);
    printf("Posición semáforo: %.1f m\n", config.posicion_semaforo);
    printf("Intervalo entrada: %.1f s\n", config.intervalo_entrada_vehiculos);
    printf("Velocidad máxima: %.1f m/s (%.1f km/h)\n", 
           params.velocidad_maxima, params.velocidad_maxima * 3.6);
    printf("Semáforo - Verde: %.1fs, Rojo: %.1fs\n", 
           semaforo.duracion_verde, semaforo.duracion_rojo);
    printf("Paso de simulación: %.3f s\n", config.paso_simulacion);
    printf("Tiempo límite: %.1f s\n", config.tiempo_limite_simulacion);
    printf("===============================\n\n");
}

// ============================================================================
// FUNCIÓN PRINCIPAL
// ============================================================================

int main() {
    srand((unsigned int)time(NULL));
    
    printf("=== SIMULACIÓN DE TRÁFICO ESCALABLE v2.0 ===\n");
    printf("Características:\n");
    printf("- Gestión dinámica de memoria\n");
    printf("- Control avanzado de colisiones\n");
    printf("- Semáforo inteligente\n");
    printf("- Estadísticas detalladas\n");
    printf("- Escalable hasta miles de vehículos\n\n");
    
    // Configuración interactiva
    configurar_simulacion_personalizada();
    
    // Validar configuración
    if (!validar_configuracion()) {
        fprintf(stderr, "Configuración inválida. Terminando.\n");
        return 1;
    }
    
    // Inicializar sistema
    inicializar_sistema();
    
    // Ejecutar simulación
    printf("Iniciando simulación...\n\n");
    clock_t inicio = clock();
    
    procesar_eventos_escalable();
    
    clock_t fin = clock();
    double tiempo_ejecucion = ((double)(fin - inicio)) / CLOCKS_PER_SEC;
    
    printf("\n=== MÉTRICAS DE RENDIMIENTO ===\n");
    printf("Tiempo de ejecución: %.3f segundos\n", tiempo_ejecucion);
    printf("Tiempo simulado: %.2f segundos\n", calle.tiempo_actual);
    printf("Factor de aceleración: %.1fx\n", calle.tiempo_actual / tiempo_ejecucion);
    printf("Vehículos procesados: %d\n", calle.total_vehiculos_creados);
    printf("Throughput: %.1f vehículos/segundo de simulación\n", 
           (double)calle.total_vehiculos_completados / calle.tiempo_actual);
    
    // Limpiar sistema
    limpiar_sistema();
    
    printf("\nSimulación completada exitosamente.\n");
    return 0;
}