#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "config.h"
#include "simulador.h"

static int fallos = 0, total = 0;

#define CHECK(cond) do { total++; if (!(cond)) { fallos++; \
    printf("  FALLO linea %d: %s\n", __LINE__, #cond); } } while (0)

/* Disco de 1 MB con bloques de 4 KB y archivos de 8 a 64 KB */
static void armar_config(config_t *cfg, mecanismo_t m)
{
    memset(cfg, 0, sizeof *cfg);
    cfg->disco_mb = 1;
    cfg->bloque_kb = 4;
    cfg->mecanismo = m;
    cfg->archivo_min_kb = 8;
    cfg->archivo_max_kb = 64;
    cfg->semilla = 1234;
    config_validar(cfg);
}

static void probar(mecanismo_t m)
{
    config_t cfg;
    simulador_t sim;
    int i;

    printf("-- mecanismo: %s\n", config_nombre_mecanismo(m));
    armar_config(&cfg, m);
    CHECK(simulador_iniciar(&sim, &cfg) == 0);

    /* sin archivos no se puede eliminar ni acceder */
    CHECK(simulador_eliminar_aleatorio(&sim) == -1);
    CHECK(simulador_acceder_aleatorio(&sim) == -1);

    for (i = 0; i < 5; i++)
        CHECK(simulador_crear_archivo(&sim) == ASIG_RES_OK);
    CHECK(sim.n_archivos == 5 && sim.creados == 5);

    for (i = 0; i < 3; i++)
        CHECK(simulador_acceder_aleatorio(&sim) == 0);
    CHECK(sim.n_busquedas == 3 && sim.total_accesos >= 3);

    CHECK(simulador_eliminar_aleatorio(&sim) == 0);
    CHECK(sim.n_archivos == 4 && sim.eliminados == 1);
    CHECK(disco_verificar(sim.disco) == 0);

    /* llenar el disco hasta que falle: tiene que fallar algun dia */
    for (i = 0; i < 1000; i++) {
        if (simulador_crear_archivo(&sim) != ASIG_RES_OK)
            break;
    }
    CHECK(i < 1000);
    CHECK(disco_verificar(sim.disco) == 0);

    simulador_destruir(&sim);
}

int main(void)
{
    probar(ASIG_CONTIGUA);
    probar(ASIG_ENLAZADA);
    probar(ASIG_INDEXADA);
    printf("\n%d comprobaciones, %d fallos\n", total, fallos);
    return fallos == 0 ? 0 : 1;
}
