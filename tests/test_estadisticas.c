#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "config.h"
#include "simulador.h"

static int fallos = 0, total = 0;

#define CHECK(cond) do { total++; if (!(cond)) { fallos++; \
    printf("  FALLO linea %d: %s\n", __LINE__, #cond); } } while (0)

/* Disco de 1 MB con bloques de 4 KB = 256 bloques */
static void armar_config(config_t *cfg, mecanismo_t m)
{
    memset(cfg, 0, sizeof *cfg);
    cfg->disco_mb = 1;
    cfg->bloque_kb = 4;
    cfg->mecanismo = m;
    cfg->archivo_min_kb = 8;
    cfg->archivo_max_kb = 64;
    cfg->semilla = 99;
    config_validar(cfg);
}

static int casi_igual(double a, double b)
{
    double d = a - b;
    if (d < 0)
        d = -d;
    return d < 0.01;
}

static void test_fragmentacion(void)
{
    config_t cfg;
    simulador_t sim;
    printf("test_fragmentacion\n");

    armar_config(&cfg, ASIG_CONTIGUA);
    CHECK(simulador_iniciar(&sim, &cfg) == 0);

    /* disco vacio: todo junto, 0% */
    CHECK(casi_igual(simulador_fragmentacion(&sim), 0.0));

    /* ocupar los bloques 5 y 10: quedan 3 huecos (5, 4 y 245 bloques) */
    disco_ocupar(sim.disco, 5, 1);
    disco_ocupar(sim.disco, 10, 1);
    CHECK(disco_cantidad_huecos(sim.disco) == 3);
    CHECK(disco_mayor_libre_contiguo(sim.disco) == 245);
    /* (254 - 245) / 254 * 100 = 3.543 */
    CHECK(casi_igual(simulador_fragmentacion(&sim), 3.543));

    /* el registro guarda maximo y promedio */
    simulador_registrar_fragmentacion(&sim);
    CHECK(sim.frag_medidas == 1);
    CHECK(casi_igual(sim.frag_maxima, 3.543));
    simulador_imprimir_estado(&sim);
    CHECK(sim.frag_medidas == 2);

    simulador_destruir(&sim);
}

static void test_disco_lleno_y_estadisticas(void)
{
    config_t cfg;
    simulador_t sim;
    uint32_t i;
    printf("test_disco_lleno_y_estadisticas\n");

    armar_config(&cfg, ASIG_ENLAZADA);
    CHECK(simulador_iniciar(&sim, &cfg) == 0);

    /* disco completamente lleno: libres = 0, no debe dividir por cero */
    for (i = 0; i < sim.disco->total_bloques; i++)
        disco_ocupar(sim.disco, i, 1);
    CHECK(casi_igual(simulador_fragmentacion(&sim), 0.0));

    /* estadisticas sin busquedas: no debe dividir por cero */
    simulador_imprimir_estadisticas(&sim);
    for (i = 0; i < sim.disco->total_bloques; i++)
        disco_liberar(sim.disco, i);

    /* con busquedas: promedio = total / cantidad */
    for (i = 0; i < 4; i++)
        simulador_crear_archivo(&sim);
    for (i = 0; i < 6; i++)
        simulador_acceder_aleatorio(&sim);
    CHECK(sim.n_busquedas == 6);
    simulador_imprimir_estadisticas(&sim);

    simulador_destruir(&sim);
}

int main(void)
{
    test_fragmentacion();
    test_disco_lleno_y_estadisticas();
    printf("\n%d comprobaciones, %d fallos\n", total, fallos);
    return fallos == 0 ? 0 : 1;
}
