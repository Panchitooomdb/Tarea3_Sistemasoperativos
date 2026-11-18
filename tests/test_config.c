#include <stdio.h>
#include <string.h>

#include "config.h"

static int fallos = 0, total = 0;

#define CHECK(cond) do { total++; if (!(cond)) { fallos++; \
    printf("  FALLO linea %d: %s\n", __LINE__, #cond); } } while (0)

static config_t base(void)
{
    config_t c;
    memset(&c, 0, sizeof c);
    c.disco_mb = 100; c.bloque_kb = 4; c.mecanismo = ASIG_CONTIGUA;
    c.archivo_min_kb = 10; c.archivo_max_kb = 100;
    return c;
}

static int leer_desde_texto(const char *texto, config_t *cfg)
{
    FILE *in = fmemopen((void *)texto, strlen(texto), "r");
    FILE *out = tmpfile();
    int r;
    memset(cfg, 0, sizeof *cfg);
    r = config_leer(cfg, in, out);
    fclose(in);
    fclose(out);
    return r;
}

static void test_parseo(void)
{
    uint64_t v;
    printf("test_parseo\n");
    CHECK(config_parsear_u64("42", &v) == 0 && v == 42);
    CHECK(config_parsear_u64("  7  \n", &v) == 0 && v == 7);
    CHECK(config_parsear_u64("", &v) == -1);
    CHECK(config_parsear_u64("\n", &v) == -1);
    CHECK(config_parsear_u64("-5", &v) == -1);
    CHECK(config_parsear_u64("+5", &v) == -1);
    CHECK(config_parsear_u64("abc", &v) == -1);
    CHECK(config_parsear_u64("12abc", &v) == -1);
    CHECK(config_parsear_u64("3.5", &v) == -1);
    CHECK(config_parsear_u64("99999999999999999999999", &v) == -1);
    CHECK(config_parsear_u64(NULL, &v) == -1);

    CHECK(config_parsear_mecanismo("contigua") == ASIG_CONTIGUA);
    CHECK(config_parsear_mecanismo("ENLAZADA\n") == ASIG_ENLAZADA);
    CHECK(config_parsear_mecanismo("Indexada") == ASIG_INDEXADA);
    CHECK(config_parsear_mecanismo("1") == ASIG_CONTIGUA);
    CHECK(config_parsear_mecanismo("3") == ASIG_INDEXADA);
    CHECK(config_parsear_mecanismo("4") == ASIG_INVALIDA);
    CHECK(config_parsear_mecanismo("") == ASIG_INVALIDA);
    CHECK(config_parsear_mecanismo("fat") == ASIG_INVALIDA);
}

static void test_validacion(void)
{
    config_t c;
    printf("test_validacion\n");

    c = base();
    CHECK(config_validar(&c) == CFG_OK);
    CHECK(c.total_bloques == 25600);
    CHECK(c.disco_bytes == 104857600ull);
    CHECK(c.bloque_bytes == 4096);

    c = base(); c.disco_mb = 0;          CHECK(config_validar(&c) == CFG_ERR_DISCO_RANGO);
    c = base(); c.disco_mb = 70000;      CHECK(config_validar(&c) == CFG_ERR_DISCO_RANGO);
    c = base(); c.bloque_kb = 0;         CHECK(config_validar(&c) == CFG_ERR_BLOQUE_RANGO);
    c = base(); c.bloque_kb = 2048;      CHECK(config_validar(&c) == CFG_ERR_BLOQUE_RANGO);
    c = base(); c.bloque_kb = 3;         CHECK(config_validar(&c) == CFG_ERR_NO_DIVISIBLE);
    c = base(); c.disco_mb = 1; c.bloque_kb = 512; CHECK(config_validar(&c) == CFG_ERR_POCOS_BLOQUES);
    c = base(); c.disco_mb = 8192; c.bloque_kb = 1; CHECK(config_validar(&c) == CFG_ERR_MUCHOS_BLOQUES);
    c = base(); c.mecanismo = ASIG_INVALIDA; CHECK(config_validar(&c) == CFG_ERR_MECANISMO);
    c = base(); c.archivo_min_kb = 0;    CHECK(config_validar(&c) == CFG_ERR_ARCHIVO_MIN);
    c = base(); c.archivo_min_kb = 50; c.archivo_max_kb = 10;
    CHECK(config_validar(&c) == CFG_ERR_ARCHIVO_MAX_MENOR);
    c = base(); c.archivo_max_kb = 100 * 1024 + 1;
    CHECK(config_validar(&c) == CFG_ERR_ARCHIVO_MAX_DISCO);
    c = base(); c.archivo_min_kb = c.archivo_max_kb = 5;
    CHECK(config_validar(&c) == CFG_OK);   /* min == max es valido */
}

static void test_lectura(void)
{
    config_t c;
    printf("test_lectura\n");

    CHECK(leer_desde_texto("100\n4\ncontigua\n10\n100\n", &c) == 0);
    CHECK(c.total_bloques == 25600 && c.mecanismo == ASIG_CONTIGUA);

    /* Reintentos: cada linea mala debe ser rechazada y repetida. */
    CHECK(leer_desde_texto("abc\n-5\n0\n100\n3\n0\n4\nfat\n2\n0\n10\n5\n100\n", &c) == 0);
    CHECK(c.disco_mb == 100 && c.bloque_kb == 4 && c.mecanismo == ASIG_ENLAZADA);
    CHECK(c.archivo_min_kb == 10 && c.archivo_max_kb == 100);

    CHECK(leer_desde_texto("", &c) == -1);                    /* EOF inmediato */
    CHECK(leer_desde_texto("100\n4\ncontigua\n", &c) == -1);  /* EOF a medias */
}

int main(void)
{
    test_parseo();
    test_validacion();
    test_lectura();
    printf("\n%d comprobaciones, %d fallos\n", total, fallos);
    return fallos == 0 ? 0 : 1;
}
