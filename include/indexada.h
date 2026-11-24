#ifndef INDEXADA_H
#define INDEXADA_H

#include <stdint.h>

#include "archivo.h"
#include "asignacion.h"
#include "disco.h"

/* Tamano de cada referencia (numero de bloque) dentro de un bloque indice.
 * (Decision de diseno: 4 bytes, igual que el puntero de la enlazada.) */
#define INDEXADA_REF_BYTES 4u

/* Referencias a datos que caben en UN bloque indice. Si un archivo necesita mas,
 * se usan varios bloques indice ENCADENADOS: la ultima entrada de cada indice
 * se reserva para apuntar al siguiente. Por eso:
 *     refs_por_indice = bloque_bytes / 4 - 1.      0 si el bloque es demasiado chico. */
uint64_t indexada_refs_por_indice(uint64_t bloque_bytes);

/* Bloques de datos = ceil(tam / bloque_bytes): aqui el bloque entero guarda datos. */
uint64_t indexada_bloques_datos(uint64_t tam_bytes, uint64_t bloque_bytes);

/* Bloques indice = ceil(bloques_datos / refs_por_indice). */
uint64_t indexada_bloques_indice(uint64_t bloques_datos, uint64_t refs_por_indice);

/* Total a reservar = datos + indices. 0 si algun argumento es invalido. */
uint64_t indexada_bloques_necesarios(uint64_t tam_bytes, uint64_t bloque_bytes);

/* Reserva los primeros bloques libres (sin exigir continuidad). Cada grupo queda como
 * [bloque indice][sus bloques de datos]. Nunca falla por fragmentacion: solo por falta
 * de espacio TOTAL (datos + indices). Si falla, disco y *a quedan sin modificar. */
asig_resultado_t indexada_asignar(disco_t *d, archivo_t *a, uint32_t id,
                                  uint64_t tam_bytes, uint64_t bloque_bytes);

/* Libera bloques de datos e indices, libera la memoria de refs y deja *a en cero.
 * Solo libera bloques que pertenecen al archivo. 0 si ok, -1 si encontro inconsistencias. */
int indexada_liberar(disco_t *d, archivo_t *a);

/* Localiza pos_bytes. Accesos = (bloques indice leidos) + 1 bloque de datos:
 * hay que recorrer la cadena de indices hasta el que contiene la referencia
 * (q + 1 lecturas, con q = bloque_logico / refs_por_indice) y luego leer el dato.
 * 0 si ok, -1 si pos esta fuera del archivo o las estructuras son invalidas. */
int indexada_localizar(const disco_t *d, const archivo_t *a, uint64_t pos_bytes,
                       uint64_t bloque_bytes, acceso_t *out);

/* Comprueba cadena de indices, cantidad de indices y duenyo de cada referencia.
 * 0 si es consistente, -1 si no. */
int indexada_verificar(const disco_t *d, const archivo_t *a, uint64_t bloque_bytes);

#endif
