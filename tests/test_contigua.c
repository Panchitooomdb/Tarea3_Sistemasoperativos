#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "archivo.h"
#include "asignacion.h"
#include "contigua.h"
#include "disco.h"

static int fallos = 0, total = 0;

#define CHECK(cond) do { total++; if (!(cond)) { fallos++; \
    printf("  FALLO linea %d: %s\n", __LINE__, #cond); } } while (0)

#define B4K 4096u

static void test_bloques_necesarios(void)
{
    printf("test_bloques_necesarios\n");
    CHECK(contigua_bloques_necesarios(1, B4K) == 1);
    CHECK(contigua_bloques_necesarios(B4K, B4K) == 1);
    CHECK(contigua_bloques_necesarios(B4K + 1, B4K) == 2);
    CHECK(contigua_bloques_necesarios(10000, B4K) == 3);
    CHECK(contigua_bloques_necesarios(0, B4K) == 0);
    CHECK(contigua_bloques_necesarios(10, 0) == 0);
    /* sin desbordamiento con tamanos enormes */
    CHECK(contigua_bloques_necesarios(UINT64_MAX, 1) == UINT64_MAX);
    CHECK(contigua_bloques_necesarios(UINT64_MAX, B4K) == (UINT64_MAX / B4K) + 1);
}

static void test_buscar(void)
{
    disco_t *d = disco_crear(10);
    uint32_t ini = 99;
    printf("test_buscar\n");

    CHECK(contigua_buscar(d, 10, &ini) == 0 && ini == 0);
    disco_ocupar(d, 2, 1);
    disco_ocupar(d, 3, 1);                 /* libres: [0,1] [4..9] */
    CHECK(contigua_buscar(d, 2, &ini) == 0 && ini == 0);   /* primer ajuste */
    CHECK(contigua_buscar(d, 3, &ini) == 0 && ini == 4);
    CHECK(contigua_buscar(d, 6, &ini) == 0 && ini == 4);
    CHECK(contigua_buscar(d, 7, &ini) == -1);
    CHECK(contigua_buscar(d, 0, &ini) == -1);
    CHECK(contigua_buscar(NULL, 1, &ini) == -1);
    CHECK(contigua_buscar(d, 1, NULL) == -1);
    disco_destruir(d);
}

static void test_asignacion_exitosa(void)
{
    disco_t *d = disco_crear(100);
    archivo_t a, b;
    uint32_t i;
    printf("test_asignacion_exitosa\n");

    memset(&a, 0, sizeof a);
    memset(&b, 0, sizeof b);

    CHECK(contigua_asignar(d, &a, 1, 10000, B4K) == ASIG_RES_OK);
    CHECK(a.id == 1 && a.tam_bytes == 10000);
    CHECK(a.bloques_datos == 3 && a.bloques_total == 3);
    CHECK(a.inicio == 0 && a.largo == 3);
    CHECK(disco_ocupados(d) == 3 && disco_libres(d) == 97);
    for (i = 0; i < 3; i++)
        CHECK(disco_dueno(d, i) == 1);
    CHECK(disco_dueno(d, 3) == DISCO_LIBRE);

    CHECK(contigua_asignar(d, &b, 2, 3 * B4K, B4K) == ASIG_RES_OK);
    CHECK(b.inicio == 3 && b.largo == 3);          /* justo a continuacion */
    CHECK(disco_ocupados(d) == 6);
    CHECK(disco_verificar(d) == 0);
    disco_destruir(d);
}

static void test_fragmentacion_externa(void)
{
    disco_t *d = disco_crear(10);
    archivo_t f[5], g;
    uint32_t i;
    printf("test_fragmentacion_externa\n");

    memset(&g, 0, sizeof g);
    /* 5 archivos de 2 bloques: llenan el disco entero */
    for (i = 0; i < 5; i++) {
        memset(&f[i], 0, sizeof f[i]);
        CHECK(contigua_asignar(d, &f[i], i + 1, 2048, 1024) == ASIG_RES_OK);
    }
    CHECK(disco_libres(d) == 0);
    CHECK(contigua_asignar(d, &g, 9, 1024, 1024) == ASIG_RES_ERR_SIN_ESPACIO);

    /* liberar 2 y 4 -> libres [2,3] y [6,7]: 4 libres pero maximo 2 seguidos */
    CHECK(contigua_liberar(d, &f[1]) == 0);
    CHECK(contigua_liberar(d, &f[3]) == 0);
    CHECK(disco_libres(d) == 4);
    CHECK(disco_mayor_libre_contiguo(d) == 2);
    CHECK(disco_cantidad_huecos(d) == 2);

    /* 3 bloques: hay 4 libres en total -> FRAGMENTACION, no SIN_ESPACIO */
    CHECK(contigua_asignar(d, &g, 9, 3 * 1024, 1024) == ASIG_RES_ERR_FRAGMENTACION);
    CHECK(disco_libres(d) == 4);           /* el disco no cambio */
    CHECK(g.id == 0 && g.largo == 0);      /* el archivo tampoco */

    /* 5 bloques: no alcanzan ni sumando -> SIN_ESPACIO */
    CHECK(contigua_asignar(d, &g, 9, 5 * 1024, 1024) == ASIG_RES_ERR_SIN_ESPACIO);

    /* 2 bloques si caben: primer ajuste usa el primer hueco */
    CHECK(contigua_asignar(d, &g, 9, 2048, 1024) == ASIG_RES_OK);
    CHECK(g.inicio == 2);
    CHECK(disco_verificar(d) == 0);
    disco_destruir(d);
}

static void test_liberar_y_reutilizar(void)
{
    disco_t *d = disco_crear(20);
    archivo_t a, b;
    printf("test_liberar_y_reutilizar\n");

    memset(&a, 0, sizeof a);
    memset(&b, 0, sizeof b);
    CHECK(contigua_asignar(d, &a, 1, 5 * B4K, B4K) == ASIG_RES_OK);
    CHECK(a.inicio == 0 && disco_ocupados(d) == 5);

    CHECK(contigua_liberar(d, &a) == 0);
    CHECK(disco_libres(d) == 20 && a.id == 0 && a.largo == 0);
    CHECK(contigua_liberar(d, &a) == 0);           /* doble liberacion: no-op */
    CHECK(disco_libres(d) == 20);

    /* los bloques liberados se reutilizan */
    CHECK(contigua_asignar(d, &b, 2, 5 * B4K, B4K) == ASIG_RES_OK);
    CHECK(b.inicio == 0 && disco_dueno(d, 0) == 2);
    CHECK(disco_verificar(d) == 0);

    /* un archivo con datos corruptos se detecta al liberar */
    b.id = 77;                                     /* ya no coincide con el dueno */
    CHECK(contigua_liberar(d, &b) == -1);
    disco_destruir(d);
}

static void test_parametros_y_limites(void)
{
    disco_t *d = disco_crear(10);
    archivo_t a;
    printf("test_parametros_y_limites\n");

    memset(&a, 0, sizeof a);
    CHECK(contigua_asignar(d, &a, 1, 0, B4K) == ASIG_RES_ERR_PARAM);
    CHECK(contigua_asignar(d, &a, 0, 100, B4K) == ASIG_RES_ERR_PARAM);
    CHECK(contigua_asignar(d, &a, 1, 100, 0) == ASIG_RES_ERR_PARAM);
    CHECK(contigua_asignar(NULL, &a, 1, 100, B4K) == ASIG_RES_ERR_PARAM);
    CHECK(contigua_asignar(d, NULL, 1, 100, B4K) == ASIG_RES_ERR_PARAM);
    CHECK(disco_libres(d) == 10);

    /* archivo mas grande que todo el disco, incluso uno gigantesco */
    CHECK(contigua_asignar(d, &a, 1, 11 * B4K, B4K) == ASIG_RES_ERR_SIN_ESPACIO);
    CHECK(contigua_asignar(d, &a, 1, UINT64_MAX, B4K) == ASIG_RES_ERR_SIN_ESPACIO);

    /* archivo que ocupa justo todo el disco */
    CHECK(contigua_asignar(d, &a, 1, 10 * B4K, B4K) == ASIG_RES_OK);
    CHECK(disco_libres(d) == 0);
    CHECK(contigua_asignar(d, &a, 2, 1, B4K) == ASIG_RES_ERR_SIN_ESPACIO);
    CHECK(contigua_liberar(NULL, &a) == -1);
    disco_destruir(d);
}

static void test_localizar(void)
{
    disco_t *d = disco_crear(100);
    archivo_t pad, a;
    acceso_t r;
    printf("test_localizar\n");

    memset(&pad, 0, sizeof pad);
    memset(&a, 0, sizeof a);
    contigua_asignar(d, &pad, 1, 7 * B4K, B4K);                  /* ocupa 0..6 */
    CHECK(contigua_asignar(d, &a, 2, 10000, B4K) == ASIG_RES_OK); /* 7..9 */
    CHECK(a.inicio == 7 && a.largo == 3);

    /* primer bloque */
    CHECK(contigua_localizar(&a, 0, B4K, &r) == 0);
    CHECK(r.bloque_logico == 0 && r.bloque_fisico == 7 && r.accesos == 1);
    CHECK(r.pos_bytes == 0);
    /* ultimo byte del primer bloque y primer byte del segundo */
    CHECK(contigua_localizar(&a, B4K - 1, B4K, &r) == 0 && r.bloque_logico == 0);
    CHECK(contigua_localizar(&a, B4K, B4K, &r) == 0);
    CHECK(r.bloque_logico == 1 && r.bloque_fisico == 8);
    /* ultimo byte valido del archivo -> ultimo bloque */
    CHECK(contigua_localizar(&a, 9999, B4K, &r) == 0);
    CHECK(r.bloque_logico == 2 && r.bloque_fisico == 9 && r.accesos == 1);
    /* fuera del archivo */
    CHECK(contigua_localizar(&a, 10000, B4K, &r) == -1);
    CHECK(contigua_localizar(&a, UINT64_MAX, B4K, &r) == -1);
    CHECK(contigua_localizar(&a, 0, 0, &r) == -1);
    CHECK(contigua_localizar(NULL, 0, B4K, &r) == -1);
    CHECK(contigua_localizar(&a, 0, B4K, NULL) == -1);
    disco_destruir(d);
}

static void test_mensajes(void)
{
    printf("test_mensajes\n");
    CHECK(strlen(asig_mensaje(ASIG_RES_OK)) > 0);
    CHECK(strlen(asig_mensaje(ASIG_RES_ERR_SIN_ESPACIO)) > 0);
    CHECK(strlen(asig_mensaje(ASIG_RES_ERR_FRAGMENTACION)) > 0);
    CHECK(strcmp(asig_mensaje(ASIG_RES_ERR_SIN_ESPACIO),
                 asig_mensaje(ASIG_RES_ERR_FRAGMENTACION)) != 0);
}

int main(void)
{
    test_bloques_necesarios();
    test_buscar();
    test_asignacion_exitosa();
    test_fragmentacion_externa();
    test_liberar_y_reutilizar();
    test_parametros_y_limites();
    test_localizar();
    test_mensajes();
    printf("\n%d comprobaciones, %d fallos\n", total, fallos);
    return fallos == 0 ? 0 : 1;
}
