#ifndef ENLAZADA_H
#define ENLAZADA_H

#include <stdint.h>

#include "archivo.h"
#include "asignacion.h"
#include "disco.h"

/* Tamano del puntero simulado que cada bloque reserva para apuntar al siguiente.
 * (Decision de diseno: 4 bytes = un numero de bloque de 32 bits.) */
#define ENLAZADA_PUNTERO_BYTES 4u

/* Bytes de datos que caben en un bloque: bloque_bytes - puntero.
 * 0 si el bloque no es mayor que el puntero. */
uint64_t enlazada_capacidad_util(uint64_t bloque_bytes);

/* Bloques necesarios = ceil(tam_bytes / capacidad_util). 0 si algun argumento invalido. */
uint64_t enlazada_bloques_necesarios(uint64_t tam_bytes, uint64_t bloque_bytes);

/* Asigna un archivo usando los primeros bloques libres (sin exigir continuidad)
 * y los encadena en orden. Solo puede fallar por falta de espacio TOTAL:
 * enlazada nunca sufre fragmentacion externa. Si falla, nada cambia. */
asig_resultado_t enlazada_asignar(disco_t *d, archivo_t *a, uint32_t id,
                                  uint64_t tam_bytes, uint64_t bloque_bytes);

/* Recorre la cadena liberando cada bloque y deja *a en cero.
 * 0 si ok (o sin bloques); -1 si la cadena estaba corrupta. */
int enlazada_liberar(disco_t *d, archivo_t *a);

/* Localiza pos_bytes recorriendo la cadena desde el primer bloque.
 * bloque_logico = pos / capacidad_util; accesos = bloque_logico + 1
 * (hay que leer todos los bloques anteriores para seguir sus punteros).
 * 0 si ok, -1 si pos esta fuera del archivo o la cadena es invalida. */
int enlazada_localizar(const disco_t *d, const archivo_t *a, uint64_t pos_bytes,
                       uint64_t bloque_bytes, acceso_t *out);

/* Comprueba la cadena completa: largo declarado, dueno de cada bloque y fin de cadena.
 * 0 si es consistente, -1 si no. */
int enlazada_verificar(const disco_t *d, const archivo_t *a);

#endif
