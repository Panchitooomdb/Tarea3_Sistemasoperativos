/* Estres determinista de la asignacion indexada contra un modelo independiente. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "archivo.h"
#include "asignacion.h"
#include "disco.h"
#include "indexada.h"

#define TOTAL       400u
#define BLOQUE      1024u
#define REFS        255u               /* BLOQUE / 4 - 1 */
#define MAX_ARCH    TOTAL
#define OPERACIONES 6000

static int fallos = 0, total = 0;

#define CHECK(cond) do { total++; if (!(cond)) { fallos++; \
    if (fallos < 20) printf("  FALLO linea %d: %s\n", __LINE__, #cond); } } while (0)

static void verificar_global(const disco_t *d, const archivo_t *f, const int *activo)
{
    static uint32_t cuenta[TOTAL];
    uint32_t i, k, cur, esperados = 0;

    memset(cuenta, 0, sizeof cuenta);
    for (i = 0; i < MAX_ARCH; i++) {
        if (!activo[i])
            continue;
        cur = f[i].primero;                         /* cadena de indices, recorrida aparte */
        for (k = 0; k < f[i].n_indices; k++) {
            CHECK(cur < d->total_bloques);
            if (cur >= d->total_bloques)
                break;
            CHECK(disco_dueno(d, cur) == f[i].id);
            cuenta[cur]++;
            cur = disco_siguiente(d, cur);
        }
        CHECK(cur == DISCO_FIN);
        for (k = 0; k < f[i].bloques_datos; k++) {  /* bloques de datos */
            uint32_t b = f[i].refs[k];
            CHECK(b < d->total_bloques);
            if (b >= d->total_bloques)
                continue;
            CHECK(disco_dueno(d, b) == f[i].id);
            CHECK(d->siguiente[b] == DISCO_FIN);    /* un dato no tiene enlaces */
            cuenta[b]++;
        }
        CHECK(indexada_verificar(d, &f[i], BLOQUE) == 0);
        esperados += f[i].bloques_total;
    }
    for (i = 0; i < d->total_bloques; i++) {
        CHECK(cuenta[i] <= 1);
        if (disco_dueno(d, i) == DISCO_LIBRE)
            CHECK(d->siguiente[i] == DISCO_FIN);
    }
    CHECK(disco_ocupados(d) == esperados);
    CHECK(disco_verificar(d) == 0);
}

int main(void)
{
    disco_t *d = disco_crear(TOTAL);
    archivo_t f[MAX_ARCH];
    int activo[MAX_ARCH];
    uint32_t sig_id, op;
    unsigned semilla, n_ok = 0, n_sin = 0, n_lib = 0, n_multi = 0;

    for (semilla = 1; semilla <= 5; semilla++) {
        srand(semilla);
        memset(f, 0, sizeof f);
        memset(activo, 0, sizeof activo);
        sig_id = 1;

        for (op = 0; op < OPERACIONES; op++) {
            if (rand() % 100 < 55) {                                 /* crear */
                uint32_t n = 1 + (uint32_t)(rand() % (rand() % 4 == 0 ? 300 : 40));
                uint64_t tam = (uint64_t)(n - 1) * BLOQUE + 1 + (uint64_t)(rand() % BLOQUE);
                uint32_t n_idx = (n + REFS - 1) / REFS;              /* modelo: ceil(n/255) */
                uint32_t necesarios = n + n_idx, libres = disco_libres(d);
                uint32_t lista[700], e = 0, i;
                archivo_t tmp;
                asig_resultado_t res;
                int slot = -1, k;

                for (k = 0; k < (int)MAX_ARCH; k++)
                    if (!activo[k]) { slot = k; break; }
                if (slot < 0)
                    continue;

                for (i = 0; i < d->total_bloques && e < necesarios; i++)   /* primeros libres */
                    if (disco_dueno(d, i) == DISCO_LIBRE)
                        lista[e++] = i;

                memset(&tmp, 0, sizeof tmp);
                res = indexada_asignar(d, &tmp, sig_id, tam, BLOQUE);

                if (libres >= necesarios) {
                    uint32_t pos = 0, g, j, restan = n, idx_prev = DISCO_FIN, dato = 0;
                    CHECK(res == ASIG_RES_OK);                       /* nunca fragmentacion */
                    CHECK(tmp.bloques_datos == n && tmp.n_indices == n_idx);
                    CHECK(tmp.bloques_total == necesarios);
                    /* modelo del reparto: [indice][hasta 255 datos] por grupo */
                    for (g = 0; g < n_idx; g++) {
                        uint32_t en_grupo = restan < REFS ? restan : REFS;
                        CHECK(g == 0 ? tmp.primero == lista[pos] : disco_siguiente(d, idx_prev) == lista[pos]);
                        idx_prev = lista[pos++];
                        for (j = 0; j < en_grupo; j++)
                            CHECK(tmp.refs[dato++] == lista[pos++]);
                        restan -= en_grupo;
                    }
                    if (n_idx > 1) n_multi++;
                    f[slot] = tmp; activo[slot] = 1; sig_id++; n_ok++;
                } else {
                    CHECK(res == ASIG_RES_ERR_SIN_ESPACIO);
                    CHECK(disco_libres(d) == libres);
                    CHECK(tmp.refs == NULL);
                    n_sin++;
                }
            } else {                                                 /* eliminar al azar */
                int k, cand[MAX_ARCH], nc = 0;
                for (k = 0; k < (int)MAX_ARCH; k++)
                    if (activo[k]) cand[nc++] = k;
                if (nc == 0)
                    continue;
                k = cand[rand() % nc];
                CHECK(indexada_liberar(d, &f[k]) == 0);
                activo[k] = 0; n_lib++;
            }
            verificar_global(d, f, activo);
        }

        {
            int k;
            for (k = 0; k < (int)MAX_ARCH; k++)
                if (activo[k]) { CHECK(indexada_liberar(d, &f[k]) == 0); activo[k] = 0; }
        }
        CHECK(disco_libres(d) == TOTAL);
        CHECK(disco_cantidad_huecos(d) == 1);
    }

    printf("operaciones: %u creaciones ok (%u con varios indices), %u sin espacio, %u eliminaciones\n",
           n_ok, n_multi, n_sin, n_lib);
    CHECK(n_ok > 0 && n_sin > 0 && n_multi > 0);
    disco_destruir(d);
    printf("\n%d comprobaciones, %d fallos\n", total, fallos);
    return fallos == 0 ? 0 : 1;
}
