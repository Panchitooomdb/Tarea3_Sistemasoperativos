#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "config.h"
#include "disco.h"

static void uso(const char *prog)
{
    fprintf(stderr, "Uso: %s [--semilla N]\n", prog);
}

int main(int argc, char **argv)
{
    config_t cfg;
    disco_t *disco;
    uint64_t semilla_arg;

    memset(&cfg, 0, sizeof cfg);
    cfg.semilla = (unsigned)time(NULL);

    if (argc == 3 && strcmp(argv[1], "--semilla") == 0) {
        if (config_parsear_u64(argv[2], &semilla_arg) != 0 || semilla_arg > 0xFFFFFFFFu) {
            fprintf(stderr, "Semilla invalida.\n");
            return EXIT_FAILURE;
        }
        cfg.semilla = (unsigned)semilla_arg;
    } else if (argc != 1) {
        uso(argv[0]);
        return EXIT_FAILURE;
    }

    printf("Simulador de asignacion de archivos - Tarea 3 SO\n\n");

    if (config_leer(&cfg, stdin, stdout) != 0) {
        fprintf(stderr, "\nEntrada interrumpida: no se pudo completar la configuracion.\n");
        return EXIT_FAILURE;
    }

    config_imprimir(&cfg, stdout);

    /* total_bloques <= CFG_TOTAL_BLOQUES_MAX, cabe de sobra en uint32_t */
    disco = disco_crear((uint32_t)cfg.total_bloques);
    if (disco == NULL) {
        fprintf(stderr, "No se pudo crear el disco virtual (memoria insuficiente).\n");
        return EXIT_FAILURE;
    }

    printf("Disco virtual creado: %" PRIu32 " bloques (%" PRIu32 " libres, %" PRIu32 " ocupados)\n\n",
           disco->total_bloques, disco_libres(disco), disco_ocupados(disco));
    disco_imprimir_mapa(disco, stdout);

    printf("\n[Etapa 2] Disco listo. Aun no hay archivos ni simulacion.\n");

    disco_destruir(disco);
    return EXIT_SUCCESS;
}
