#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "archivo.h"
#include "asignacion.h"
#include "disco.h"
#include "enlazada.h"
#include "indexada.h"

static int fallos = 0, total = 0;

#define CHECK(cond) do { total++; if (!(cond)) { fallos++; \
    printf("  FALLO linea %d: %s\n", __LINE__, #cond); } } while (0)

#define B1K 1024u     /* 256 entradas por indice -> 255 referencias a datos */
#define B4K 4096u

static void test_calculos(void)
{
    printf("test_calculos\n");
    CHECK(indexada_refs_por_indice(1024) == 255);
    CHECK(indexada_refs_por_indice(4096) == 1023);
    CHECK(indexada_refs_por_indice(8) == 1);
    CHECK(indexada_refs_por_indice(7) == 0);       /* una sola entrada: solo cabe el enlace */
    CHECK(indexada_refs_por_indice(4) == 0);
    CHECK(indexada_refs_por_indice(0) == 0);

    CHECK(indexada_bloques_datos(1, B1K) == 1);
    CHECK(indexada_bloques_datos(1024, B1K) == 1);
    CHECK(indexada_bloques_datos(1025, B1K) == 2);   /* aqui el bloque ENTERO guarda datos */
    CHECK(indexada_bloques_datos(0, B1K) == 0);
    CHECK(indexada_bloques_indice(1, 255) == 1);
    CHECK(indexada_bloques_indice(255, 255) == 1);
    CHECK(indexada_bloques_indice(256, 255) == 2);
    CHECK(indexada_bloques_indice(0, 255) == 0);

    CHECK(indexada_bloques_necesarios(1, B1K) == 2);                 /* 1 dato + 1 indice */
    CHECK(indexada_bloques_necesarios(255 * 1024, B1K) == 256);      /* 255 + 1 */
    CHECK(indexada_bloques_necesarios(255 * 1024 + 1, B1K) == 258);  /* 256 datos + 2 indices */
    CHECK(indexada_bloques_necesarios(0, B1K) == 0);
    CHECK(indexada_bloques_necesarios(100, 4) == 0);
    CHECK(indexada_bloques_necesarios(UINT64_MAX, B4K) ==
          (UINT64_MAX / B4K + 1) + ((UINT64_MAX / B4K + 1) / 1023 + 1));
    /* comparacion con enlazada: 4000 bytes con bloques de 1 KB */
    CHECK(indexada_bloques_necesarios(4000, B1K) == 5);              /* 4 datos + 1 indice */
    CHECK(enlazada_bloques_necesarios(4000, B1K) == 4);              /* 4 bloques con puntero */
}

static void test_asignacion_simple(void)
{
    disco_t *d = disco_crear(20);
    archivo_t a;
    printf("test_asignacion_simple\n");

    memset(&a, 0, sizeof a);
    CHECK(indexada_asignar(d, &a, 1, 3 * 1024, B1K) == ASIG_RES_OK);
    CHECK(a.bloques_datos == 3 && a.bloques_total == 4 && a.n_indices == 1);
    CHECK(a.primero == 0);                                  /* el bloque 0 es el indice */
    CHECK(a.refs[0] == 1 && a.refs[1] == 2 && a.refs[2] == 3);
    CHECK(disco_dueno(d, 0) == 1 && disco_dueno(d, 3) == 1);
    CHECK(disco_ocupados(d) == 4);                          /* el indice cuenta como ocupado */
    CHECK(disco_siguiente(d, 0) == DISCO_FIN);
    CHECK(indexada_verificar(d, &a, B1K) == 0);
    CHECK(disco_verificar(d) == 0);
    CHECK(indexada_liberar(d, &a) == 0);
    disco_destruir(d);
}

static void test_archivo_grande_varios_indices(void)
{
    disco_t *d = disco_crear(400);
    archivo_t a;
    printf("test_archivo_grande_varios_indices\n");

    memset(&a, 0, sizeof a);
    /* 300 bloques de datos > 255: necesita 2 indices encadenados */
    CHECK(indexada_asignar(d, &a, 1, 300 * 1024, B1K) == ASIG_RES_OK);
    CHECK(a.bloques_datos == 300 && a.n_indices == 2 && a.bloques_total == 302);
    CHECK(disco_ocupados(d) == 302);
    CHECK(a.primero == 0);
    CHECK(disco_siguiente(d, 0) == 256);                    /* indice0 -> indice1 */
    CHECK(disco_siguiente(d, 256) == DISCO_FIN);
    CHECK(a.refs[0] == 1 && a.refs[254] == 255);            /* indice 0: datos 1..255 */
    CHECK(a.refs[255] == 257 && a.refs[299] == 301);        /* indice 1: datos 257..301 */
    CHECK(indexada_verificar(d, &a, B1K) == 0);
    CHECK(indexada_liberar(d, &a) == 0);
    CHECK(disco_libres(d) == 400);
    disco_destruir(d);
}

static void test_bloques_separados(void)
{
    disco_t *d = disco_crear(12);
    archivo_t a;
    printf("test_bloques_separados\n");

    memset(&a, 0, sizeof a);
    disco_ocupar(d, 1, 50);
    disco_ocupar(d, 3, 50);
    disco_ocupar(d, 5, 50);                                 /* libres: 0,2,4,6,7,8,... */
    CHECK(indexada_asignar(d, &a, 2, 4 * 1024, B1K) == ASIG_RES_OK);
    CHECK(a.primero == 0 && a.n_indices == 1);
    CHECK(a.refs[0] == 2 && a.refs[1] == 4 && a.refs[2] == 6 && a.refs[3] == 7);
    CHECK(indexada_verificar(d, &a, B1K) == 0);
    CHECK(indexada_liberar(d, &a) == 0);
    CHECK(disco_dueno(d, 1) == 50);                         /* los bloques ajenos intactos */
    disco_destruir(d);
}

static void test_espacio_del_indice(void)
{
    disco_t *d = disco_crear(10);
    archivo_t a;
    printf("test_espacio_del_indice\n");

    memset(&a, 0, sizeof a);
    /* 10 bloques de datos caben en 10 bloques... pero el indice necesita 1 mas */
    CHECK(indexada_asignar(d, &a, 1, 10 * 1024, B1K) == ASIG_RES_ERR_SIN_ESPACIO);
    CHECK(disco_libres(d) == 10 && a.refs == NULL && a.id == 0);   /* nada cambio */
    /* 9 datos + 1 indice = justo los 10 bloques */
    CHECK(indexada_asignar(d, &a, 1, 9 * 1024, B1K) == ASIG_RES_OK);
    CHECK(disco_libres(d) == 0);
    CHECK(indexada_asignar(d, &a, 2, 1, B1K) == ASIG_RES_ERR_SIN_ESPACIO);
    CHECK(indexada_liberar(d, &a) == 0);
    disco_destruir(d);
}

static void test_liberar_y_reutilizar(void)
{
    disco_t *d = disco_crear(10);
    archivo_t a, b;
    printf("test_liberar_y_reutilizar\n");

    memset(&a, 0, sizeof a);
    memset(&b, 0, sizeof b);
    CHECK(indexada_asignar(d, &a, 1, 4 * 1024, B1K) == ASIG_RES_OK);   /* 5 bloques */
    CHECK(disco_ocupados(d) == 5);
    CHECK(indexada_liberar(d, &a) == 0);                    /* datos E indice */
    CHECK(disco_libres(d) == 10 && a.id == 0 && a.refs == NULL && a.n_indices == 0);
    CHECK(indexada_liberar(d, &a) == 0);                    /* doble liberacion: no-op */

    CHECK(indexada_asignar(d, &b, 2, 4 * 1024, B1K) == ASIG_RES_OK);
    CHECK(b.primero == 0 && disco_dueno(d, 0) == 2);        /* bloques reutilizados */
    CHECK(disco_verificar(d) == 0);
    CHECK(indexada_liberar(d, &b) == 0);
    disco_destruir(d);
}

static void test_localizar(void)
{
    disco_t *d = disco_crear(400);
    archivo_t a;
    acceso_t r;
    printf("test_localizar\n");

    memset(&a, 0, sizeof a);
    CHECK(indexada_asignar(d, &a, 1, 300 * 1024 - 100, B1K) == ASIG_RES_OK);  /* 300 datos, 2 indices */
    CHECK(a.bloques_datos == 300 && a.n_indices == 2);

    /* primer bloque: 1 indice + 1 dato = 2 accesos */
    CHECK(indexada_localizar(d, &a, 0, B1K, &r) == 0);
    CHECK(r.bloque_logico == 0 && r.bloque_fisico == 1 && r.accesos == 2);
    CHECK(indexada_localizar(d, &a, 1023, B1K, &r) == 0 && r.bloque_logico == 0);
    CHECK(indexada_localizar(d, &a, 1024, B1K, &r) == 0);
    CHECK(r.bloque_logico == 1 && r.bloque_fisico == 2 && r.accesos == 2);
    /* ultimo dato del primer indice: aun 2 accesos */
    CHECK(indexada_localizar(d, &a, 254 * 1024, B1K, &r) == 0);
    CHECK(r.bloque_logico == 254 && r.bloque_fisico == 255 && r.accesos == 2);
    /* primer dato del segundo indice: hay que leer 2 indices + 1 dato = 3 */
    CHECK(indexada_localizar(d, &a, 255 * 1024, B1K, &r) == 0);
    CHECK(r.bloque_logico == 255 && r.bloque_fisico == 257 && r.accesos == 3);
    /* ultimo byte valido: ultimo bloque */
    CHECK(indexada_localizar(d, &a, 300 * 1024 - 101, B1K, &r) == 0);
    CHECK(r.bloque_logico == 299 && r.bloque_fisico == 301 && r.accesos == 3);
    /* fuera del archivo / parametros invalidos */
    CHECK(indexada_localizar(d, &a, 300 * 1024 - 100, B1K, &r) == -1);
    CHECK(indexada_localizar(d, &a, UINT64_MAX, B1K, &r) == -1);
    CHECK(indexada_localizar(d, &a, 0, 4, &r) == -1);
    CHECK(indexada_localizar(NULL, &a, 0, B1K, &r) == -1);
    CHECK(indexada_localizar(d, NULL, 0, B1K, &r) == -1);
    CHECK(indexada_localizar(d, &a, 0, B1K, NULL) == -1);
    CHECK(indexada_liberar(d, &a) == 0);
    disco_destruir(d);
}

static void test_corrupcion(void)
{
    disco_t *d = disco_crear(400);
    archivo_t a;
    acceso_t r;
    printf("test_corrupcion\n");

    memset(&a, 0, sizeof a);
    indexada_asignar(d, &a, 1, 300 * 1024, B1K);
    CHECK(indexada_verificar(d, &a, B1K) == 0);

    disco_set_siguiente(d, 0, DISCO_FIN);                   /* se corta la cadena de indices */
    CHECK(indexada_verificar(d, &a, B1K) == -1);
    CHECK(indexada_localizar(d, &a, 255 * 1024, B1K, &r) == -1);   /* necesita el 2o indice */
    CHECK(indexada_localizar(d, &a, 0, B1K, &r) == 0);             /* el 1o sigue accesible */
    disco_set_siguiente(d, 0, 256);                         /* reparada */
    CHECK(indexada_verificar(d, &a, B1K) == 0);

    disco_ocupar(d, 399, 77);
    a.refs[5] = 399;                                        /* referencia a un bloque ajeno */
    CHECK(indexada_verificar(d, &a, B1K) == -1);
    CHECK(indexada_localizar(d, &a, 5 * 1024, B1K, &r) == -1);
    CHECK(indexada_liberar(d, &a) == -1);                   /* detecta el problema... */
    CHECK(disco_dueno(d, 399) == 77);                       /* ...y NO libera el bloque ajeno */
    CHECK(a.refs == NULL);                                  /* sin fuga de memoria */
    disco_destruir(d);
}

static void test_parametros_y_limites(void)
{
    disco_t *d = disco_crear(10);
    archivo_t a;
    printf("test_parametros_y_limites\n");

    memset(&a, 0, sizeof a);
    CHECK(indexada_asignar(d, &a, 1, 0, B1K) == ASIG_RES_ERR_PARAM);
    CHECK(indexada_asignar(d, &a, 0, 100, B1K) == ASIG_RES_ERR_PARAM);
    CHECK(indexada_asignar(d, &a, 1, 100, 4) == ASIG_RES_ERR_PARAM);   /* bloque sin espacio para refs */
    CHECK(indexada_asignar(NULL, &a, 1, 100, B1K) == ASIG_RES_ERR_PARAM);
    CHECK(indexada_asignar(d, NULL, 1, 100, B1K) == ASIG_RES_ERR_PARAM);
    CHECK(indexada_asignar(d, &a, 1, UINT64_MAX, B4K) == ASIG_RES_ERR_SIN_ESPACIO);
    CHECK(disco_libres(d) == 10 && a.refs == NULL);
    CHECK(indexada_liberar(NULL, &a) == -1);
    CHECK(indexada_verificar(d, &a, B1K) == -1);            /* archivo vacio no es valido */
    disco_destruir(d);
}

int main(void)
{
    test_calculos();
    test_asignacion_simple();
    test_archivo_grande_varios_indices();
    test_bloques_separados();
    test_espacio_del_indice();
    test_liberar_y_reutilizar();
    test_localizar();
    test_corrupcion();
    test_parametros_y_limites();
    printf("\n%d comprobaciones, %d fallos\n", total, fallos);
    return fallos == 0 ? 0 : 1;
}
