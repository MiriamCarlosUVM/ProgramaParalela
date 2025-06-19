#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <string.h>

// CONSTANTES DEL SISTEMA
#define MAX_EVENTOS 1000        // Máximo número de eventos en la lista
#define MAX_COLA 100           // Máximo número de pacientes en cola
#define MAX_PACIENTES 500      // Máximo número de pacientes para estadísticas
#define NUM_DOCTORES 2         // Número de doctores disponibles
#define TIEMPO_SIMULACION 480.0 // 8 horas en minutos

// TIPOS DE EVENTOS
typedef enum {
    LLEGADA,           // Llegada de un paciente
    FIN_ATENCION      // Fin de atención de un paciente
} TipoEvento;

// ESTRUCTURA PACIENTE

typedef struct {
    int id;                    // Identificador único del paciente
    double tiempo_llegada;     // Momento en que llega al sistema
    double tiempo_inicio_atencion;  // Cuándo comienza a ser atendido
    double tiempo_fin_atencion;     // Cuándo termina su atención
} Paciente;

// ESTRUCTURA EVENTO

typedef struct {
    TipoEvento tipo;          // Tipo de evento (LLEGADA o FIN_ATENCION)
    double tiempo;            // Momento en que ocurre el evento
    Paciente *paciente;       // Puntero al paciente asociado (si aplica)
} Evento;

// ESTRUCTURA DEL SIMULADOR

typedef struct {
    // Estado del sistema
    double tiempo_actual;              // Reloj de simulación
    int doctores_libres;              // Número de doctores disponibles
    
    // Cola de pacientes esperando
    Paciente *cola_pacientes[MAX_COLA]; // Array de punteros a pacientes en cola
    int frente_cola;                   // Índice del frente de la cola
    int final_cola;                    // Índice del final de la cola
    int tamaño_cola;                   // Número actual de pacientes en cola
    
    // Lista de eventos futuros
    Evento lista_eventos[MAX_EVENTOS];  // Array de eventos ordenados por tiempo
    int num_eventos;                    // Número actual de eventos en la lista
    
    // Pacientes y estadísticas
    Paciente pacientes[MAX_PACIENTES];  // Array de todos los pacientes creados
    int contador_pacientes;             // Contador para asignar IDs únicos
    int pacientes_atendidos;           // Número de pacientes que completaron atención
    
    // Métricas del sistema
    int longitud_maxima_cola;          // Longitud máxima alcanzada por la cola
    double tiempo_ocupacion_doctores;   // Tiempo total que los doctores estuvieron ocupados
    double suma_tiempos_sistema;       // Suma de todos los tiempos en sistema
    double suma_tiempos_espera;        // Suma de todos los tiempos de espera
} Simulador;

// VARIABLES GLOBALES
Simulador sim;  // Instancia única del simulador

/*
 * Genera un número aleatorio entre 0 y 1
 */
double uniforme() {
    return (double)rand() / RAND_MAX;
}

/*
 * Genera tiempo entre llegadas usando distribución exponencial.
 * Media = 10 minutos (lambda = 0.1)
 */
double tiempo_entre_llegadas() {
    double u = uniforme();
    return -10.0 * log(u);  // -1/lambda * ln(u), donde lambda = 0.1
}

/*
 * Genera tiempo de atención usando distribución triangular.
 * Parámetros: mínimo=5, máximo=20, moda=10 minutos.
 */
double tiempo_atencion() {
    double a = 5.0;   // mínimo
    double b = 20.0;  // máximo
    double c = 10.0;  // moda
    
    double u = uniforme();
    double fc = (c - a) / (b - a);  // Punto de corte
    
    if (u <= fc) {
        return a + sqrt(u * (b - a) * (c - a));
    } else {
        return b - sqrt((1 - u) * (b - a) * (b - c));
    }
}

// FUNCIONES DE MANEJO DE COLA

void inicializar_cola() {
    sim.frente_cola = 0;
    sim.final_cola = 0;
    sim.tamaño_cola = 0;
}

/*
 * Añade un paciente al final de la cola (FIFO).
 */
void encolar_paciente(Paciente *paciente) {
    if (sim.tamaño_cola < MAX_COLA) {
        sim.cola_pacientes[sim.final_cola] = paciente;
        sim.final_cola = (sim.final_cola + 1) % MAX_COLA;  // Circular
        sim.tamaño_cola++;
        
        // Actualizar estadística de longitud máxima
        if (sim.tamaño_cola > sim.longitud_maxima_cola) {
            sim.longitud_maxima_cola = sim.tamaño_cola;
        }
    }
}

/*
 * Saca un paciente del frente de la cola (FIFO - First In, First Out).
 */
Paciente* desencolar_paciente() {
    if (sim.tamaño_cola > 0) {
        Paciente *paciente = sim.cola_pacientes[sim.frente_cola];
        sim.frente_cola = (sim.frente_cola + 1) % MAX_COLA;  // Circular
        sim.tamaño_cola--;
        return paciente;
    }
    return NULL;
}

// FUNCIONES DE MANEJO DE EVENTOS
/*
 * Inserta un evento en la lista manteniendo el orden cronológico.
 * Usa inserción ordenada para que los eventos se procesen en orden.
 */
void insertar_evento(TipoEvento tipo, double tiempo, Paciente *paciente) {
    if (sim.num_eventos >= MAX_EVENTOS) {
        printf("ERROR: Lista de eventos llena!\n");
        return;
    }
    
    // Buscar posición correcta para insertar (mantener orden por tiempo)
    int pos = sim.num_eventos;
    for (int i = 0; i < sim.num_eventos; i++) {
        if (tiempo < sim.lista_eventos[i].tiempo) {
            pos = i;
            break;
        }
    }
    
    // Desplazar eventos hacia la derecha para hacer espacio
    for (int i = sim.num_eventos; i > pos; i--) {
        sim.lista_eventos[i] = sim.lista_eventos[i-1];
    }
    
    // Insertar el nuevo evento
    sim.lista_eventos[pos].tipo = tipo;
    sim.lista_eventos[pos].tiempo = tiempo;
    sim.lista_eventos[pos].paciente = paciente;
    sim.num_eventos++;
}

/*
 * Saca y retorna el próximo evento (el más temprano).
 * Los eventos se procesan en orden cronológico.
 */
Evento obtener_proximo_evento() {
    Evento evento = sim.lista_eventos[0];
    
    // Desplazar todos los eventos hacia la izquierda
    for (int i = 0; i < sim.num_eventos - 1; i++) {
        sim.lista_eventos[i] = sim.lista_eventos[i + 1];
    }
    sim.num_eventos--;
    
    return evento;
}

// FUNCIONES DE PROCESAMIENTO DE EVENTOS
/*
 * Programa la llegada del próximo paciente.
 * Solo programa si está dentro del tiempo de simulación.
 */
void programar_proxima_llegada() {
    double tiempo_llegada = sim.tiempo_actual + tiempo_entre_llegadas();
    
    if (tiempo_llegada < TIEMPO_SIMULACION) {
        // Crear nuevo paciente
        sim.contador_pacientes++;
        Paciente *nuevo_paciente = &sim.pacientes[sim.contador_pacientes - 1];
        nuevo_paciente->id = sim.contador_pacientes;
        nuevo_paciente->tiempo_llegada = tiempo_llegada;
        nuevo_paciente->tiempo_inicio_atencion = 0;
        nuevo_paciente->tiempo_fin_atencion = 0;
        
        // Programar evento de llegada
        insertar_evento(LLEGADA, tiempo_llegada, nuevo_paciente);
    }
}

/*
 * Inicia la atención de un paciente.
 * Asigna un doctor y programa el fin de la atención.
 */
void iniciar_atencion(Paciente *paciente) {
    // Registrar inicio de atención
    paciente->tiempo_inicio_atencion = sim.tiempo_actual;
    sim.doctores_libres--;  // Ocupar un doctor
    
    // Calcular duración de la atención
    double duracion = tiempo_atencion();
    paciente->tiempo_fin_atencion = sim.tiempo_actual + duracion;
    
    // Programar evento de fin de atención
    insertar_evento(FIN_ATENCION, paciente->tiempo_fin_atencion, paciente);
    
    printf("  -> Paciente %d inicia atencion (durara %.1f min)\n", 
           paciente->id, duracion);
}

/*
 * Procesa la llegada de un paciente al sistema.
 * Decide si puede ser atendido inmediatamente o debe esperar.
 */
void procesar_llegada(Paciente *paciente) {
    printf("Tiempo %.1f: Llega paciente %d\n", sim.tiempo_actual, paciente->id);
    
    // Programar la siguiente llegada
    programar_proxima_llegada();
    
    // Verificar si hay doctores disponibles
    if (sim.doctores_libres > 0) {
        // Atender inmediatamente
        iniciar_atencion(paciente);
    } else {
        // Añadir a la cola de espera
        encolar_paciente(paciente);
        printf("  -> Paciente %d entra en cola. Cola actual: %d\n", 
               paciente->id, sim.tamaño_cola);
    }
}

/*
 * Procesa el fin de atención de un paciente.
 * Libera el doctor y atiende al siguiente si hay cola.
 */
void procesar_fin_atencion(Paciente *paciente) {
    printf("Tiempo %.1f: Paciente %d termina atencion\n", 
           sim.tiempo_actual, paciente->id);
    
    // Liberar doctor
    sim.doctores_libres++;
    
    // Actualizar estadísticas
    sim.pacientes_atendidos++;
    double tiempo_sistema = paciente->tiempo_fin_atencion - paciente->tiempo_llegada;
    double tiempo_espera = paciente->tiempo_inicio_atencion - paciente->tiempo_llegada;
    double duracion_atencion = paciente->tiempo_fin_atencion - paciente->tiempo_inicio_atencion;
    
    sim.suma_tiempos_sistema += tiempo_sistema;
    sim.suma_tiempos_espera += tiempo_espera;
    sim.tiempo_ocupacion_doctores += duracion_atencion;
    
    // Si hay pacientes esperando, atender al siguiente
    if (sim.tamaño_cola > 0) {
        Paciente *siguiente = desencolar_paciente();
        printf("  -> Sacando paciente %d de la cola\n", siguiente->id);
        iniciar_atencion(siguiente);
    }
}

// FUNCIÓN PRINCIPAL DE SIMULACIÓN
/*
 * Ejecuta la simulación procesando eventos en orden cronológico.
 * Bucle principal: mientras haya eventos y estén dentro del tiempo límite.
 */
void ejecutar_simulacion() {
    printf("=== INICIANDO SIMULACION DE CLINICA ===\n");
    printf("Doctores disponibles: %d\n", NUM_DOCTORES);
    printf("Tiempo de simulacion: %.0f minutos\n\n", TIEMPO_SIMULACION);
    
    // Procesar eventos mientras existan y estén dentro del tiempo
    while (sim.num_eventos > 0 && 
           sim.lista_eventos[0].tiempo <= TIEMPO_SIMULACION) {
        
        // Obtener el próximo evento (el más temprano)
        Evento evento_actual = obtener_proximo_evento();
        
        // Actualizar el reloj de simulación
        sim.tiempo_actual = evento_actual.tiempo;
        
        // Procesar el evento según su tipo
        switch (evento_actual.tipo) {
            case LLEGADA:
                procesar_llegada(evento_actual.paciente);
                break;
            case FIN_ATENCION:
                procesar_fin_atencion(evento_actual.paciente);
                break;
        }
    }
    
    printf("\n=== SIMULACION TERMINADA (tiempo: %.1f min) ===\n", sim.tiempo_actual);
}

// FUNCIÓN DE INICIALIZACIÓN
/*
 * Inicializa todas las estructuras del simulador.
 */
void inicializar_simulador() {
    // Estado inicial del sistema
    sim.tiempo_actual = 0.0;
    sim.doctores_libres = NUM_DOCTORES;
    sim.num_eventos = 0;
    sim.contador_pacientes = 0;
    sim.pacientes_atendidos = 0;
    
    // Estadísticas iniciales
    sim.longitud_maxima_cola = 0;
    sim.tiempo_ocupacion_doctores = 0.0;
    sim.suma_tiempos_sistema = 0.0;
    sim.suma_tiempos_espera = 0.0;
    
    // Inicializar cola
    inicializar_cola();
    
    // Programar la primera llegada
    programar_proxima_llegada();
}

// FUNCIÓN DE REPORTE
/*
 * Genera un reporte con las estadísticas de la simulación.
 */
void generar_reporte() {
    if (sim.pacientes_atendidos == 0) {
        printf("No hay datos suficientes para generar el reporte.\n");
        return;
    }
    
    // Calcular métricas promedio
    double tiempo_promedio_sistema = sim.suma_tiempos_sistema / sim.pacientes_atendidos;
    double tiempo_promedio_espera = sim.suma_tiempos_espera / sim.pacientes_atendidos;
    
    // Porcentaje de ocupación de doctores
    double tiempo_total_posible = TIEMPO_SIMULACION * NUM_DOCTORES;
    double porcentaje_ocupacion = (sim.tiempo_ocupacion_doctores / tiempo_total_posible) * 100.0;
    
    // Imprimir reporte
    printf("\n");
    printf("==================================================\n");
    printf("           REPORTE DE SIMULACION\n");
    printf("==================================================\n");
    printf("Pacientes atendidos: %d\n", sim.pacientes_atendidos);
    printf("Pacientes en cola al final: %d\n", sim.tamaño_cola);
    printf("Tiempo promedio en sistema: %.2f minutos\n", tiempo_promedio_sistema);
    printf("Tiempo promedio de espera: %.2f minutos\n", tiempo_promedio_espera);
    printf("Longitud maxima de cola: %d pacientes\n", sim.longitud_maxima_cola);
    printf("Ocupacion de doctores: %.1f%%\n", porcentaje_ocupacion);
    printf("==================================================\n");
}

// FUNCIÓN MAIN
int main() {
    // Inicializar generador de números aleatorios
    srand(time(NULL));  // Usar tiempo actual como semilla
    
    // Ejecutar simulación
    inicializar_simulador();
    ejecutar_simulacion();
    generar_reporte();
    
    return 0;
}