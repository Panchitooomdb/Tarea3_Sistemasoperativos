#include <inttypes.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "config.h"
#include "simulador.h"

#define SEG_CREAR     2   /* cada cuantos segundos se crea un archivo */
#define SEG_INICIO    30  /* desde que segundo empiezan a eliminar y acceder */
#define SEG_PERIODICO 5   /* cada cuantos segundos se elimina y se accede */

/* Se pone en 1 cuando el usuario presiona Ctrl+C (para terminar con estadisticas) */
static volatile sig_atomic_t interrumpido = 0;

static void al_presionar_ctrl_c(int senal)
{
    (void)senal;
    interrumpido = 1;
}

static void uso(const char *prog)
{
    fprintf(stderr, "Uso: %s [--semilla N] [--rapido]\n", prog);
    fprintf(stderr, "  --semilla N  fija la semilla aleatoria (para repetir una simulacion)\n");
    fprintf(stderr, "  --rapido     no espera 1 segundo entre pasos (solo para pruebas)\n");
}

int main(int argc, char **argv)
{
    config_t cfg;
    simulador_t sim;
    asig_resultado_t res;
    uint64_t semilla_arg;
    int rapido = 0;
    int segundo = 0;
    int hubo_evento;
    int i;

    memset(&cfg, 0, sizeof cfg);
    cfg.semilla = (unsigned)time(NULL);

    /* Argumentos opcionales de la linea de comandos */
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--rapido") == 0) {
            rapido = 1;
        } else if (strcmp(argv[i], "--semilla") == 0 && i + 1 < argc) {
            if (config_parsear_u64(argv[i + 1], &semilla_arg) != 0 || semilla_arg > 0xFFFFFFFFu) {
                fprintf(stderr, "Semilla invalida.\n");
                return EXIT_FAILURE;
            }
            cfg.semilla = (unsigned)semilla_arg;
            i++;
        } else {
            uso(argv[0]);
            return EXIT_FAILURE;
        }
    }

    printf("Simulador de asignacion de archivos - Tarea 3 SO\n\n");

    if (config_leer(&cfg, stdin, stdout) != 0) {
        fprintf(stderr, "\nEntrada interrumpida: no se pudo completar la configuracion.\n");
        return EXIT_FAILURE;
    }

    config_imprimir(&cfg, stdout);

    if (simulador_iniciar(&sim, &cfg) != 0) {
        fprintf(stderr, "No se pudo crear el disco virtual (memoria insuficiente).\n");
        return EXIT_FAILURE;
    }

    signal(SIGINT, al_presionar_ctrl_c);

    printf("Disco virtual creado: %" PRIu32 " bloques\n", sim.disco->total_bloques);
    printf("Comienza la simulacion (Ctrl+C para terminarla antes)\n");
    simulador_imprimir_estado(&sim);

    /* Cada vuelta del ciclo es un segundo de la simulacion */
    while (!interrumpido) {
        if (!rapido)
            sleep(1);
        if (interrumpido)
            break;
        segundo++;
        printf("\n--- segundo %d ---\n", segundo);
        hubo_evento = 0;

        /* Cada 2 segundos se crea un archivo */
        if (segundo % SEG_CREAR == 0) {
            res = simulador_crear_archivo(&sim);
            if (res != ASIG_RES_OK) {
                printf("\nNo se pudo crear el archivo: %s\n", asig_mensaje(res));
                printf("La simulacion termina.\n");
                break;
            }
            hubo_evento = 1;
        }

        /* Desde los 30 segundos, cada 5 segundos se elimina un archivo y se accede a otro */
        if (segundo >= SEG_INICIO && segundo % SEG_PERIODICO == 0) {
            simulador_eliminar_aleatorio(&sim);
            simulador_acceder_aleatorio(&sim);
            hubo_evento = 1;
        }

        if (hubo_evento)
            simulador_imprimir_estado(&sim);
    }

    if (interrumpido)
        printf("\nSimulacion interrumpida por el usuario.\n");

    printf("\nEstado final del disco:\n");
    simulador_imprimir_estado(&sim);
    disco_imprimir_mapa(sim.disco, stdout);
    simulador_imprimir_estadisticas(&sim);

    simulador_destruir(&sim);
    return EXIT_SUCCESS;
}
