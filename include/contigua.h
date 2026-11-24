#ifndef CONTIGUA_H
#define CONTIGUA_H

#include <stdint.h>

#include "archivo.h"
#include "asignacion.h"
#include "disco.h"

/* Bloques necesarios = ceil(tam_bytes / bloque_bytes). 0 si algun argumento es 0.
 * Devuelve uint64_t para no desbordar con tamanos enormes. */
uint64_t contigua_bloques_necesarios(uint64_t tam_bytes, uint64_t bloque_bytes);

/* Busca (primer ajuste / first-fit) n bloques libres consecutivos.
 * 0 si los encuentra (inicio en *inicio), -1 si no existe tal secuencia. */
int contigua_buscar(const disco_t *d, uint32_t n, uint32_t *inicio);

/* Asigna un archivo de tam_bytes en bloques consecutivos.
 *  - Si falla, el disco y *a quedan SIN modificar.
 *  - ASIG_RES_ERR_SIN_ESPACIO: libres totales < bloques necesarios.
 *  - ASIG_RES_ERR_FRAGMENTACION: libres totales alcanzan, pero ninguna
 *    secuencia consecutiva es lo bastante larga. */
asig_resultado_t contigua_asignar(disco_t *d, archivo_t *a, uint32_t id,
                                  uint64_t tam_bytes, uint64_t bloque_bytes);

/* Libera todos los bloques del archivo y deja *a en cero.
 * 0 si ok (o si no tenia bloques), -1 si encontro bloques inconsistentes. */
int contigua_liberar(disco_t *d, archivo_t *a);

/* Localiza la posicion logica pos_bytes. Accesos = 1: la direccion fisica
 * se calcula como inicio + bloque_logico, y solo se lee ese bloque.
 * 0 si ok, -1 si la posicion esta fuera del archivo o hay parametros invalidos. */
int contigua_localizar(const archivo_t *a, uint64_t pos_bytes,
                       uint64_t bloque_bytes, acceso_t *out);

#endif
