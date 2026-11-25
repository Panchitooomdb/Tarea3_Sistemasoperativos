#ifndef SIMULADOR_H
#define SIMULADOR_H

#include <stdint.h>

#include "archivo.h"
#include "asignacion.h"
#include "config.h"
#include "disco.h"

/* Estado completo de la simulacion: disco, lista de archivos y contadores. */
typedef struct {
    config_t   cfg;
    disco_t   *disco;

    archivo_t *archivos;      /* arreglo dinamico de archivos que existen ahora */
    uint32_t   n_archivos;
    uint32_t   capacidad;     /* espacio reservado en 'archivos' */
    uint32_t   siguiente_id;  /* id del proximo archivo a crear (parte en 1) */

    /* Contadores para las estadisticas finales */
    uint32_t   creados;
    uint32_t   eliminados;
    uint64_t   total_accesos; /* suma de accesos a disco de todas las busquedas */
    uint32_t   n_busquedas;   /* cantidad de busquedas hechas */
} simulador_t;

/* Crea el disco y deja todo en cero. 0 si ok, -1 si falla. */
int  simulador_iniciar(simulador_t *sim, const config_t *cfg);

/* Libera archivos y disco. */
void simulador_destruir(simulador_t *sim);

/* Crea un archivo de tamano aleatorio (dentro del rango de la config) y lo
 * asigna segun el mecanismo elegido. Devuelve el resultado de la asignacion;
 * si no es ASIG_RES_OK la simulacion debe terminar. */
asig_resultado_t simulador_crear_archivo(simulador_t *sim);

/* Elimina un archivo al azar. 0 si elimino, -1 si no habia archivos. */
int  simulador_eliminar_aleatorio(simulador_t *sim);

/* Accede a una posicion al azar de un archivo al azar. 0 si ok, -1 si no habia. */
int  simulador_acceder_aleatorio(simulador_t *sim);

#endif
