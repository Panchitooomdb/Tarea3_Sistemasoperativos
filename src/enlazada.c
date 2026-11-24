#include "enlazada.h"

uint64_t enlazada_capacidad_util(uint64_t bloque_bytes)
{
    return bloque_bytes > ENLAZADA_PUNTERO_BYTES ? bloque_bytes - ENLAZADA_PUNTERO_BYTES : 0;
}

uint64_t enlazada_bloques_necesarios(uint64_t tam_bytes, uint64_t bloque_bytes)
{
    uint64_t util = enlazada_capacidad_util(bloque_bytes);

    if (tam_bytes == 0 || util == 0)
        return 0;
    return tam_bytes / util + (tam_bytes % util != 0);   /* ceil sin desbordar */
}

/* Libera los primeros 'cantidad' bloques de una cadena parcial (rollback). */
static void deshacer(disco_t *d, uint32_t primero, uint32_t cantidad)
{
    uint32_t cur = primero, sig, k;

    for (k = 0; k < cantidad && cur != DISCO_FIN; k++) {
        sig = disco_siguiente(d, cur);
        disco_liberar(d, cur);
        cur = sig;
    }
}

asig_resultado_t enlazada_asignar(disco_t *d, archivo_t *a, uint32_t id,
                                  uint64_t tam_bytes, uint64_t bloque_bytes)
{
    uint64_t n;
    uint32_t i, primero = DISCO_FIN, anterior = DISCO_FIN, asignados = 0;

    if (d == NULL || a == NULL || id == DISCO_LIBRE || tam_bytes == 0 ||
        enlazada_capacidad_util(bloque_bytes) == 0)
        return ASIG_RES_ERR_PARAM;

    n = enlazada_bloques_necesarios(tam_bytes, bloque_bytes);

    /* No hay fragmentacion externa aqui: basta con que alcancen los libres. */
    if (n > disco_libres(d))
        return ASIG_RES_ERR_SIN_ESPACIO;

    /* Tomar los primeros n bloques libres y encadenarlos en orden. */
    for (i = 0; i < d->total_bloques && asignados < n; i++) {
        if (!disco_esta_libre(d, i))
            continue;
        if (disco_ocupar(d, i, id) != 0) {
            deshacer(d, primero, asignados);
            return ASIG_RES_ERR_INTERNO;
        }
        if (anterior == DISCO_FIN) {
            primero = i;
        } else if (disco_set_siguiente(d, anterior, i) != 0) {
            disco_liberar(d, i);
            deshacer(d, primero, asignados);
            return ASIG_RES_ERR_INTERNO;
        }
        anterior = i;
        asignados++;
    }

    if (asignados != n) {                 /* no deberia ocurrir: n <= libres */
        deshacer(d, primero, asignados);
        return ASIG_RES_ERR_INTERNO;
    }

    a->id = id;
    a->tam_bytes = tam_bytes;
    a->bloques_datos = (uint32_t)n;
    a->bloques_total = (uint32_t)n;       /* los punteros viven dentro de los bloques */
    a->primero = primero;
    a->inicio = 0;
    a->largo = 0;
    return ASIG_RES_OK;
}

int enlazada_liberar(disco_t *d, archivo_t *a)
{
    uint32_t cur, sig, k;
    int estado = 0;

    if (d == NULL || a == NULL)
        return -1;
    if (a->bloques_total == 0)
        return 0;                         /* nada que liberar */

    cur = a->primero;
    for (k = 0; k < a->bloques_total; k++) {
        if (!disco_bloque_valido(d, cur) || disco_dueno(d, cur) != a->id) {
            estado = -1;                  /* cadena corrupta: no tocar bloques ajenos */
            break;
        }
        sig = disco_siguiente(d, cur);    /* leer ANTES de liberar (liberar borra el puntero) */
        disco_liberar(d, cur);
        cur = sig;
    }
    if (estado == 0 && cur != DISCO_FIN)
        estado = -1;                      /* la cadena era mas larga de lo declarado */

    a->id = 0;
    a->tam_bytes = 0;
    a->bloques_datos = 0;
    a->bloques_total = 0;
    a->primero = DISCO_FIN;
    a->inicio = 0;
    a->largo = 0;
    return estado;
}

int enlazada_localizar(const disco_t *d, const archivo_t *a, uint64_t pos_bytes,
                       uint64_t bloque_bytes, acceso_t *out)
{
    uint64_t util = enlazada_capacidad_util(bloque_bytes);
    uint64_t logico, k;
    uint32_t cur;

    if (d == NULL || a == NULL || out == NULL || util == 0 || a->bloques_total == 0)
        return -1;
    if (pos_bytes >= a->tam_bytes)
        return -1;

    logico = pos_bytes / util;            /* ojo: se divide por la capacidad UTIL */
    if (logico >= a->bloques_total)
        return -1;

    cur = a->primero;
    for (k = 0; k < logico; k++) {        /* cada salto = leer un bloque y seguir su puntero */
        if (!disco_bloque_valido(d, cur) || disco_dueno(d, cur) != a->id)
            return -1;
        cur = disco_siguiente(d, cur);
    }
    if (!disco_bloque_valido(d, cur) || disco_dueno(d, cur) != a->id)
        return -1;

    out->pos_bytes = pos_bytes;
    out->bloque_logico = (uint32_t)logico;
    out->bloque_fisico = cur;
    out->accesos = (uint32_t)logico + 1;
    return 0;
}

int enlazada_verificar(const disco_t *d, const archivo_t *a)
{
    uint32_t cur, k;

    if (d == NULL || a == NULL || a->bloques_total == 0)
        return -1;

    cur = a->primero;
    for (k = 0; k < a->bloques_total; k++) {
        if (!disco_bloque_valido(d, cur) || disco_dueno(d, cur) != a->id)
            return -1;
        cur = disco_siguiente(d, cur);
    }
    return cur == DISCO_FIN ? 0 : -1;     /* un ciclo o exceso de largo termina aqui en -1 */
}
