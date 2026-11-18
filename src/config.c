#include "config.h"

#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

/* ---------- Parseo ---------- */

int config_parsear_u64(const char *texto, uint64_t *salida)
{
    char *fin = NULL;
    unsigned long long v;

    if (texto == NULL || salida == NULL)
        return -1;

    while (isspace((unsigned char)*texto))
        texto++;
    /* strtoull acepta '-' y lo "envuelve": hay que rechazarlo a mano. */
    if (*texto == '\0' || *texto == '-' || *texto == '+')
        return -1;

    errno = 0;
    v = strtoull(texto, &fin, 10);
    if (errno == ERANGE || fin == texto)
        return -1;
    while (isspace((unsigned char)*fin))
        fin++;
    if (*fin != '\0')
        return -1;  /* basura al final: "12abc" */

    *salida = (uint64_t)v;
    return 0;
}

static int igual_ignorando_caso(const char *a, const char *b)
{
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b))
            return 0;
        a++;
        b++;
    }
    return *a == '\0' && *b == '\0';
}

mecanismo_t config_parsear_mecanismo(const char *texto)
{
    char buf[32];
    size_t i = 0, n;

    if (texto == NULL)
        return ASIG_INVALIDA;
    while (isspace((unsigned char)*texto))
        texto++;
    n = strlen(texto);
    while (n > 0 && isspace((unsigned char)texto[n - 1]))
        n--;
    if (n == 0 || n >= sizeof buf)
        return ASIG_INVALIDA;
    for (i = 0; i < n; i++)
        buf[i] = texto[i];
    buf[n] = '\0';

    if (igual_ignorando_caso(buf, "contigua") || strcmp(buf, "1") == 0)
        return ASIG_CONTIGUA;
    if (igual_ignorando_caso(buf, "enlazada") || strcmp(buf, "2") == 0)
        return ASIG_ENLAZADA;
    if (igual_ignorando_caso(buf, "indexada") || strcmp(buf, "3") == 0)
        return ASIG_INDEXADA;
    return ASIG_INVALIDA;
}

const char *config_nombre_mecanismo(mecanismo_t m)
{
    switch (m) {
    case ASIG_CONTIGUA: return "contigua";
    case ASIG_ENLAZADA: return "enlazada";
    case ASIG_INDEXADA: return "indexada";
    default:            return "invalida";
    }
}

const char *config_mensaje(cfg_estado_t e)
{
    switch (e) {
    case CFG_OK:                  return "configuracion valida";
    case CFG_ERR_DISCO_RANGO:     return "el tamano del disco esta fuera de rango";
    case CFG_ERR_BLOQUE_RANGO:    return "el tamano de bloque esta fuera de rango";
    case CFG_ERR_NO_DIVISIBLE:    return "el disco no es multiplo exacto del tamano de bloque";
    case CFG_ERR_POCOS_BLOQUES:   return "el disco tendria muy pocos bloques";
    case CFG_ERR_MUCHOS_BLOQUES:  return "el disco tendria demasiados bloques";
    case CFG_ERR_MECANISMO:       return "mecanismo de asignacion invalido";
    case CFG_ERR_ARCHIVO_MIN:     return "el tamano minimo de archivo debe ser >= 1 KB";
    case CFG_ERR_ARCHIVO_MAX_MENOR: return "el tamano maximo es menor que el minimo";
    case CFG_ERR_ARCHIVO_MAX_DISCO: return "el tamano maximo de archivo excede el disco";
    }
    return "error desconocido";
}

/* ---------- Validacion ---------- */

cfg_estado_t config_validar(config_t *cfg)
{
    uint64_t disco_kb;

    if (cfg->disco_mb < CFG_DISCO_MB_MIN || cfg->disco_mb > CFG_DISCO_MB_MAX)
        return CFG_ERR_DISCO_RANGO;
    if (cfg->bloque_kb < CFG_BLOQUE_KB_MIN || cfg->bloque_kb > CFG_BLOQUE_KB_MAX)
        return CFG_ERR_BLOQUE_RANGO;
    if (cfg->mecanismo != ASIG_CONTIGUA && cfg->mecanismo != ASIG_ENLAZADA &&
        cfg->mecanismo != ASIG_INDEXADA)
        return CFG_ERR_MECANISMO;

    /* Seguro: disco_mb <= 65536 y bloque_kb <= 1024, no hay desbordamiento. */
    disco_kb = cfg->disco_mb * 1024u;
    if (disco_kb % cfg->bloque_kb != 0)
        return CFG_ERR_NO_DIVISIBLE;

    cfg->total_bloques = disco_kb / cfg->bloque_kb;
    if (cfg->total_bloques < CFG_TOTAL_BLOQUES_MIN)
        return CFG_ERR_POCOS_BLOQUES;
    if (cfg->total_bloques > CFG_TOTAL_BLOQUES_MAX)
        return CFG_ERR_MUCHOS_BLOQUES;

    if (cfg->archivo_min_kb < 1)
        return CFG_ERR_ARCHIVO_MIN;
    if (cfg->archivo_max_kb < cfg->archivo_min_kb)
        return CFG_ERR_ARCHIVO_MAX_MENOR;
    if (cfg->archivo_max_kb > disco_kb)
        return CFG_ERR_ARCHIVO_MAX_DISCO;

    cfg->disco_bytes  = disco_kb * 1024u;
    cfg->bloque_bytes = cfg->bloque_kb * 1024u;
    return CFG_OK;
}

/* ---------- Lectura interactiva ---------- */

/* Lee una linea. Devuelve 0 ok, -1 en EOF. Descarta el resto si es muy larga. */
static int leer_linea(FILE *in, char *buf, size_t tam)
{
    if (fgets(buf, (int)tam, in) == NULL)
        return -1;
    if (strchr(buf, '\n') == NULL && !feof(in)) {
        int c;
        while ((c = fgetc(in)) != '\n' && c != EOF)
            ;
    }
    return 0;
}

/* Pide un numero hasta que sea valido. */
static int pedir_u64(FILE *in, FILE *out, const char *prompt, uint64_t *v)
{
    char linea[128];

    for (;;) {
        fprintf(out, "%s", prompt);
        fflush(out);
        if (leer_linea(in, linea, sizeof linea) != 0)
            return -1;
        if (config_parsear_u64(linea, v) == 0)
            return 0;
        fprintf(out, "  Error: ingrese un entero positivo valido.\n");
    }
}

int config_leer(config_t *cfg, FILE *in, FILE *out)
{
    char linea[128];
    cfg_estado_t e;

    for (;;) {
        if (pedir_u64(in, out, "Tamano del disco (MB): ", &cfg->disco_mb) != 0)
            return -1;
        if (cfg->disco_mb >= CFG_DISCO_MB_MIN && cfg->disco_mb <= CFG_DISCO_MB_MAX)
            break;
        fprintf(out, "  Error: %s (permitido %u..%u MB).\n",
                config_mensaje(CFG_ERR_DISCO_RANGO), CFG_DISCO_MB_MIN, CFG_DISCO_MB_MAX);
    }

    for (;;) {
        uint64_t disco_kb = cfg->disco_mb * 1024u;
        if (pedir_u64(in, out, "Tamano de bloque (KB): ", &cfg->bloque_kb) != 0)
            return -1;
        if (cfg->bloque_kb < CFG_BLOQUE_KB_MIN || cfg->bloque_kb > CFG_BLOQUE_KB_MAX) {
            fprintf(out, "  Error: %s (permitido %u..%u KB).\n",
                    config_mensaje(CFG_ERR_BLOQUE_RANGO), CFG_BLOQUE_KB_MIN, CFG_BLOQUE_KB_MAX);
            continue;
        }
        if (disco_kb % cfg->bloque_kb != 0) {
            fprintf(out, "  Error: %s.\n", config_mensaje(CFG_ERR_NO_DIVISIBLE));
            continue;
        }
        if (disco_kb / cfg->bloque_kb < CFG_TOTAL_BLOQUES_MIN) {
            fprintf(out, "  Error: %s (minimo %u).\n",
                    config_mensaje(CFG_ERR_POCOS_BLOQUES), CFG_TOTAL_BLOQUES_MIN);
            continue;
        }
        if (disco_kb / cfg->bloque_kb > CFG_TOTAL_BLOQUES_MAX) {
            fprintf(out, "  Error: %s (maximo %u).\n",
                    config_mensaje(CFG_ERR_MUCHOS_BLOQUES), CFG_TOTAL_BLOQUES_MAX);
            continue;
        }
        break;
    }

    for (;;) {
        fprintf(out, "Mecanismo (contigua/enlazada/indexada o 1/2/3): ");
        fflush(out);
        if (leer_linea(in, linea, sizeof linea) != 0)
            return -1;
        cfg->mecanismo = config_parsear_mecanismo(linea);
        if (cfg->mecanismo != ASIG_INVALIDA)
            break;
        fprintf(out, "  Error: %s.\n", config_mensaje(CFG_ERR_MECANISMO));
    }

    for (;;) {
        if (pedir_u64(in, out, "Tamano minimo de archivo (KB): ", &cfg->archivo_min_kb) != 0)
            return -1;
        if (cfg->archivo_min_kb >= 1 && cfg->archivo_min_kb <= cfg->disco_mb * 1024u)
            break;
        fprintf(out, "  Error: debe estar entre 1 KB y el tamano del disco.\n");
    }

    for (;;) {
        if (pedir_u64(in, out, "Tamano maximo de archivo (KB): ", &cfg->archivo_max_kb) != 0)
            return -1;
        if (cfg->archivo_max_kb < cfg->archivo_min_kb) {
            fprintf(out, "  Error: %s.\n", config_mensaje(CFG_ERR_ARCHIVO_MAX_MENOR));
            continue;
        }
        if (cfg->archivo_max_kb > cfg->disco_mb * 1024u) {
            fprintf(out, "  Error: %s.\n", config_mensaje(CFG_ERR_ARCHIVO_MAX_DISCO));
            continue;
        }
        break;
    }

    /* Validacion final y calculo de derivados: red de seguridad. */
    e = config_validar(cfg);
    if (e != CFG_OK) {
        fprintf(out, "Error interno de validacion: %s\n", config_mensaje(e));
        return -1;
    }
    return 0;
}

void config_imprimir(const config_t *cfg, FILE *out)
{
    fprintf(out, "\n=== Configuracion de la simulacion ===\n");
    fprintf(out, "Disco             : %" PRIu64 " MB (%" PRIu64 " bytes)\n",
            cfg->disco_mb, cfg->disco_bytes);
    fprintf(out, "Bloque            : %" PRIu64 " KB (%" PRIu64 " bytes)\n",
            cfg->bloque_kb, cfg->bloque_bytes);
    fprintf(out, "Total de bloques  : %" PRIu64 "\n", cfg->total_bloques);
    fprintf(out, "Mecanismo         : %s\n", config_nombre_mecanismo(cfg->mecanismo));
    fprintf(out, "Archivos (rango)  : %" PRIu64 " KB .. %" PRIu64 " KB\n",
            cfg->archivo_min_kb, cfg->archivo_max_kb);
    fprintf(out, "Semilla aleatoria : %u\n", cfg->semilla);
    fprintf(out, "======================================\n\n");
}
