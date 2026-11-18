#include <stdio.h>
#include <string.h>

#include "config.h"
#include "disco.h"

static int fallos = 0, total = 0;

#define CHECK(cond) do { total++; if (!(cond)) { fallos++; \
    printf("  FALLO linea %d: %s\n", __LINE__, #cond); } } while (0)

static void test_creacion(void)
{
    disco_t *d;
    uint32_t i;
    printf("test_creacion\n");

    CHECK(disco_crear(0) == NULL);
    disco_destruir(NULL);                     /* no debe fallar */

    d = disco_crear(10);
    CHECK(d != NULL);
    CHECK(d->total_bloques == 10);
    CHECK(disco_libres(d) == 10);
    CHECK(disco_ocupados(d) == 0);
    CHECK(disco_mayor_libre_contiguo(d) == 10);
    CHECK(disco_cantidad_huecos(d) == 1);
    CHECK(disco_verificar(d) == 0);
    for (i = 0; i < 10; i++)
        CHECK(disco_esta_libre(d, i));
    disco_destruir(d);

    /* el disco mas grande permitido por la configuracion */
    d = disco_crear(CFG_TOTAL_BLOQUES_MAX);
    CHECK(d != NULL && disco_libres(d) == CFG_TOTAL_BLOQUES_MAX);
    disco_destruir(d);
}

static void test_ocupar_liberar(void)
{
    disco_t *d = disco_crear(10);
    printf("test_ocupar_liberar\n");

    CHECK(disco_ocupar(d, 3, 7) == 0);
    CHECK(disco_libres(d) == 9 && disco_ocupados(d) == 1);
    CHECK(disco_dueno(d, 3) == 7);
    CHECK(!disco_esta_libre(d, 3));
    CHECK(disco_cantidad_huecos(d) == 2);          /* [0..2] y [4..9] */
    CHECK(disco_mayor_libre_contiguo(d) == 6);

    /* casos de error: nada debe cambiar */
    CHECK(disco_ocupar(d, 3, 8) == -1);            /* ya ocupado */
    CHECK(disco_ocupar(d, 4, DISCO_LIBRE) == -1);  /* id 0 invalido */
    CHECK(disco_ocupar(d, 10, 1) == -1);           /* fuera de rango */
    CHECK(disco_ocupar(NULL, 0, 1) == -1);
    CHECK(disco_liberar(d, 5) == -1);              /* ya libre */
    CHECK(disco_liberar(d, 10) == -1);             /* fuera de rango */
    CHECK(disco_dueno(d, 3) == 7 && disco_libres(d) == 9);
    CHECK(disco_dueno(d, 99) == DISCO_LIBRE);
    CHECK(!disco_esta_libre(d, 99));

    CHECK(disco_liberar(d, 3) == 0);
    CHECK(disco_libres(d) == 10 && disco_cantidad_huecos(d) == 1);

    /* reutilizacion de un bloque liberado por otro archivo */
    CHECK(disco_ocupar(d, 3, 9) == 0 && disco_dueno(d, 3) == 9);
    CHECK(disco_verificar(d) == 0);
    disco_destruir(d);
}

static void test_huecos_y_disco_lleno(void)
{
    disco_t *d = disco_crear(10);
    uint32_t i;
    printf("test_huecos_y_disco_lleno\n");

    /* ocupados 2,5,6 -> libres [0,1] [3,4] [7,8,9] */
    CHECK(disco_ocupar(d, 2, 1) == 0);
    CHECK(disco_ocupar(d, 5, 1) == 0);
    CHECK(disco_ocupar(d, 6, 1) == 0);
    CHECK(disco_libres(d) == 7);
    CHECK(disco_cantidad_huecos(d) == 3);
    CHECK(disco_mayor_libre_contiguo(d) == 3);

    /* disco completamente ocupado */
    for (i = 0; i < 10; i++)
        if (disco_esta_libre(d, i))
            CHECK(disco_ocupar(d, i, 2) == 0);
    CHECK(disco_libres(d) == 0 && disco_ocupados(d) == 10);
    CHECK(disco_mayor_libre_contiguo(d) == 0);
    CHECK(disco_cantidad_huecos(d) == 0);
    CHECK(disco_verificar(d) == 0);
    disco_destruir(d);
}

static void test_verificar_detecta_corrupcion(void)
{
    disco_t *d = disco_crear(10);
    printf("test_verificar_detecta_corrupcion\n");

    CHECK(disco_verificar(d) == 0);
    d->libres++;                       /* corrupcion artificial */
    CHECK(disco_verificar(d) == -1);
    d->libres--;
    d->dueno[4] = 5;                   /* bloque ocupado sin pasar por la API */
    CHECK(disco_verificar(d) == -1);
    CHECK(disco_verificar(NULL) == -1);
    disco_destruir(d);
}

static void leer_todo(FILE *f, char *buf, size_t tam)
{
    size_t n;
    rewind(f);
    n = fread(buf, 1, tam - 1, f);
    buf[n] = '\0';
}

static void test_mapa(void)
{
    char buf[4096];
    char *cuerpo;
    disco_t *d;
    FILE *f;
    uint32_t i;
    printf("test_mapa\n");

    d = disco_crear(8);
    for (i = 0; i < 4; i++)
        disco_ocupar(d, i, 1);
    f = tmpfile();
    disco_imprimir_mapa(d, f);
    leer_todo(f, buf, sizeof buf);
    fclose(f);
    cuerpo = strchr(buf, '\n');            /* saltar la linea de leyenda */
    CHECK(cuerpo != NULL && strstr(cuerpo, "####....") != NULL);
    disco_destruir(d);

    /* disco grande: varios bloques por caracter -> aparece '+' */
    d = disco_crear(5000);                 /* 3 bloques por caracter */
    disco_ocupar(d, 0, 1);
    f = tmpfile();
    disco_imprimir_mapa(d, f);
    leer_todo(f, buf, sizeof buf);
    fclose(f);
    cuerpo = strchr(buf, '\n');
    CHECK(cuerpo != NULL && strstr(cuerpo, "+.") != NULL);
    disco_destruir(d);
}

int main(void)
{
    test_creacion();
    test_ocupar_liberar();
    test_huecos_y_disco_lleno();
    test_verificar_detecta_corrupcion();
    test_mapa();
    printf("\n%d comprobaciones, %d fallos\n", total, fallos);
    return fallos == 0 ? 0 : 1;
}
