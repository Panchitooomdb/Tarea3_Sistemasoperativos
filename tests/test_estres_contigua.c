/* Prueba de estres determinista: miles de operaciones aleatorias sobre el
 * asignador contiguo, verificando despues de CADA operacion que el disco sea
 * consistente y que el resultado coincida con un calculo independiente. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "archivo.h"
#include "asignacion.h"
#include "contigua.h"
#include "disco.h"

#define TOTAL      200u
#define BLOQUE     1024u
#define MAX_ARCH   TOTAL
#define OPERACIONES 20000

static int fallos = 0, total = 0;

#define CHECK(cond) do { total++; if (!(cond)) { fallos++; \
    if (fallos < 20) printf("  FALLO linea %d: %s\n", __LINE__, #cond); } } while (0)

/* Fuerza bruta: primer inicio con n libres consecutivos, o -1. */
static long primer_hueco(const disco_t *d, uint32_t n)
{
    uint32_t i, j;
    for (i = 0; i + n <= d->total_bloques; i++) {
        for (j = 0; j < n; j++)
            if (!disco_esta_libre(d, i + j))
                break;
        if (j == n)
            return (long)i;
    }
    return -1;
}

/* El mapa debe coincidir EXACTAMENTE con lo que declaran los archivos. */
static void verificar_global(const disco_t *d, const archivo_t *f, const int *activo)
{
    uint32_t i, j, esperados = 0;
    static uint32_t cuenta[TOTAL];

    memset(cuenta, 0, sizeof cuenta);
    for (i = 0; i < MAX_ARCH; i++) {
        if (!activo[i])
            continue;
        for (j = 0; j < f[i].largo; j++) {
            CHECK(f[i].inicio + j < d->total_bloques);
            CHECK(disco_dueno(d, f[i].inicio + j) == f[i].id);
            cuenta[f[i].inicio + j]++;
        }
        esperados += f[i].largo;
    }
    for (i = 0; i < d->total_bloques; i++)
        CHECK(cuenta[i] <= 1);                          /* sin solapes */
    CHECK(disco_ocupados(d) == esperados);              /* sin bloques huerfanos */
    CHECK(disco_verificar(d) == 0);
}

int main(void)
{
    disco_t *d = disco_crear(TOTAL);
    archivo_t f[MAX_ARCH];
    int activo[MAX_ARCH];
    uint32_t sig_id = 1, op;
    unsigned semilla;
    unsigned n_ok = 0, n_frag = 0, n_sin = 0, n_lib = 0;

    for (semilla = 1; semilla <= 5; semilla++) {
        srand(semilla);
        memset(f, 0, sizeof f);
        memset(activo, 0, sizeof activo);
        sig_id = 1;

        for (op = 0; op < OPERACIONES; op++) {
            int r = rand() % 100;

            if (r < 55) {                               /* crear */
                uint32_t n = 1 + (uint32_t)(rand() % 30);
                uint64_t tam = (uint64_t)(n - 1) * BLOQUE + 1 + (uint64_t)(rand() % BLOQUE);
                long esperado = primer_hueco(d, n);
                uint32_t libres = disco_libres(d);
                archivo_t tmp;
                asig_resultado_t res;
                int slot = -1, k;

                for (k = 0; k < (int)MAX_ARCH; k++)
                    if (!activo[k]) { slot = k; break; }
                if (slot < 0)
                    continue;

                memset(&tmp, 0, sizeof tmp);
                res = contigua_asignar(d, &tmp, sig_id, tam, BLOQUE);

                if (esperado >= 0) {
                    CHECK(res == ASIG_RES_OK);
                    CHECK(tmp.inicio == (uint32_t)esperado);   /* es first-fit */
                    CHECK(tmp.largo == n && tmp.bloques_total == n);
                    f[slot] = tmp; activo[slot] = 1; sig_id++; n_ok++;
                } else if (libres >= n) {
                    CHECK(res == ASIG_RES_ERR_FRAGMENTACION);
                    CHECK(disco_libres(d) == libres);          /* sin cambios */
                    n_frag++;
                } else {
                    CHECK(res == ASIG_RES_ERR_SIN_ESPACIO);
                    CHECK(disco_libres(d) == libres);
                    n_sin++;
                }
            } else {                                    /* eliminar uno al azar */
                int k, cand[MAX_ARCH], nc = 0;
                for (k = 0; k < (int)MAX_ARCH; k++)
                    if (activo[k]) cand[nc++] = k;
                if (nc == 0)
                    continue;
                k = cand[rand() % nc];
                CHECK(contigua_liberar(d, &f[k]) == 0);
                activo[k] = 0; n_lib++;
            }
            verificar_global(d, f, activo);
        }

        /* vaciar todo: el disco debe quedar identico al inicial */
        {
            int k;
            for (k = 0; k < (int)MAX_ARCH; k++)
                if (activo[k]) { CHECK(contigua_liberar(d, &f[k]) == 0); activo[k] = 0; }
        }
        CHECK(disco_libres(d) == TOTAL);
        CHECK(disco_mayor_libre_contiguo(d) == TOTAL);
        CHECK(disco_cantidad_huecos(d) == 1);
    }

    printf("operaciones: %u creaciones ok, %u fragmentacion, %u sin espacio, %u eliminaciones\n",
           n_ok, n_frag, n_sin, n_lib);
    CHECK(n_frag > 0 && n_sin > 0 && n_ok > 0);          /* se ejercitaron los 3 caminos */
    disco_destruir(d);
    printf("\n%d comprobaciones, %d fallos\n", total, fallos);
    return fallos == 0 ? 0 : 1;
}
