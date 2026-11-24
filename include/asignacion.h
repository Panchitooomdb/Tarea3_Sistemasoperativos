#ifndef ASIGNACION_H
#define ASIGNACION_H

#include <stdint.h>

/* Resultado comun a los tres algoritmos de asignacion. */
typedef enum {
    ASIG_RES_OK = 0,
    ASIG_RES_ERR_PARAM,          /* parametros invalidos (tam 0, id 0, NULL...) */
    ASIG_RES_ERR_SIN_ESPACIO,    /* faltan bloques libres EN TOTAL */
    ASIG_RES_ERR_FRAGMENTACION,  /* hay suficientes libres, pero no contiguos */
    ASIG_RES_ERR_INTERNO         /* inconsistencia inesperada (no deberia ocurrir) */
} asig_resultado_t;

/* Resultado de localizar una posicion logica de un archivo. */
typedef struct {
    uint64_t pos_bytes;       /* posicion logica pedida, en bytes */
    uint32_t bloque_logico;   /* indice del bloque dentro del archivo (0 = primero) */
    uint32_t bloque_fisico;   /* numero de bloque en el disco */
    uint32_t accesos;         /* lecturas de bloques de disco necesarias */
} acceso_t;

const char *asig_mensaje(asig_resultado_t r);

#endif
