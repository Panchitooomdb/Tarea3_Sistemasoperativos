/* Estres determinista de la asignacion enlazada contra un modelo independiente. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "archivo.h"
#include "asignacion.h"
#include "disco.h"
#include "enlazada.h"

#define TOTAL       200u
#define BLOQUE      1024u
#define UTIL        (BLOQUE - 4u)
#define MAX_ARCH    TOTAL
#define OPERACIONES 20000

static int fallos = 0, total = 0;

#define CHECK(cond) do { total++; if (!(cond)) { fallos++; \
    if (fallos < 20) printf("  FALLO linea %d: %s\n", __LINE__, #cond); } } while (0)

/* Cadena recorrida de forma independiente: cada bloque debe ser de su dueno,
 * no repetirse, y la cadena debe terminar justo tras 'largo' bloques. */
static void verificar_global(const disco_t *d, const archivo_t *f, const int *activo)
{
    static uint32_t cuenta[TOTAL];
    uint32_t i, k, cur, esperados = 0;

    memset(cuenta, 0, sizeof cuenta);
    for (i = 0; i < MAX_ARCH; i++) {
        if (!activo[i])
            continue;
        cur = f[i].primero;
        for (k = 0; k < f[i].bloques_total; k++) {
            CHECK(cur < d->total_bloques);
            if (cur >= d->total_bloques)
                break;
            CHECK(disco_dueno(d, cur) == f[i].id);
            cuenta[cur]++;
            cur = disco_siguiente(d, cur);
        }
        CHECK(cur == DISCO_FIN);
        CHECK(enlazada_verificar(d, &f[i]) == 0);
        esperados += f[i].bloques_total;
    }
    for (i = 0; i < d->total_bloques; i++) {
        CHECK(cuenta[i] <= 1);                               /* ningun bloque compartido */
        if (disco_dueno(d, i) == DISCO_LIBRE)
            CHECK(d->siguiente[i] == DISCO_FIN);             /* libre => sin puntero */
    }
    CHECK(disco_ocupados(d) == esperados);                   /* sin bloques huerfanos */
    CHECK(disco_verificar(d) == 0);
}

int main(void)
{
    disco_t *d = disco_crear(TOTAL);
    archivo_t f[MAX_ARCH];
    int activo[MAX_ARCH];
    uint32_t sig_id, op;
    unsigned semilla, n_ok = 0, n_sin = 0, n_lib = 0, n_separados = 0;

    for (semilla = 1; semilla <= 5; semilla++) {
        srand(semilla);
        memset(f, 0, sizeof f);
        memset(activo, 0, sizeof activo);
        sig_id = 1;

        for (op = 0; op < OPERACIONES; op++) {
            if (rand() % 100 < 55) {                         /* crear */
                uint32_t n = 1 + (uint32_t)(rand() % 30);
                uint64_t tam = (uint64_t)(n - 1) * UTIL + 1 + (uint64_t)(rand() % UTIL);
                uint32_t libres = disco_libres(d), esperados[30], e = 0, i;
                archivo_t tmp;
                asig_resultado_t res;
                int slot = -1, k;

                for (k = 0; k < (int)MAX_ARCH; k++)
                    if (!activo[k]) { slot = k; break; }
                if (slot < 0)
                    continue;

                /* modelo: los n primeros bloques libres, calculados aparte */
                for (i = 0; i < d->total_bloques && e < n; i++)
                    if (disco_dueno(d, i) == DISCO_LIBRE)
                        esperados[e++] = i;

                memset(&tmp, 0, sizeof tmp);
                res = enlazada_asignar(d, &tmp, sig_id, tam, BLOQUE);

                if (libres >= n) {
                    uint32_t cur;
                    CHECK(res == ASIG_RES_OK);              /* nunca falla por fragmentacion */
                    CHECK(tmp.bloques_total == n);
                    cur = tmp.primero;
                    for (i = 0; i < n; i++) {
                        CHECK(cur == esperados[i]);          /* mismos bloques, mismo orden */
                        if (i > 0 && esperados[i] != esperados[i - 1] + 1)
                            n_separados++;
                        cur = disco_siguiente(d, cur);
                    }
                    f[slot] = tmp; activo[slot] = 1; sig_id++; n_ok++;
                } else {
                    CHECK(res == ASIG_RES_ERR_SIN_ESPACIO);
                    CHECK(disco_libres(d) == libres);
                    n_sin++;
                }
            } else {                                         /* eliminar al azar */
                int k, cand[MAX_ARCH], nc = 0;
                for (k = 0; k < (int)MAX_ARCH; k++)
                    if (activo[k]) cand[nc++] = k;
                if (nc == 0)
                    continue;
                k = cand[rand() % nc];
                CHECK(enlazada_liberar(d, &f[k]) == 0);
                activo[k] = 0; n_lib++;
            }
            verificar_global(d, f, activo);
        }

        {
            int k;
            for (k = 0; k < (int)MAX_ARCH; k++)
                if (activo[k]) { CHECK(enlazada_liberar(d, &f[k]) == 0); activo[k] = 0; }
        }
        CHECK(disco_libres(d) == TOTAL);
        CHECK(disco_cantidad_huecos(d) == 1);
    }

    printf("operaciones: %u creaciones ok, %u sin espacio, %u eliminaciones, %u saltos no contiguos\n",
           n_ok, n_sin, n_lib, n_separados);
    CHECK(n_ok > 0 && n_sin > 0 && n_separados > 0);        /* se ejercitaron los caminos clave */
    disco_destruir(d);
    printf("\n%d comprobaciones, %d fallos\n", total, fallos);
    return fallos == 0 ? 0 : 1;
}
