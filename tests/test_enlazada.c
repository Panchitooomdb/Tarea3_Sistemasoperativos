#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "archivo.h"
#include "asignacion.h"
#include "contigua.h"
#include "disco.h"
#include "enlazada.h"

static int fallos = 0, total = 0;

#define CHECK(cond) do { total++; if (!(cond)) { fallos++; \
    printf("  FALLO linea %d: %s\n", __LINE__, #cond); } } while (0)

#define B1K 1024u
#define B4K 4096u

static void test_capacidad_y_bloques(void)
{
    printf("test_capacidad_y_bloques\n");
    CHECK(enlazada_capacidad_util(4096) == 4092);
    CHECK(enlazada_capacidad_util(1024) == 1020);
    CHECK(enlazada_capacidad_util(4) == 0);
    CHECK(enlazada_capacidad_util(3) == 0);
    CHECK(enlazada_capacidad_util(0) == 0);

    CHECK(enlazada_bloques_necesarios(4092, B4K) == 1);
    CHECK(enlazada_bloques_necesarios(4093, B4K) == 2);
    CHECK(enlazada_bloques_necesarios(10000, B4K) == 3);
    CHECK(enlazada_bloques_necesarios(0, B4K) == 0);
    CHECK(enlazada_bloques_necesarios(100, 4) == 0);

    /* el espacio de punteros SI cuesta bloques: 8192 bytes son 2 bloques en
     * contigua, pero 3 en enlazada (2 * 4092 = 8184 < 8192) */
    CHECK(contigua_bloques_necesarios(8192, B4K) == 2);
    CHECK(enlazada_bloques_necesarios(8192, B4K) == 3);
    /* justo en el limite de la capacidad util */
    CHECK(enlazada_bloques_necesarios(3 * 1020, B1K) == 3);
    CHECK(enlazada_bloques_necesarios(3 * 1020 + 1, B1K) == 4);
    /* sin desbordamiento */
    CHECK(enlazada_bloques_necesarios(UINT64_MAX, B4K) == UINT64_MAX / 4092 + 1);
}

static void test_asignacion_simple(void)
{
    disco_t *d = disco_crear(20);
    archivo_t a;
    printf("test_asignacion_simple\n");

    memset(&a, 0, sizeof a);
    CHECK(enlazada_asignar(d, &a, 1, 10000, B4K) == ASIG_RES_OK);
    CHECK(a.id == 1 && a.tam_bytes == 10000);
    CHECK(a.bloques_datos == 3 && a.bloques_total == 3 && a.primero == 0);
    CHECK(disco_ocupados(d) == 3);
    CHECK(disco_siguiente(d, 0) == 1);
    CHECK(disco_siguiente(d, 1) == 2);
    CHECK(disco_siguiente(d, 2) == DISCO_FIN);
    CHECK(enlazada_verificar(d, &a) == 0);
    CHECK(disco_verificar(d) == 0);
    disco_destruir(d);
}

static void test_bloques_separados(void)
{
    disco_t *d = disco_crear(10);
    archivo_t a, c, g;
    printf("test_bloques_separados\n");

    memset(&a, 0, sizeof a);
    memset(&c, 0, sizeof c);
    memset(&g, 0, sizeof g);
    /* ocupar 1, 3 y 5 con otro archivo: libres = 0,2,4,6,7,8,9 (7 en total, max 4 seguidos) */
    disco_ocupar(d, 1, 50);
    disco_ocupar(d, 3, 50);
    disco_ocupar(d, 5, 50);
    CHECK(disco_mayor_libre_contiguo(d) == 4);

    /* 5 bloques: contigua FALLA por fragmentacion... */
    CHECK(contigua_asignar(d, &c, 2, 5 * B1K, B1K) == ASIG_RES_ERR_FRAGMENTACION);
    /* ...pero enlazada lo logra con bloques separados (5 * 1020 bytes = 5 bloques) */
    CHECK(enlazada_asignar(d, &a, 3, 5 * 1020, B1K) == ASIG_RES_OK);
    CHECK(a.primero == 0 && a.bloques_total == 5);
    CHECK(disco_siguiente(d, 0) == 2);
    CHECK(disco_siguiente(d, 2) == 4);
    CHECK(disco_siguiente(d, 4) == 6);
    CHECK(disco_siguiente(d, 6) == 7);
    CHECK(disco_siguiente(d, 7) == DISCO_FIN);
    CHECK(enlazada_verificar(d, &a) == 0);

    /* quedan 2 libres (8, 9): pedir 3 bloques es falta de espacio, NUNCA fragmentacion */
    CHECK(enlazada_asignar(d, &g, 4, 3 * 1020, B1K) == ASIG_RES_ERR_SIN_ESPACIO);
    CHECK(disco_libres(d) == 2 && g.id == 0);
    /* y 2 bloques si caben, usando exactamente el espacio que quedaba */
    CHECK(enlazada_asignar(d, &g, 4, 2 * 1020, B1K) == ASIG_RES_OK);
    CHECK(disco_libres(d) == 0);
    CHECK(disco_verificar(d) == 0);
    disco_destruir(d);
}

static void test_liberar_y_reutilizar(void)
{
    disco_t *d = disco_crear(10);
    archivo_t a, b;
    printf("test_liberar_y_reutilizar\n");

    memset(&a, 0, sizeof a);
    memset(&b, 0, sizeof b);
    enlazada_asignar(d, &a, 1, 4 * 1020, B1K);            /* 0,1,2,3 */
    CHECK(enlazada_liberar(d, &a) == 0);
    CHECK(disco_libres(d) == 10 && a.id == 0 && a.bloques_total == 0);
    CHECK(disco_siguiente(d, 0) == DISCO_FIN);            /* punteros borrados */
    CHECK(disco_siguiente(d, 2) == DISCO_FIN);
    CHECK(enlazada_liberar(d, &a) == 0);                  /* doble liberacion: no-op */

    CHECK(enlazada_asignar(d, &b, 2, 4 * 1020, B1K) == ASIG_RES_OK);
    CHECK(b.primero == 0 && disco_dueno(d, 0) == 2);       /* bloques reutilizados */
    CHECK(disco_verificar(d) == 0);
    disco_destruir(d);
}

static void test_localizar(void)
{
    disco_t *d = disco_crear(10);
    archivo_t a;
    acceso_t r;
    printf("test_localizar\n");

    memset(&a, 0, sizeof a);
    disco_ocupar(d, 1, 50);
    disco_ocupar(d, 3, 50);                               /* cadena: 0,2,4,5,6 */
    CHECK(enlazada_asignar(d, &a, 2, 5000, B1K) == ASIG_RES_OK);   /* ceil(5000/1020)=5 */
    CHECK(a.bloques_total == 5);

    /* primer bloque: 1 acceso */
    CHECK(enlazada_localizar(d, &a, 0, B1K, &r) == 0);
    CHECK(r.bloque_logico == 0 && r.bloque_fisico == 0 && r.accesos == 1);
    CHECK(enlazada_localizar(d, &a, 1019, B1K, &r) == 0 && r.bloque_logico == 0);
    /* el limite se calcula con la capacidad UTIL (1020), no con 1024 */
    CHECK(enlazada_localizar(d, &a, 1020, B1K, &r) == 0);
    CHECK(r.bloque_logico == 1 && r.bloque_fisico == 2 && r.accesos == 2);
    CHECK(enlazada_localizar(d, &a, 2 * 1020, B1K, &r) == 0);
    CHECK(r.bloque_logico == 2 && r.bloque_fisico == 4 && r.accesos == 3);
    /* ultimo byte: ultimo bloque, accesos = bloques del archivo */
    CHECK(enlazada_localizar(d, &a, 4999, B1K, &r) == 0);
    CHECK(r.bloque_logico == 4 && r.bloque_fisico == 6 && r.accesos == 5);
    /* fuera del archivo y parametros invalidos */
    CHECK(enlazada_localizar(d, &a, 5000, B1K, &r) == -1);
    CHECK(enlazada_localizar(d, &a, UINT64_MAX, B1K, &r) == -1);
    CHECK(enlazada_localizar(d, &a, 0, 4, &r) == -1);
    CHECK(enlazada_localizar(NULL, &a, 0, B1K, &r) == -1);
    CHECK(enlazada_localizar(d, NULL, 0, B1K, &r) == -1);
    CHECK(enlazada_localizar(d, &a, 0, B1K, NULL) == -1);
    disco_destruir(d);
}

static void test_cadena_corrupta(void)
{
    disco_t *d = disco_crear(10);
    archivo_t a;
    acceso_t r;
    printf("test_cadena_corrupta\n");

    memset(&a, 0, sizeof a);
    enlazada_asignar(d, &a, 1, 4 * 1020, B1K);            /* 0,1,2,3 */
    CHECK(enlazada_verificar(d, &a) == 0);

    disco_set_siguiente(d, 1, DISCO_FIN);                  /* cadena cortada en el bloque 1 */
    CHECK(enlazada_verificar(d, &a) == -1);
    CHECK(enlazada_localizar(d, &a, 3 * 1020, B1K, &r) == -1);
    CHECK(enlazada_localizar(d, &a, 0, B1K, &r) == 0);     /* lo anterior al corte sigue accesible */

    disco_set_siguiente(d, 1, 0);                          /* ciclo 0 -> 1 -> 0 */
    CHECK(enlazada_verificar(d, &a) == -1);

    disco_set_siguiente(d, 1, 2);                          /* reparada */
    CHECK(enlazada_verificar(d, &a) == 0);
    disco_ocupar(d, 8, 77);
    disco_set_siguiente(d, 3, 8);                          /* cadena apunta a bloque ajeno */
    CHECK(enlazada_verificar(d, &a) == -1);
    CHECK(enlazada_liberar(d, &a) == -1);                  /* no libera el bloque ajeno */
    CHECK(disco_dueno(d, 8) == 77);
    disco_destruir(d);
}

static void test_parametros_y_limites(void)
{
    disco_t *d = disco_crear(10);
    archivo_t a;
    printf("test_parametros_y_limites\n");

    memset(&a, 0, sizeof a);
    CHECK(enlazada_asignar(d, &a, 1, 0, B4K) == ASIG_RES_ERR_PARAM);
    CHECK(enlazada_asignar(d, &a, 0, 100, B4K) == ASIG_RES_ERR_PARAM);
    CHECK(enlazada_asignar(d, &a, 1, 100, 4) == ASIG_RES_ERR_PARAM);   /* bloque sin espacio util */
    CHECK(enlazada_asignar(NULL, &a, 1, 100, B4K) == ASIG_RES_ERR_PARAM);
    CHECK(enlazada_asignar(d, NULL, 1, 100, B4K) == ASIG_RES_ERR_PARAM);
    CHECK(disco_libres(d) == 10);

    CHECK(enlazada_asignar(d, &a, 1, UINT64_MAX, B4K) == ASIG_RES_ERR_SIN_ESPACIO);
    CHECK(enlazada_asignar(d, &a, 1, 11 * 4092, B4K) == ASIG_RES_ERR_SIN_ESPACIO);
    CHECK(enlazada_asignar(d, &a, 1, 10 * 4092, B4K) == ASIG_RES_OK);   /* justo todo el disco */
    CHECK(disco_libres(d) == 0);
    CHECK(enlazada_asignar(d, &a, 2, 1, B4K) == ASIG_RES_ERR_SIN_ESPACIO);
    CHECK(enlazada_liberar(NULL, &a) == -1);
    disco_destruir(d);
}

int main(void)
{
    test_capacidad_y_bloques();
    test_asignacion_simple();
    test_bloques_separados();
    test_liberar_y_reutilizar();
    test_localizar();
    test_cadena_corrupta();
    test_parametros_y_limites();
    printf("\n%d comprobaciones, %d fallos\n", total, fallos);
    return fallos == 0 ? 0 : 1;
}
