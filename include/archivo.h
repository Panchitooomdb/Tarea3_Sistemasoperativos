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

    /* Asignacion enlazada: primer bloque de la cadena. El resto se alcanza
     * siguiendo los punteros guardados en el disco (disco_siguiente). */
    uint32_t primero;
} archivo_t;

#endif
