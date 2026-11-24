#ifndef ARCHIVO_H
#define ARCHIVO_H

#include <stdint.h>

/* Un archivo simulado. Solo existe en memoria: no hay datos reales.
 * Las siguientes etapas agregaran los campos de enlazada e indexada. */
typedef struct {
    uint32_t id;             /* identificador unico (>= 1) */
    uint64_t tam_bytes;      /* tamano logico */
    uint32_t bloques_datos;  /* bloques necesarios para los datos */
    uint32_t bloques_total;  /* bloques reservados en el disco (datos + extras) */

    /* Asignacion contigua: el archivo ocupa [inicio, inicio + largo) */
    uint32_t inicio;
    uint32_t largo;

    /* Enlazada: primer bloque de la cadena. Indexada: primer bloque INDICE.
     * El resto se alcanza siguiendo los punteros del disco (disco_siguiente). */
    uint32_t primero;

    /* Asignacion indexada. refs[j] = bloque fisico del bloque logico j, es decir,
     * el contenido de los bloques indice. Es memoria dinamica del archivo: se
     * libera SIEMPRE con indexada_liberar (no copiar el struct y liberar dos veces). */
    uint32_t  n_indices;   /* cantidad de bloques indice */
    uint32_t *refs;
} archivo_t;

#endif
