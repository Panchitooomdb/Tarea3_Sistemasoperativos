#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>
#include <stdio.h>

/* Limites de validacion (decisiones de diseno, NO del enunciado). */
#define CFG_DISCO_MB_MIN        1u
#define CFG_DISCO_MB_MAX        65536u      /* 64 GB */
#define CFG_BLOQUE_KB_MIN       1u
#define CFG_BLOQUE_KB_MAX       1024u       /* 1 MB */
#define CFG_TOTAL_BLOQUES_MIN   4u
#define CFG_TOTAL_BLOQUES_MAX   4194304u    /* 4M bloques: acota la RAM */

typedef enum {
    ASIG_INVALIDA = 0,
    ASIG_CONTIGUA = 1,
    ASIG_ENLAZADA = 2,
    ASIG_INDEXADA = 3
} mecanismo_t;

typedef enum {
    CFG_OK = 0,
    CFG_ERR_DISCO_RANGO,
    CFG_ERR_BLOQUE_RANGO,
    CFG_ERR_NO_DIVISIBLE,
    CFG_ERR_POCOS_BLOQUES,
    CFG_ERR_MUCHOS_BLOQUES,
    CFG_ERR_MECANISMO,
    CFG_ERR_ARCHIVO_MIN,
    CFG_ERR_ARCHIVO_MAX_MENOR,
    CFG_ERR_ARCHIVO_MAX_DISCO
} cfg_estado_t;

typedef struct {
    /* Parametros ingresados por el usuario */
    uint64_t    disco_mb;
    uint64_t    bloque_kb;
    mecanismo_t mecanismo;
    uint64_t    archivo_min_kb;
    uint64_t    archivo_max_kb;
    unsigned    semilla;

    /* Valores derivados (los calcula config_validar) */
    uint64_t    disco_bytes;
    uint64_t    bloque_bytes;
    uint64_t    total_bloques;
} config_t;

/* Convierte texto a uint64_t. Devuelve 0 si es valido, -1 si no
 * (vacio, negativo, letras, desbordamiento). */
int config_parsear_u64(const char *texto, uint64_t *salida);

/* Acepta "contigua"/"enlazada"/"indexada" (sin importar mayusculas) o 1/2/3. */
mecanismo_t config_parsear_mecanismo(const char *texto);

const char *config_nombre_mecanismo(mecanismo_t m);
const char *config_mensaje(cfg_estado_t estado);

/* Valida todos los parametros y calcula los valores derivados. */
cfg_estado_t config_validar(config_t *cfg);

/* Pide los parametros por 'in', reintentando ante errores.
 * Devuelve 0 si la config quedo valida, -1 si se llego a EOF. */
int config_leer(config_t *cfg, FILE *in, FILE *out);

void config_imprimir(const config_t *cfg, FILE *out);

#endif
