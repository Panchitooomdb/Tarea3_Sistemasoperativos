#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "config.h"

static void uso(const char *prog)
{
    fprintf(stderr, "Uso: %s [--semilla N]\n", prog);
}

int main(int argc, char **argv)
{
    config_t cfg;
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
    printf("[Etapa 1] Configuracion lista. Aun no hay disco ni simulacion.\n");
    return EXIT_SUCCESS;
}
