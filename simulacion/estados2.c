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
    .paso_simulacion = 0.05,
    .max_autos = 50,
    .intervalo_entrada_vehiculos = 2.0,
    .tiempo_limite_simulacion = 0.0  // 0 = sin límite de tiempo
};

// ============================================================================
// ENUMERACIONES Y ESTRUCTURAS MEJORADAS
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
    double factor_congestion; // Nuevo: reduce velocidad en congestion
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
// VARIABLES GLOBALES MEJORADAS
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
        fprintf(stderr, "ERROR: No se pudo asignar memoria para vehiculos\n");
        exit(1);
    }
    
    printf("Sistema inicializado: capacidad %d vehiculos, memoria: %zu bytes\n", 
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
            fprintf(stderr, "ERROR: No se pudo redimensionar array de vehiculos\n");
            return 0;
        }
        
        // Inicializar nuevos elementos
        for (int i = calle.capacidad_vehiculos; i < nueva_capacidad; i++) {
            nuevos_vehiculos[i] = NULL;
        }
        
        calle.vehiculos_activos = nuevos_vehiculos;
        calle.capacidad_vehiculos = nueva_capacidad;
        
        printf("Sistema redimensionado a capacidad: %d vehiculos\n", nueva_capacidad);
    }
    return 1;
}

// ============================================================================
// FUNCIONES DE CONTROL DE TRÁFICO MEJORADAS
// ============================================================================

Vehiculo* encontrar_vehiculo_adelante_optimizado(double posicion, Vehiculo* vehiculo_actual) {
    Vehiculo* mas_cercano = NULL;
    double distancia_minima = config.longitud_total;
    
    // Búsqueda optimizada: solo en un rango relevante
    double rango_busqueda = 50.0; // Búsqueda hasta 50m adelante
    
    for (int i = 0; i < calle.num_vehiculos_activos; i++) {
        Vehiculo* v = calle.vehiculos_activos[i];
        
        if (v == vehiculo_actual || !v) continue;
        
        // Solo considerar vehiculos en rango relevante
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
// FUNCIONES DE CONTROL DEL Semaforo CON LÓGICA INTELIGENTE
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
// FUNCIONES DE GESTIÓN DE EVENTOS MEJORADAS
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
        // Búsqueda desde el final (más eficiente para eventos próximos)
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
// FUNCIONES DE REPORTES MEJORADAS
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
    
    // Mostrar solo algunos vehiculos para evitar spam
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
        printf("... y %d vehiculos más\n", calle.num_vehiculos_activos - mostrar_hasta);
    }
    
    printf("Memoria: %zu KB | Cola max: %d eventos\n", 
           calle.memoria_utilizada / 1024, 0); // Actualizar con stats de cola
    printf("\n");
}

void imprimir_estadisticas_finales(Vehiculo** todos_vehiculos, int total) {
    printf("\n==== ESTADISTICAS FINALES DE LA SIMULACIÓN ====\n");
    printf("Tiempo total: %.2f segundos\n", calle.tiempo_actual);
    printf("Vehiculos creados: %d\n", calle.total_vehiculos_creados);
    printf("Vehiculos completados: %d\n", calle.total_vehiculos_completados);
    printf("Ciclos de Semaforo: %d\n", semaforo.ciclos_completados);

    //  Crear nombre de archivo con fecha y hora
    char nombre_archivo[128];
    time_t ahora = time(NULL);
    struct tm *t = localtime(&ahora);
    strftime(nombre_archivo, sizeof(nombre_archivo), "resultados_%Y%m%d_%H%M%S.csv", t);

    //  Abrir archivo CSV
    FILE *csv = fopen(nombre_archivo, "w");
    if (!csv) {
        printf("❌ ERROR: No se pudo crear archivo CSV: %s\n", nombre_archivo);
        perror("Detalle del error");
    } else {
        fprintf(csv, "ID,Entrada,Semaforo,Salida,Detenido,VelProm\n");
        printf("✅ Creando archivo: %s\n", nombre_archivo);
    }

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

        //  Guardar estadísticas generales en CSV
        if (csv) {
            fprintf(csv, "# ESTADISTICAS_GENERALES\n");
            fprintf(csv, "TiempoTotal (s),%.2f\n", calle.tiempo_actual);
            fprintf(csv, "VehiculosCreados,%d\n", calle.total_vehiculos_creados);
            fprintf(csv, "VehiculosCompletados,%d\n", calle.total_vehiculos_completados);
            fprintf(csv, "CiclosSemaforo (Verde-Verde 1 ciclo),%d\n", semaforo.ciclos_completados);
            fprintf(csv, "TiempoPromedio (s),%.2f\n", tiempo_promedio);
            fprintf(csv, "VelocidadPromedio (m/s),%.2f\n", velocidad_promedio_total);
            fprintf(csv, "TiempoDetenidoPromedio (s),%.2f\n", tiempo_detenido_promedio);
            fprintf(csv, "Eficiencia (%),%.1f\n", (velocidad_promedio_total / params.velocidad_maxima) * 100.0);
            fprintf(csv, "# DATOS_VEHICULOS\n");
            fprintf(csv, "ID,Entrada,Semaforo,Salida,Detenido,VelProm\n");
        }
    }

    printf("\n=== TABLA DETALLADA ===\n");
    printf("ID | Entrada | Semaforo | Salida | Detenido | Vel.Prom\n");
    printf("---|---------|----------|--------|----------|----------\n");

    //  Guardar TODOS los vehiculos completados
    int contador_csv = 0;
    for (int i = 0; i < total; i++) {
        if (todos_vehiculos[i] && todos_vehiculos[i]->estado == SALIENDO) {
            Vehiculo* v = todos_vehiculos[i];
            double vel_promedio = 0.0;
            if (v->tiempo_salida > v->tiempo_entrada) {
                vel_promedio = config.longitud_total / (v->tiempo_salida - v->tiempo_entrada);
            }

            // Imprimir en terminal (solo primeros 10 para visualización)
            if (contador_csv < 10) {
                printf("%2d | %7.2f | %8.2f | %6.2f | %8.2f | %8.2f\n",
                    v->id, v->tiempo_entrada, v->tiempo_llegada_semaforo,
                    v->tiempo_salida, v->tiempo_total_detenido, vel_promedio);
            }

            // Guardar en CSV (TODOS los vehiculos)
            if (csv) {
                fprintf(csv, "%d,%.2f,%.2f,%.2f,%.2f,%.2f\n",
                        v->id, v->tiempo_entrada, v->tiempo_llegada_semaforo,
                        v->tiempo_salida, v->tiempo_total_detenido, vel_promedio);
                fflush(csv);  // Forzar escritura
            }
            contador_csv++;
        }
    }

    //  Cerrar CSV
    if (csv) {
        fclose(csv);
        printf("\n✅ Resultados guardados en %s (%d vehiculos)\n", nombre_archivo, contador_csv);
    } else {
        printf("\n❌ No se pudo guardar archivo CSV\n");
    }

    printf("\n");
}


// ============================================================================
// FUNCIONES PARA CSV DE ESTADOS DETALLADOS
// ============================================================================

FILE *csv_estados = NULL;

// Abrir archivo CSV de estados
void inicializar_csv_estados() {
    char nombre_archivo[128];
    time_t ahora = time(NULL);
    struct tm *t = localtime(&ahora);
    strftime(nombre_archivo, sizeof(nombre_archivo), "estados_%Y%m%d_%H%M%S.csv", t);

    csv_estados = fopen(nombre_archivo, "w");
    if (!csv_estados) {
        printf("❌ ERROR: No se pudo crear archivo de estados: %s\n", nombre_archivo);
        perror("Detalle del error");
        return;
    }

    // Encabezado
    fprintf(csv_estados, "Tiempo,ID,Posicion,Velocidad,Aceleracion,EstadoVehiculo,Semaforo\n");
    fflush(csv_estados);

    printf("✅ Archivo de estados creado: %s\n", nombre_archivo);
}

// Registrar estado de vehículo
void registrar_estado_vehiculo(Vehiculo* v) {
    if (!csv_estados || !v) return;

    fprintf(csv_estados, "%.2f,%d,%.2f,%.2f,%.2f,%s,%s\n",
            calle.tiempo_actual,
            v->id,
            v->posicion,
            v->velocidad,
            v->aceleracion,
            estado_str(v->estado),
            color_semaforo(semaforo.estado));
    fflush(csv_estados);
}

// Cerrar CSV de estados
void cerrar_csv_estados() {
    if (csv_estados) {
        fclose(csv_estados);
        csv_estados = NULL;
        printf("✅ Archivo de estados cerrado correctamente.\n");
    }
}


// ============================================================================
// FUNCIÓN PRINCIPAL MEJORADA Y ESCALABLE
// ============================================================================

void procesar_eventos_escalable() {
    ColaEventos cola = {0};
    int id_auto = 1;
    Vehiculo** todos_vehiculos = (Vehiculo**)calloc(config.max_autos, sizeof(Vehiculo*));
    
    if (!todos_vehiculos) {
        fprintf(stderr, "ERROR: No se pudo asignar memoria para array de vehiculos\n");
        return;
    }
    
    // Evento inicial
    Evento primer_evento = {0.0, ENTRADA, id_auto, NULL, 1};
    insertar_evento_optimizado(&cola, primer_evento);
    
    int eventos_procesados = 0;
    double ultimo_reporte = 0.0;
    
    // BUCLE PRINCIPAL MEJORADO - SIN LÍMITE DE TIEMPO
    while ((cola.inicio || calle.num_vehiculos_activos > 0)) {
        
        Evento* e = obtener_siguiente_evento_seguro(&cola);
        
        if (!e) {
            if (calle.num_vehiculos_activos > 0) {
                printf("ADVERTENCIA: Sin eventos pero %d vehiculos activos en t=%.2f\n", 
                       calle.num_vehiculos_activos, calle.tiempo_actual);
                
                // Debug: mostrar vehiculos restantes
                printf("Vehiculos restantes:\n");
                for (int i = 0; i < calle.num_vehiculos_activos; i++) {
                    Vehiculo* v = calle.vehiculos_activos[i];
                    if (v) {
                        printf("  ID:%d Pos:%.1f Vel:%.2f Estado:%s\n", 
                               v->id, v->posicion, v->velocidad, estado_str(v->estado));
                    }
                }
            }
            break;
        }
        
        calle.tiempo_actual = e->tiempo;
        actualizar_semaforo_inteligente(calle.tiempo_actual);
        eventos_procesados++;
        
        // Protección contra bucles infinitos (basada en eventos, no tiempo)
        if (eventos_procesados > config.max_autos * 50000) {
            printf("ADVERTENCIA: Demasiados eventos procesados (%d). Verificando estado...\n", 
                   eventos_procesados);
            
            // Verificar si hay progreso
            static double ultima_posicion_maxima = 0.0;
            double posicion_maxima_actual = 0.0;
            
            for (int i = 0; i < calle.num_vehiculos_activos; i++) {
                if (calle.vehiculos_activos[i] && 
                    calle.vehiculos_activos[i]->posicion > posicion_maxima_actual) {
                    posicion_maxima_actual = calle.vehiculos_activos[i]->posicion;
                }
            }
            
            if (posicion_maxima_actual <= ultima_posicion_maxima) {
                printf("ERROR: No hay progreso en la simulación. Terminando para evitar bucle infinito.\n");
                printf("Posición máxima: %.2f, Vehiculos activos: %d\n", 
                       posicion_maxima_actual, calle.num_vehiculos_activos);
                break;
            }
            
            ultima_posicion_maxima = posicion_maxima_actual;
            eventos_procesados = 0; // Reiniciar contador
        }
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
                    fprintf(stderr, "ERROR: No se pudo crear vehiculo\n");
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
                
                printf("[%.2f] Vehiculo %d entra (total activos: %d)\n", 
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
            
            // Registrar llegada al Semaforo
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
                    // Lógica de Semaforo
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
            
            // Detener cerca del Semaforo si es necesario
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
            
            registrar_estado_vehiculo(v);

            // VERIFICAR SALIDA DEL SISTEMA
            if (v->posicion >= config.longitud_total) {
                v->tiempo_salida = calle.tiempo_actual;
                v->estado = SALIENDO;
                calle.total_vehiculos_completados++;
                
                double tiempo_total = v->tiempo_salida - v->tiempo_entrada;
                printf("[%.2f] Vehiculo %d completa recorrido en %.2fs (vel.prom: %.2fm/s)\n",
                       calle.tiempo_actual, v->id, tiempo_total, v->velocidad_promedio);
                
                // Remover de vehiculos activos
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
        
        // REPORTES PERIÓDICOS (cada 10 segundos) - Menos frecuentes para simulaciones largas
        if (calle.tiempo_actual - ultimo_reporte >= 20.0) {
            imprimir_estado_detallado();
            ultimo_reporte = calle.tiempo_actual;
            
            // Mostrar progreso de completitud
            double porcentaje_completado = (double)calle.total_vehiculos_completados / config.max_autos * 100.0;
            printf(">>> PROGRESO: %.1f%% completado (%d/%d vehiculos) - Tiempo: %.1fs <<<\n\n", 
                   porcentaje_completado, calle.total_vehiculos_completados, config.max_autos, calle.tiempo_actual);
        }
        
        free(e);
    }
    
    // LIMPIEZA Y REPORTES FINALES
    printf("\n=== SIMULACION COMPLETADA ===\n");
    printf("Eventos procesados: %d\n", eventos_procesados);
    
    // Verificar si todos los vehiculos completaron el recorrido
    if (calle.total_vehiculos_completados == config.max_autos) {
        printf("✓ EXITO: Todos los %d vehiculos completaron el recorrido\n", config.max_autos);
    } else {
        printf("⚠ INCOMPLETO: %d/%d vehiculos completaron el recorrido\n", 
               calle.total_vehiculos_completados, config.max_autos);
        printf("Vehiculos restantes en el sistema: %d\n", calle.num_vehiculos_activos);
    }
    
    printf("Tiempo total de simulacion: %.2f segundos\n", calle.tiempo_actual);
    
    imprimir_estadisticas_finales(todos_vehiculos, calle.total_vehiculos_creados);
    
    // Liberar memoria de vehiculos restantes en la cola
    while (cola.inicio) {
        Evento* e = obtener_siguiente_evento_seguro(&cola);
        if (e) free(e);
    }
    
    // Liberar memoria de todos los vehiculos
    for (int i = 0; i < calle.total_vehiculos_creados; i++) {
        if (todos_vehiculos[i]) {
            free(todos_vehiculos[i]);
            todos_vehiculos[i] = NULL;
        }
    }
    
    free(todos_vehiculos);
    printf("Memoria de vehiculos liberada correctamente.\n");
}

// ============================================================================
// FUNCIONES DE CONFIGURACIÓN Y VALIDACIÓN
// ============================================================================

int validar_configuracion() {
    int errores = 0;
    
    printf("\n=== VALIDACION FINAL ===\n");
    
    // Validar límites básicos
    if (config.max_autos <= 0 || config.max_autos > 5000) {
        fprintf(stderr, "ERROR: max_autos debe estar entre 1 y 5000 (actual: %d)\n", config.max_autos);
        errores++;
    }
    
    if (config.paso_simulacion <= 0.001 || config.paso_simulacion > 1.0) {
        fprintf(stderr, "ERROR: paso_simulacion debe estar entre 0.001 y 1.0 (actual: %.3f)\n", 
                config.paso_simulacion);
        errores++;
    }
    
    if (config.longitud_total <= 0 || config.longitud_total > 2000.0) {
        fprintf(stderr, "ERROR: longitud_total debe estar entre 1 y 2000m (actual: %.1f)\n", 
                config.longitud_total);
        errores++;
    }
    
    if (config.posicion_semaforo <= 0 || config.posicion_semaforo >= config.longitud_total) {
        fprintf(stderr, "ERROR: posicion_semaforo debe estar entre 1 y %.1fm (actual: %.1f)\n", 
                config.longitud_total - 1, config.posicion_semaforo);
        errores++;
    }
    
    if (config.intervalo_entrada_vehiculos <= 0 || config.intervalo_entrada_vehiculos > 10.0) {
        fprintf(stderr, "ERROR: intervalo_entrada_vehiculos debe estar entre 0.1 y 10.0s (actual: %.2f)\n", 
                config.intervalo_entrada_vehiculos);
        errores++;
    }
    
    if (params.velocidad_maxima <= 0 || params.velocidad_maxima > 50.0) {
        fprintf(stderr, "ERROR: velocidad_maxima debe estar entre 1 y 50 m/s (actual: %.1f)\n", 
                params.velocidad_maxima);
        errores++;
    }
    
    if (params.aceleracion_maxima <= 0 || params.aceleracion_maxima > 10.0) {
        fprintf(stderr, "ERROR: aceleracion_maxima debe estar entre 0.5 y 10.0 m/s² (actual: %.1f)\n", 
                params.aceleracion_maxima);
        errores++;
    }
    
    if (params.desaceleracion_maxima >= 0 || params.desaceleracion_maxima < -15.0) {
        fprintf(stderr, "ERROR: desaceleracion_maxima debe estar entre -15.0 y -1.0 m/s² (actual: %.1f)\n", 
                params.desaceleracion_maxima);
        errores++;
    }
    
    if (semaforo.duracion_verde < 5.0 || semaforo.duracion_verde > 120.0) {
        fprintf(stderr, "ERROR: duracion_verde debe estar entre 5 y 120s (actual: %.1f)\n", 
                semaforo.duracion_verde);
        errores++;
    }
    
    if (semaforo.duracion_rojo < 5.0 || semaforo.duracion_rojo > 120.0) {
        fprintf(stderr, "ERROR: duracion_rojo debe estar entre 5 y 120s (actual: %.1f)\n", 
                semaforo.duracion_rojo);
        errores++;
    }
    
    // Validar tiempo límite (opcional)
    if (config.tiempo_limite_simulacion < 0.0 || config.tiempo_limite_simulacion > 7200.0) {
        if (config.tiempo_limite_simulacion != 0.0) { // 0 = sin límite, es válido
            fprintf(stderr, "ERROR: tiempo_limite_simulacion debe ser 0 (sin límite) o entre 1 y 7200s (actual: %.1f)\n", 
                    config.tiempo_limite_simulacion);
            errores++;
        }
    }
    
    // Validaciones de coherencia
    double distancia_frenado = (params.velocidad_maxima * params.velocidad_maxima) / 
                              (2.0 * fabs(params.desaceleracion_maxima));
    if (distancia_frenado > config.posicion_semaforo * 0.8) {
        printf("ADVERTENCIA: Distancia de frenado (%.1fm) muy grande comparada con posición del Semaforo (%.1fm)\n", 
               distancia_frenado, config.posicion_semaforo);
    }
    
    double tiempo_ciclo = semaforo.duracion_verde + semaforo.duracion_amarillo + semaforo.duracion_rojo;
    double vehiculos_por_ciclo = tiempo_ciclo / config.intervalo_entrada_vehiculos;
    if (vehiculos_por_ciclo > 20) {
        printf("ADVERTENCIA: Se crearán muchos vehiculos por ciclo de Semaforo (%.1f). Puede causar congestión.\n", 
               vehiculos_por_ciclo);
    }
    
    if (config.paso_simulacion > config.intervalo_entrada_vehiculos / 5.0) {
        printf("ADVERTENCIA: Paso de simulación grande comparado con intervalo de entrada. Puede afectar precisión.\n");
    }
    
    // Estimación de memoria requerida
    size_t memoria_estimada = config.max_autos * sizeof(Vehiculo) + 
                             config.max_autos * sizeof(Vehiculo*) + 
                             sizeof(SistemaCalle);
    double memoria_mb = memoria_estimada / (1024.0 * 1024.0);
    
    if (memoria_mb > 100.0) {
        printf("ADVERTENCIA: Memoria estimada: %.1f MB. Simulación puede ser lenta.\n", memoria_mb);
    } else {
        printf("Memoria estimada: %.2f MB\n", memoria_mb);
    }
    
    // Estimación de tiempo de ejecucion
    if (config.tiempo_limite_simulacion == 0.0) {
        // Sin límite de tiempo - estimar basado en el recorrido completo
        double tiempo_minimo_recorrido = config.longitud_total / params.velocidad_maxima;
        double tiempo_estimado_total = tiempo_minimo_recorrido * config.max_autos * 1.5; // Factor de seguridad
        printf("Tiempo estimado de simulación: %.1f segundos (todos los vehiculos)\n", tiempo_estimado_total);
        
        if (tiempo_estimado_total > 1800) { // 30 minutos
            printf("ADVERTENCIA: Simulación larga estimada (%.1f min). Considere reducir número de vehiculos.\n", 
                   tiempo_estimado_total / 60.0);
        }
    } else {
        printf("Límite de tiempo configurado: %.1f segundos\n", config.tiempo_limite_simulacion);
    }
    
    double eventos_estimados = config.max_autos * (config.longitud_total / params.velocidad_maxima) / config.paso_simulacion;
    if (eventos_estimados > 2000000) {
        printf("ADVERTENCIA: Eventos estimados: %.0f. Simulación puede tardar mucho tiempo.\n", eventos_estimados);
    }
    
    if (errores > 0) {
        fprintf(stderr, "\nSe encontraron %d errores en la configuracion.\n", errores);
        return 0;
    } else {
        printf("✓ Configuracion valida\n");
        return 1;
    }
}

// Función auxiliar para limpiar el buffer de entrada
void limpiar_buffer() {
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}

// Función auxiliar para leer enteros con validación
int leer_entero_validado(const char* mensaje, int valor_actual, int min, int max) {
    int valor, resultado;
    char buffer[100];
    
    do {
        printf("%s (actual: %d, rango: %d-%d): ", mensaje, valor_actual, min, max);
        
        if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
            printf("Error de lectura. Usando valor actual.\n");
            return valor_actual;
        }
        
        // Verificar si solo presionó Enter (usar valor actual)
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

// Función auxiliar para leer doubles con validación
double leer_double_validado(const char* mensaje, double valor_actual, double min, double max) {
    double valor;
    int resultado;
    char buffer[100];
    
    do {
        printf("%s (actual: %.2f, rango: %.2f-%.2f): ", mensaje, valor_actual, min, max);
        
        if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
            printf("Error de lectura. Usando valor actual.\n");
            return valor_actual;
        }
        
        // Verificar si solo presionó Enter (usar valor actual)
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

// Función auxiliar para leer un carácter s/n con validación
char leer_si_no(const char* mensaje) {
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

void configurar_simulacion_personalizada() {
    printf("=== CONFIGURACION DE SIMULACION ===\n");
    printf("Instrucciones:\n");
    printf("- Presione Enter para mantener el valor actual\n");
    printf("- Los valores deben estar dentro de los rangos permitidos\n");
    printf("- Use punto decimal (.) para números decimales\n\n");
    
    char respuesta = leer_si_no("¿Desea usar configuracion personalizada?");
    
    if (respuesta == 's' || respuesta == 'S') {
        printf("\n--- CONFIGURACION DE VEHICULOS ---\n");
        config.max_autos = leer_entero_validado(
            "Número máximo de autos", 
            config.max_autos, 1, 5000
        );
        
        config.intervalo_entrada_vehiculos = leer_double_validado(
            "Intervalo entre entradas de vehiculos (segundos)",
            config.intervalo_entrada_vehiculos, 0.1, 10.0
        );
        
        printf("\n--- CONFIGURACIÓN DE LA CALLE ---\n");
        config.longitud_total = leer_double_validado(
            "Longitud total de la calle (metros)",
            config.longitud_total, 50.0, 2000.0
        );
        
        // Validar que el Semaforo esté dentro de la calle
        double max_semaforo = config.longitud_total - 10.0;
        if (config.posicion_semaforo >= max_semaforo) {
            config.posicion_semaforo = max_semaforo;
        }
        
        config.posicion_semaforo = leer_double_validado(
            "Posicion del semaforo (metros desde el inicio)",
            config.posicion_semaforo, 10.0, max_semaforo
        );
        
        printf("\n--- CONFIGURACION DE VELOCIDAD ---\n");
        params.velocidad_maxima = leer_double_validado(
            "Velocidad máxima (m/s)",
            params.velocidad_maxima, 1.0, 50.0
        );
        
        printf("    (%.1f m/s = %.1f km/h)\n", 
               params.velocidad_maxima, params.velocidad_maxima * 3.6);
        
        printf("\n--- CONFIGURACION DEL Semaforo ---\n");
        semaforo.duracion_verde = leer_double_validado(
            "Duración de luz verde (segundos)",
            semaforo.duracion_verde, 5.0, 120.0
        );
        
        semaforo.duracion_amarillo = leer_double_validado(
            "Duración de luz amarilla (segundos)",
            semaforo.duracion_amarillo, 1.0, 10.0
        );
        
        semaforo.duracion_rojo = leer_double_validado(
            "Duración de luz roja (segundos)",
            semaforo.duracion_rojo, 5.0, 120.0
        );
        
        printf("\n--- CONFIGURACION AVANZADA ---\n");
        char config_avanzada = leer_si_no("¿Configurar parámetros avanzados?");
        
        if (config_avanzada == 's' || config_avanzada == 'S') {
            config.paso_simulacion = leer_double_validado(
                "Paso de simulación (segundos)",
                config.paso_simulacion, 0.01, 0.5
            );
            
            // Opción para límite de tiempo (opcional)
            char usar_limite = leer_si_no("¿Usar límite de tiempo? (recomendado: NO para completar todos los vehiculos)");
            if (usar_limite == 's' || usar_limite == 'S') {
                config.tiempo_limite_simulacion = leer_double_validado(
                    "Tiempo límite de simulación (segundos, 0 = sin límite)",
                    config.tiempo_limite_simulacion, 0.0, 7200.0
                );
            } else {
                config.tiempo_limite_simulacion = 0.0; // Sin límite
                printf("✓ Simulación sin límite de tiempo - todos los vehiculos completarán el recorrido\n");
            }
            
            params.aceleracion_maxima = leer_double_validado(
                "Aceleración máxima (m/s²)",
                params.aceleracion_maxima, 0.5, 10.0
            );
            
            params.desaceleracion_maxima = leer_double_validado(
                "Desaceleración máxima (m/s²) - valor positivo",
                fabs(params.desaceleracion_maxima), 1.0, 15.0
            );
            params.desaceleracion_maxima = -fabs(params.desaceleracion_maxima);
            
            params.distancia_seguridad_min = leer_double_validado(
                "Distancia mínima de seguridad (metros)",
                params.distancia_seguridad_min, 1.0, 20.0
            );
        } else {
            // Si no configura avanzado, mantener sin límite de tiempo
            config.tiempo_limite_simulacion = 0.0;
            printf("✓ Usando configuracion estAndar sin lImite de tiempo\n");
        }
        
        // Validaciones adicionales entre parámetros
        printf("\n--- VALIDANDO CONFIGURACION ---\n");
        
        // Ajustar paso de simulación si es necesario
        double paso_max_recomendado = config.intervalo_entrada_vehiculos / 10.0;
        if (config.paso_simulacion > paso_max_recomendado) {
            printf("ADVERTENCIA: Paso de simulacion muy grande. Ajustando a %.3f\n", 
                   paso_max_recomendado);
            config.paso_simulacion = paso_max_recomendado;
        }
        
        // Verificar que el Semaforo no esté muy cerca del final
        if (config.posicion_semaforo > config.longitud_total * 0.9) {
            printf("ADVERTENCIA: Semaforo muy cerca del final. Puede afectar los resultados.\n");
        }
        
        // Verificar coherencia de velocidades y distancias
        double tiempo_frenado = params.velocidad_maxima / fabs(params.desaceleracion_maxima);
        double distancia_frenado = 0.5 * params.velocidad_maxima * tiempo_frenado;
        if (distancia_frenado > config.longitud_total * 0.3) {
            printf("ADVERTENCIA: Distancia de frenado (%.1fm) muy grande para la calle.\n", 
                   distancia_frenado);
        }
        
        printf("Configuracion validada correctamente.\n");
    } else {
        printf("Usando configuracion por defecto.\n");
    }
    
    // Mostrar configuración final
    printf("\n=== CONFIGURACIoN FINAL ===\n");
    printf("Máximo autos: %d\n", config.max_autos);
    printf("Longitud calle: %.1f m\n", config.longitud_total);
    printf("Posición Semaforo: %.1f m\n", config.posicion_semaforo);
    printf("Intervalo entrada: %.1f s\n", config.intervalo_entrada_vehiculos);
    printf("Velocidad máxima: %.1f m/s (%.1f km/h)\n", 
           params.velocidad_maxima, params.velocidad_maxima * 3.6);
    printf("Semaforo - Verde: %.1fs, Rojo: %.1fs\n", 
           semaforo.duracion_verde, semaforo.duracion_rojo);
    printf("Paso de simulación: %.3f s\n", config.paso_simulacion);
    
    if (config.tiempo_limite_simulacion > 0.0) {
        printf("Límite de tiempo: %.1f s\n", config.tiempo_limite_simulacion);
    } else {
        printf("Límite de tiempo: NINGUNO (completar todos los vehiculos)\n");
    }
    
    printf("===============================\n\n");
}

// ============================================================================
// FUNCIÓN PRINCIPAL MEJORADA
// ============================================================================

int main() {
    srand((unsigned int)time(NULL));
    
    printf("=== SIMULACION DE TRAFICO ESCALABLE v2.0 ===\n");
    printf("Características:\n");
    printf("- Gestion dinamica de memoria\n");
    printf("- Control avanzado de colisiones\n");
    printf("- Semaforo inteligente\n");
    printf("- Estadisticas detalladas\n");
    printf("- Escalable hasta miles de vehiculos\n\n");
    
    // Configuracion interactiva
    configurar_simulacion_personalizada();
    
    // Validar configuracion
    if (!validar_configuracion()) {
        fprintf(stderr, "Configuracion invalida. Terminando.\n");
        return 1;
    }
    
    // Inicializar sistema
    inicializar_sistema();
    
    // Inicializar CSV de estados
    inicializar_csv_estados();

    // Ejecutar simulación
    printf("Iniciando simulación...\n\n");
    clock_t inicio = clock();
    
    procesar_eventos_escalable();

    // Cerrar CSV de estados
    cerrar_csv_estados();
    
    clock_t fin = clock();
    double tiempo_ejecucion = ((double)(fin - inicio)) / CLOCKS_PER_SEC;
    
    printf("\n=== METRICAS DE RENDIMIENTO ===\n");
    printf("Tiempo de ejecucion: %.3f segundos\n", tiempo_ejecucion);
    printf("Tiempo simulado: %.2f segundos\n", calle.tiempo_actual);
    printf("Factor de aceleracion: %.1fx\n", calle.tiempo_actual / tiempo_ejecucion);
    printf("Vehiculos procesados: %d\n", calle.total_vehiculos_creados);
    printf("Throughput: %.1f vehiculos/segundo de simulacion\n", 
           (double)calle.total_vehiculos_completados / calle.tiempo_actual);
    
    // Limpiar sistema
    limpiar_sistema();
    
    printf("\nSimulacion completada exitosamente.\n");
    return 0;
}