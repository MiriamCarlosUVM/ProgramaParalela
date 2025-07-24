#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <string.h>

// ============================================================================
// DEFINICIÓN DE CONSTANTES
// ============================================================================

// Tipos de eventos que pueden ocurrir en la simulación
#define ENTRADA 0              // Un vehículo entra al sistema
#define SALIDA 1               // Un vehículo sale del sistema (no usado actualmente)
#define ACTUALIZACION_VEHICULO 2  // Actualizar posición y estado de un vehículo

// Parámetros de la calle y simulación
#define NUM_SECCIONES 10       // Número de secciones en que se divide la calle
#define LONGITUD_SECCION 10.0  // Longitud de cada sección en metros
#define LONGITUD_TOTAL 100.0   // Longitud total de la calle en metros

#define POSICION_SEMAFORO 100.0 // Posición del semáforo en metros desde el inicio

#define PASO_SIMULACION 0.1    // Intervalo de tiempo entre actualizaciones (segundos)
#define MAX_AUTOS 10          // Número máximo de autos a simular

// ============================================================================
// ENUMERACIONES - ESTADOS DEL SISTEMA
// ============================================================================

// Estados posibles de un vehículo durante la simulación
typedef enum {
    ENTRANDO,           // Vehículo acaba de entrar al sistema
    ACELERANDO,         // Vehículo está acelerando hacia velocidad máxima
    VELOCIDAD_CONSTANTE,// Vehículo mantiene velocidad constante
    DESACELERANDO,      // Vehículo está reduciendo velocidad gradualmente
    FRENANDO,           // Vehículo está frenando (por semáforo o obstáculo)
    DETENIDO,           // Vehículo completamente detenido
    SALIENDO            // Vehículo ha completado el recorrido
} EstadoVehiculo;

// Estados posibles del semáforo
typedef enum {
    VERDE,              // Los vehículos pueden pasar
    AMARILLO,           // Advertencia - próximo a cambiar a rojo
    ROJO                // Los vehículos deben detenerse
} EstadoSemaforo;

// ============================================================================
// ESTRUCTURAS DE DATOS
// ============================================================================

// Estructura que representa un vehículo individual
typedef struct {
    int id;                        // Identificador único del vehículo
    double posicion;               // Posición actual en metros desde el inicio
    int seccion;                   // Sección actual de la calle (0-9)
    double velocidad;              // Velocidad actual en m/s
    double aceleracion;            // Aceleración actual en m/s²
    EstadoVehiculo estado;         // Estado actual del vehículo
    
    // Métricas de tiempo para estadísticas
    double tiempo_entrada;         // Momento en que entró al sistema
    double tiempo_en_estado;       // Tiempo acumulado en el estado actual
    double tiempo_total_frenado;   // Tiempo total frenando
    double tiempo_total_detenido;  // Tiempo total completamente detenido
    double tiempo_lento;           // Tiempo circulando a baja velocidad
    int eventos_reanudacion;       // Número de veces que reanudó movimiento
    double tiempo_llegada_semaforo;// Momento en que llegó al semáforo
    double tiempo_salida;          // Momento en que salió del sistema
} Vehiculo;

// Estructura que representa un evento en la simulación
typedef struct {
    double tiempo;      // Momento en que debe ocurrir el evento
    int tipo;           // Tipo de evento (ENTRADA, SALIDA, ACTUALIZACION_VEHICULO)
    int id_auto;        // ID del auto relacionado con el evento
    Vehiculo* vehiculo; // Puntero al vehículo (para eventos de actualización)
} Evento;

// Nodo para la cola de eventos (lista doblemente enlazada)
typedef struct Nodo {
    Evento evento;              // El evento almacenado
    struct Nodo* siguiente;     // Puntero al siguiente nodo
    struct Nodo* anterior;      // Puntero al nodo anterior
} Nodo;

// Cola de eventos ordenada por tiempo (lista enlazada ordenada)
typedef struct {
    Nodo* inicio;    // Primer evento en la cola
    Nodo* fin;       // Último evento en la cola
    int size;        // Número de eventos en la cola
} ColaEventos;

// Sistema que representa el estado completo de la calle
typedef struct {
    Vehiculo* secciones[NUM_SECCIONES];  // Vehículos por sección (no usado actualmente)
    Vehiculo* vehiculos_activos[10];     // Array de vehículos actualmente en movimiento
    int num_vehiculos_activos;           // Número de vehículos activos
    double tiempo_actual;                // Tiempo actual de la simulación
} SistemaCalle;

// Parámetros físicos de la simulación
typedef struct {
    double velocidad_maxima;        // Velocidad máxima permitida (m/s)
    double aceleracion_maxima;      // Aceleración máxima (m/s²)
    double desaceleracion_maxima;   // Desaceleración máxima (m/s²) - valor negativo
    double desaceleracion_suave;    // Desaceleración suave (m/s²) - valor negativo
    double distancia_seguridad_min; // Distancia mínima de seguridad (m)
    double longitud_vehiculo;       // Longitud promedio de un vehículo (m)
    double tiempo_reaccion;         // Tiempo de reacción del conductor (s)
} ParametrosSimulacion;

// ============================================================================
// VARIABLES GLOBALES
// ============================================================================

SistemaCalle calle;  // Estado global del sistema de tráfico

// Parámetros de simulación: vel_max=8.33m/s (~30km/h), acel=2m/s², decel=-3.5m/s²
ParametrosSimulacion params = {8.33, 2.0, -3.5, -1.5, 3.0, 4.5, 1.5}; // Aumenté distancia mínima a 3.0m

// Variables del semáforo
EstadoSemaforo semaforo_actual = VERDE;
double duracion_verde = 20.0;     // Duración de luz verde en segundos
double duracion_amarillo = 3.0;   // Duración de luz amarilla en segundos
double duracion_rojo = 10.0;      // Duración de luz roja en segundos

// Control del semáforo
typedef struct {
    double ultimo_cambio;    // Último momento de cambio de estado
    EstadoSemaforo estado;   // Estado actual del semáforo
} SemaforoControl;

SemaforoControl semaforo = {0.0, VERDE};

// ============================================================================
// FUNCIONES DE CONTROL DE COLISIONES
// ============================================================================

// Encuentra el vehículo más cercano por delante de la posición dada
Vehiculo* encontrar_vehiculo_adelante(double posicion, Vehiculo* vehiculo_actual) {
    Vehiculo* mas_cercano = NULL;
    double distancia_minima = LONGITUD_TOTAL;
    
    for (int i = 0; i < calle.num_vehiculos_activos; i++) {
        Vehiculo* v = calle.vehiculos_activos[i];
        
        // No comparar consigo mismo
        if (v == vehiculo_actual) continue;
        
        // Solo considerar vehículos que están adelante
        if (v->posicion > posicion) {
            double distancia = v->posicion - posicion;
            if (distancia < distancia_minima) {
                distancia_minima = distancia;
                mas_cercano = v;
            }
        }
    }
    
    return mas_cercano;
}

// Calcula la distancia de seguridad requerida basada en la velocidad
double calcular_distancia_seguridad(double velocidad) {
    // Distancia de seguridad = distancia mínima + tiempo de reacción * velocidad + margen de frenado
    double distancia_reaccion = velocidad * params.tiempo_reaccion;
    double distancia_frenado = (velocidad * velocidad) / (2.0 * fabs(params.desaceleracion_maxima));
    return params.distancia_seguridad_min + distancia_reaccion + distancia_frenado * 0.5;
}

// Verifica si un vehículo puede ocupar una posición sin colisionar
int puede_avanzar_sin_colision(Vehiculo* vehiculo, double nueva_posicion) {
    Vehiculo* adelante = encontrar_vehiculo_adelante(vehiculo->posicion, vehiculo);
    
    if (!adelante) return 1; // No hay vehículo adelante
    
    // Calcular la distancia que habría después del movimiento
    double distancia_resultante = adelante->posicion - nueva_posicion - params.longitud_vehiculo;
    double distancia_requerida = calcular_distancia_seguridad(vehiculo->velocidad);
    
    return distancia_resultante >= distancia_requerida;
}

// Ajusta la velocidad y aceleración para evitar colisiones
void ajustar_por_trafico(Vehiculo* vehiculo, double dt) {
    Vehiculo* adelante = encontrar_vehiculo_adelante(vehiculo->posicion, vehiculo);
    
    if (!adelante) return; // No hay vehículo adelante
    
    double distancia_actual = adelante->posicion - vehiculo->posicion - params.longitud_vehiculo;
    double distancia_requerida = calcular_distancia_seguridad(vehiculo->velocidad);
    
    // Si la distancia es crítica, frenar
    if (distancia_actual <= distancia_requerida) {
        // Determinar qué tan urgente es el frenado
        if (distancia_actual <= params.distancia_seguridad_min) {
            // Frenado de emergencia
            vehiculo->estado = FRENANDO;
            vehiculo->aceleracion = params.desaceleracion_maxima;
            printf("[%.2f] Vehiculo %d: FRENADO EMERGENCIA (dist: %.2fm)\n", 
                   calle.tiempo_actual, vehiculo->id, distancia_actual);
        } else {
            // Ajustar velocidad al vehículo de adelante
            double velocidad_objetivo = adelante->velocidad * 0.9; // Un poco más lento
            if (vehiculo->velocidad > velocidad_objetivo) {
                vehiculo->estado = DESACELERANDO;
                vehiculo->aceleracion = params.desaceleracion_suave;
            }
        }
    }
}

// ============================================================================
// FUNCIONES DE CONTROL DEL SEMÁFORO
// ============================================================================

// Actualiza el estado del semáforo basado en el tiempo actual
void actualizar_semaforo(double reloj) {
    // Calcula el ciclo completo del semáforo
    double ciclo = duracion_verde + duracion_amarillo + duracion_rojo;
    
    // Encuentra la posición dentro del ciclo actual
    double t = fmod(reloj, ciclo);
    
    // Determina el estado según la posición en el ciclo
    if (t < duracion_verde) 
        semaforo.estado = VERDE;
    else if (t < duracion_verde + duracion_amarillo) 
        semaforo.estado = AMARILLO;
    else 
        semaforo.estado = ROJO;
}

// ============================================================================
// FUNCIONES DE UTILIDAD PARA MOSTRAR INFORMACIÓN
// ============================================================================

// Convierte un estado de vehículo a texto legible
const char* estado_str(int estado) {
    switch (estado) {
        case ENTRANDO: return "ENTRANDO";
        case ACELERANDO: return "ACELERANDO";
        case VELOCIDAD_CONSTANTE: return "VEL_CONST";
        case DESACELERANDO: return "DESACELERANDO";
        case FRENANDO: return "FRENANDO";
        case DETENIDO: return "DETENIDO";
        case SALIENDO: return "SALIENDO";
        default: return "???";
    }
}

// Convierte un estado de semáforo a texto legible
const char* color_semaforo(EstadoSemaforo e) {
    switch (e) {
        case VERDE: return "VERDE";
        case AMARILLO: return "AMARILLO";
        case ROJO: return "ROJO";
        default: return "?";
    }
}

// Imprime el estado actual de todos los vehículos en la calle
void imprimir_estado_calle_extendido() {
    printf("\n=== ESTADO t=%.2f | Semaforo: %s ===\n", 
           calle.tiempo_actual, color_semaforo(semaforo.estado));
    
    // Itera sobre todos los vehículos activos y muestra su información
    for (int i = 0; i < calle.num_vehiculos_activos; i++) {
        Vehiculo* v = calle.vehiculos_activos[i];
        Vehiculo* adelante = encontrar_vehiculo_adelante(v->posicion, v);
        double distancia = adelante ? (adelante->posicion - v->posicion - params.longitud_vehiculo) : -1;
        
        printf("ID:%2d Pos=%.1fm Vel=%.2fm/s Acel=%.2f Estado=%s", 
               v->id, v->posicion, v->velocidad, v->aceleracion, estado_str(v->estado));
        
        if (adelante) {
            printf(" [Dist a ID%d: %.1fm]", adelante->id, distancia);
        }
        printf("\n");
    }
    printf("\n");
}

// ============================================================================
// FUNCIONES DE GESTIÓN DE LA COLA DE EVENTOS
// ============================================================================

// Inserta un evento en la cola manteniendo el orden cronológico
void insertar_evento(ColaEventos* cola, Evento evento) {
    // Crear nuevo nodo
    Nodo* nuevo = (Nodo*)malloc(sizeof(Nodo));
    nuevo->evento = evento;
    nuevo->siguiente = nuevo->anterior = NULL;
    
    // Si la cola está vacía, este es el primer elemento
    if (!cola->inicio) {
        cola->inicio = cola->fin = nuevo;
    } 
    // Si el evento es anterior al primer elemento, insertarlo al inicio
    else if (evento.tiempo < cola->inicio->evento.tiempo) {
        nuevo->siguiente = cola->inicio;
        cola->inicio->anterior = nuevo;
        cola->inicio = nuevo;
    } 
    // Buscar la posición correcta en la lista ordenada
    else {
        Nodo* actual = cola->inicio;
        // Avanzar hasta encontrar el lugar correcto
        while (actual->siguiente && actual->siguiente->evento.tiempo < evento.tiempo)
            actual = actual->siguiente;
        
        // Insertar el nuevo nodo después de 'actual'
        nuevo->siguiente = actual->siguiente;
        if (actual->siguiente) 
            actual->siguiente->anterior = nuevo;
        nuevo->anterior = actual;
        actual->siguiente = nuevo;
        
        // Si se insertó al final, actualizar el puntero fin
        if (!nuevo->siguiente) 
            cola->fin = nuevo;
    }
    cola->size++;
}

// Obtiene y remueve el próximo evento de la cola (el más próximo en tiempo)
Evento* obtener_siguiente_evento(ColaEventos* cola) {
    // Si no hay eventos, retornar NULL
    if (!cola->inicio) return NULL;
    
    // Obtener el primer nodo (evento más próximo)
    Nodo* nodo = cola->inicio;
    Evento* evento = (Evento*)malloc(sizeof(Evento));
    *evento = nodo->evento;  // Copiar el evento
    
    // Actualizar los punteros de la cola
    cola->inicio = nodo->siguiente;
    if (cola->inicio) 
        cola->inicio->anterior = NULL;
    else 
        cola->fin = NULL;  // La cola quedó vacía
    
    // Liberar el nodo y decrementar el tamaño
    free(nodo);
    cola->size--;
    return evento;
}

// ============================================================================
// FUNCIONES DE REPORTE Y ESTADÍSTICAS
// ============================================================================

// Imprime la tabla final con estadísticas de todos los vehículos
void imprimir_tabla_final(Vehiculo* vehiculos[], int total) {
    printf("\n==== TABLA FINAL DE RESULTADOS ====");
    printf("\nID | Entrada | Llegada Semaforo | Salida | Detenido (s)\n");
    printf("---|---------|------------------|--------|-------------\n");
    
    for (int i = 0; i < total; i++) {
        if (vehiculos[i]) {  // Verificar que el vehículo existe
            Vehiculo* v = vehiculos[i];
            printf("%2d | %7.2f | %16.2f | %6.2f | %13.2f\n",
                v->id, 
                v->tiempo_entrada, 
                v->tiempo_llegada_semaforo, 
                v->tiempo_salida, 
                v->tiempo_total_detenido);
        }
    }
    printf("\n");
}

// ============================================================================
// FUNCIONES DE GESTIÓN DE VEHÍCULOS
// ============================================================================

// Remueve un vehículo del arreglo de vehículos activos
void remover_vehiculo_activo(Vehiculo* v) {
    for (int i = 0; i < calle.num_vehiculos_activos; i++) {
        if (calle.vehiculos_activos[i] == v) {
            // Mover el último elemento a esta posición para evitar huecos
            calle.vehiculos_activos[i] = calle.vehiculos_activos[calle.num_vehiculos_activos - 1];
            calle.num_vehiculos_activos--;
            break;
        }
    }
}

// ============================================================================
// FUNCIÓN PRINCIPAL DE SIMULACIÓN
// ============================================================================

// Procesa todos los eventos de la simulación en orden cronológico
void procesar_eventos() {
    ColaEventos cola = {0};        // Inicializar cola de eventos vacía
    int id_auto = 1;               // Contador para IDs de vehículos
    int vehiculos_creados = 0;     // Contador de vehículos ya creados
    Vehiculo* resumen[MAX_AUTOS];  // Array para almacenar todos los vehículos
    
    // Inicializar el arreglo de resumen con NULLs
    for (int i = 0; i < MAX_AUTOS; i++) {
        resumen[i] = NULL;
    }
    
    // Crear el primer evento: entrada del primer vehículo en tiempo 0
    Evento primer = {0.0, ENTRADA, id_auto, NULL};
    insertar_evento(&cola, primer);

    // BUCLE PRINCIPAL: continuar mientras haya eventos O vehículos activos
    while (cola.inicio || calle.num_vehiculos_activos > 0) {
        // Obtener el próximo evento en orden cronológico
        Evento* e = obtener_siguiente_evento(&cola);
        
        // Si no hay más eventos pero quedan vehículos activos, terminar
        // (esto evita bucle infinito si algo sale mal)
        if (!e) {
            if (calle.num_vehiculos_activos > 0) {
                printf("ADVERTENCIA: No hay más eventos pero quedan %d vehículos activos\n", 
                       calle.num_vehiculos_activos);
            }
            break;
        }
        
        // Actualizar el tiempo actual del sistema
        calle.tiempo_actual = e->tiempo;
        
        // Actualizar el estado del semáforo para este momento
        actualizar_semaforo(calle.tiempo_actual);

        // ====================================================================
        // PROCESAR EVENTO DE ENTRADA DE VEHÍCULO
        // ====================================================================
        if (e->tipo == ENTRADA) {
            // Verificar que no hay un vehículo muy cerca del inicio
            int puede_entrar = 1;
            for (int i = 0; i < calle.num_vehiculos_activos; i++) {
                Vehiculo* v = calle.vehiculos_activos[i];
                if (v->posicion < params.longitud_vehiculo + params.distancia_seguridad_min) {
                    puede_entrar = 0;
                    break;
                }
            }
            
            // Solo crear vehículo si no hemos alcanzado el límite y puede entrar
            if (vehiculos_creados < MAX_AUTOS && puede_entrar) {
                // Crear un nuevo vehículo
                Vehiculo* v = (Vehiculo*)malloc(sizeof(Vehiculo));
                memset(v, 0, sizeof(Vehiculo));  // Inicializar todos los campos a 0
                
                // Configurar propiedades iniciales del vehículo
                v->id = id_auto;
                v->estado = ENTRANDO;
                v->aceleracion = params.aceleracion_maxima;
                v->tiempo_entrada = e->tiempo;
                
                // Agregar a los arrays de seguimiento
                resumen[vehiculos_creados] = v;
                calle.vehiculos_activos[calle.num_vehiculos_activos++] = v;

                printf("[%.2f] Vehiculo %d entra\n", calle.tiempo_actual, v->id);

                // Programar la primera actualización de este vehículo
                Evento actualizacion = {e->tiempo + PASO_SIMULACION, ACTUALIZACION_VEHICULO, v->id, v};
                insertar_evento(&cola, actualizacion);
                
                vehiculos_creados++;
                id_auto++;

                // Programar la entrada del siguiente vehículo (cada 3 segundos)
                // Solo si no hemos alcanzado el límite
                if (vehiculos_creados < MAX_AUTOS) {
                    Evento siguiente = {e->tiempo + 3.0, ENTRADA, id_auto, NULL};
                    insertar_evento(&cola, siguiente);
                }
            } else if (!puede_entrar) {
                // Si no puede entrar ahora, intentar más tarde
                if (vehiculos_creados < MAX_AUTOS) {
                    Evento reintento = {e->tiempo + 1.0, ENTRADA, id_auto, NULL};
                    insertar_evento(&cola, reintento);
                }
            }
        } 
        // ====================================================================
        // PROCESAR EVENTO DE ACTUALIZACIÓN DE VEHÍCULO
        // ====================================================================
        else if (e->tipo == ACTUALIZACION_VEHICULO) {
            Vehiculo* v = e->vehiculo;
            
            // Verificar que el vehículo existe y no ha salido del sistema
            if (!v || v->estado == SALIENDO) {
                free(e);
                continue;
            }
            
            double prev_vel = v->velocidad;  // Guardar velocidad anterior
            double dt = PASO_SIMULACION;     // Intervalo de tiempo

            // Registrar llegada al semáforo (solo la primera vez)
            if (v->tiempo_llegada_semaforo == 0.0 && v->posicion >= POSICION_SEMAFORO)
                v->tiempo_llegada_semaforo = calle.tiempo_actual;

            // ================================================================
            // LÓGICA DE DECISIÓN DEL COMPORTAMIENTO DEL VEHÍCULO (CON CONTROL DE TRÁFICO)
            // ================================================================
            
            // Primero verificar si ya está detenido
            if (v->velocidad == 0.0) {
                // Si está detenido, verificar si puede arrancar
                if (semaforo.estado == ROJO && v->posicion < POSICION_SEMAFORO) {
                    // Debe permanecer detenido por semáforo rojo
                    v->estado = DETENIDO;
                    v->aceleracion = 0.0;
                    v->tiempo_total_detenido += dt;
                } else {
                    // Verificar si puede arrancar sin colisionar
                    Vehiculo* adelante = encontrar_vehiculo_adelante(v->posicion, v);
                    if (adelante) {
                        double distancia = adelante->posicion - v->posicion - params.longitud_vehiculo;
                        if (distancia > params.distancia_seguridad_min) {
                            // Puede arrancar - comenzar aceleración
                            v->estado = ACELERANDO;
                            v->aceleracion = params.aceleracion_maxima;
                        } else {
                            // Debe esperar
                            v->estado = DETENIDO;
                            v->aceleracion = 0.0;
                            v->tiempo_total_detenido += dt;
                        }
                    } else {
                        // No hay vehículo adelante, puede arrancar
                        v->estado = ACELERANDO;
                        v->aceleracion = params.aceleracion_maxima;
                    }
                }
            }
            // Si está en movimiento, evaluar qué debe hacer
            else {
                // Primero verificar control de tráfico (evitar colisiones)
                ajustar_por_trafico(v, dt);
                
                // Si no fue modificado por control de tráfico, aplicar lógica normal
                if (v->estado != FRENANDO && v->estado != DESACELERANDO) {
                    // Verificar si debe detenerse por semáforo rojo
                    if (semaforo.estado == ROJO && 
                        v->posicion < POSICION_SEMAFORO &&
                        v->posicion + v->velocidad * params.tiempo_reaccion >= POSICION_SEMAFORO - 2.0) {
                        
                        // Calcular si puede detenerse a tiempo antes del semáforo
                        double distancia_al_semaforo = POSICION_SEMAFORO - v->posicion;
                        double distancia_frenado = (v->velocidad * v->velocidad) / (2.0 * fabs(params.desaceleracion_maxima));
                        
                        if (distancia_frenado >= distancia_al_semaforo - 1.0) {
                            // Necesita frenar urgentemente
                            v->estado = FRENANDO;
                            v->aceleracion = params.desaceleracion_maxima;
                            v->tiempo_total_frenado += dt;
                        } else {
                            // Puede desacelerar suavemente
                            v->estado = DESACELERANDO;
                            v->aceleracion = params.desaceleracion_suave;
                        }
                    }
                    // Verificar si debe detenerse por semáforo amarillo (opcional)
                    else if (semaforo.estado == AMARILLO && 
                             v->posicion < POSICION_SEMAFORO &&
                             v->posicion + v->velocidad * params.tiempo_reaccion >= POSICION_SEMAFORO - 5.0) {
                        
                        // En amarillo, desacelerar suavemente si está cerca
                        v->estado = DESACELERANDO;
                        v->aceleracion = params.desaceleracion_suave;
                    }
                    // Si no ha alcanzado la velocidad máxima, acelerar
                    else if (v->velocidad < params.velocidad_maxima - 0.2) {
                        v->estado = ACELERANDO;
                        v->aceleracion = params.aceleracion_maxima;
                    } 
                    // Mantener velocidad constante
                    else {
                        v->estado = VELOCIDAD_CONSTANTE;
                        v->aceleracion = 0.0;
                    }
                }
            }

            // ================================================================
            // ACTUALIZAR ESTADÍSTICAS Y MÉTRICAS
            // ================================================================
            
            if (v->estado == DETENIDO) 
                v->tiempo_total_detenido += dt;
            
            if (v->velocidad < params.velocidad_maxima / 2.0) 
                v->tiempo_lento += dt;

            // ================================================================
            // ACTUALIZAR FÍSICA DEL MOVIMIENTO CON CONTROL DE COLISIONES
            // ================================================================
            
            // Actualizar velocidad usando aceleración
            double nueva_velocidad = v->velocidad + v->aceleracion * dt;
            
            // Aplicar límites físicos
            if (nueva_velocidad < 0.0) 
                nueva_velocidad = 0.0;  // No puede ir hacia atrás
            if (nueva_velocidad > params.velocidad_maxima) 
                nueva_velocidad = params.velocidad_maxima;  // Límite de velocidad

            // Si la velocidad es muy pequeña y está desacelerando, detenerlo completamente
            if (nueva_velocidad < 0.1 && v->aceleracion < 0) {
                nueva_velocidad = 0.0;
            }

            // Calcular nueva posición
            double nueva_posicion = v->posicion + nueva_velocidad * dt;
            
            // Verificar si puede avanzar sin colisionar
            if (puede_avanzar_sin_colision(v, nueva_posicion)) {
                // Actualizar posición y velocidad
                v->velocidad = nueva_velocidad;
                v->posicion = nueva_posicion;
            } else {
                // No puede avanzar, detenerse
                v->velocidad = 0.0;
                v->estado = DETENIDO;
                v->aceleracion = 0.0;
            }
            
            // Si está muy cerca del semáforo y la velocidad es muy baja, detenerse
            if (v->posicion >= POSICION_SEMAFORO - 2.0 && 
                v->posicion < POSICION_SEMAFORO && 
                v->velocidad < 0.5 && 
                semaforo.estado == ROJO) {
                v->velocidad = 0.0;
                v->estado = DETENIDO;
            }
            
            // Contar eventos de reanudación (cuando pasa de detenido a movimiento)
            if (v->velocidad > 0 && prev_vel == 0) 
                v->eventos_reanudacion++;

            // ================================================================
            // VERIFICAR SI EL VEHÍCULO HA COMPLETADO EL RECORRIDO
            // ================================================================
            
            if (v->posicion >= LONGITUD_TOTAL) {
                // El vehículo ha llegado al final
                v->tiempo_salida = calle.tiempo_actual;
                v->estado = SALIENDO;
                
                printf("[%.2f] Vehiculo %d sale. Tiempo total: %.2fs\n",
                    calle.tiempo_actual, v->id,
                    calle.tiempo_actual - v->tiempo_entrada);
                
                // Remover de vehículos activos pero NO liberar memoria todavía
                // (la necesitamos para el reporte final)
                remover_vehiculo_activo(v);
            } else {
                // Programar la siguiente actualización de este vehículo
                Evento sig = {e->tiempo + PASO_SIMULACION, ACTUALIZACION_VEHICULO, v->id, v};
                insertar_evento(&cola, sig);
            }
        }
        
        // ====================================================================
        // MOSTRAR ESTADO PERIÓDICAMENTE (cada 5 segundos de simulación)
        // ====================================================================
        if ((int)(calle.tiempo_actual * 10) % 50 == 0) {
            imprimir_estado_calle_extendido();
        }
        
        // Liberar la memoria del evento procesado
        free(e);
    }
    
    // ========================================================================
    // GENERAR REPORTE FINAL Y LIMPIAR MEMORIA
    // ========================================================================
    
    // Imprimir tabla final con estadísticas de todos los vehículos
    printf("\n=== SIMULACION COMPLETADA ===\n");
    printf("Tiempo total de simulación: %.2f segundos\n", calle.tiempo_actual);
    printf("Vehículos que completaron el recorrido: %d\n", vehiculos_creados);
    imprimir_tabla_final(resumen, vehiculos_creados);
    
    // Liberar memoria de todos los vehículos creados
    for (int i = 0; i < vehiculos_creados; i++) {
        if (resumen[i]) {
            free(resumen[i]);
            resumen[i] = NULL;
        }
    }
}

// ============================================================================
// FUNCIÓN PRINCIPAL
// ============================================================================

int main() {
    // Inicializar generador de números aleatorios
    srand((unsigned int)time(NULL));
    
    printf("=== SIMULACION DE TRAFICO CON SEMAFORO Y CONTROL DE COLISIONES ===\n");
    
    // Ejecutar la simulación completa
    procesar_eventos();
    
    return 0;
}