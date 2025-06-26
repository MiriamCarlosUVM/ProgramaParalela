#include <stdio.h>
#include <stdlib.h>

#define ENTRADA 0
#define SALIDA 1
#define MAX_EVENTOS 100

typedef struct {
    double tiempo;   // Momento en el que ocurre el evento en segundos
    int tipo;        // entrada(0) o salida(1)
    int id_auto;     
} Evento;

typedef struct {
    Evento eventos[MAX_EVENTOS];  //areglo para almacenar eventos
    int cantidad_eventos;         // Numero actual de eventos en cola
} ColaEventos;

ColaEventos cola;

// Inserta ordenadamente por tiempo de < a > (FIFO)
void insertar_evento(Evento e) {
    int i = cola.cantidad_eventos - 1;  // apunta al ultimo evento existente
    while (i >= 0 && cola.eventos[i].tiempo > e.tiempo) {
        cola.eventos[i + 1] = cola.eventos[i];
        i--;
    }
    cola.eventos[i + 1] = e;
    cola.cantidad_eventos++;
}

Evento obtener_siguiente_evento() {
    return cola.eventos[--cola.cantidad_eventos];
}

double generar_tiempo_entre_llegadas() {
    return 2.0 + rand() % 3; // entre 2 y 4 segundos
}

double generar_tiempo_de_salida() {
    return 5.0 + rand() % 2; // entre 5 y 6 segundos
}

int main() {
    double reloj = 0.0;
    int id = 1;
    cola.cantidad_eventos = 0;

    // Primer evento de entrada
    Evento e;
    e.tiempo = 0.0;
    e.tipo = ENTRADA;
    e.id_auto = id++;
    insertar_evento(e);

    while (cola.cantidad_eventos > 0) {
        Evento actual = obtener_siguiente_evento();
        reloj = actual.tiempo;

        if (actual.tipo == ENTRADA) {
            printf("Auto %d entra en t=%.2f\n", actual.id_auto, reloj);

            // Agregar evento de salida
            Evento salida;
            salida.tiempo = reloj + generar_tiempo_de_salida();
            salida.tipo = SALIDA;
            salida.id_auto = actual.id_auto;
            insertar_evento(salida);

            // Generar otra entrada si hay espacio
            if (id <= 10) {
                Evento nueva_entrada;
                nueva_entrada.tiempo = reloj + generar_tiempo_entre_llegadas();
                nueva_entrada.tipo = ENTRADA;
                nueva_entrada.id_auto = id++;
                insertar_evento(nueva_entrada);
            }

        } else if (actual.tipo == SALIDA) {
            printf("Auto %d sale en t=%.2f\n", actual.id_auto, reloj);
        }
    }

    return 0;
}