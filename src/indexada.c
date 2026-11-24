#include "indexada.h"

#include <stdlib.h>

uint64_t indexada_refs_por_indice(uint64_t bloque_bytes)
{
    uint64_t entradas = bloque_bytes / INDEXADA_REF_BYTES;
    return entradas > 1 ? entradas - 1 : 0;      /* 1 entrada se reserva para el enlace */
}

uint64_t indexada_bloques_datos(uint64_t tam_bytes, uint64_t bloque_bytes)
{
    if (tam_bytes == 0 || bloque_bytes == 0)
        return 0;
    return tam_bytes / bloque_bytes + (tam_bytes % bloque_bytes != 0);
}

uint64_t indexada_bloques_indice(uint64_t bloques_datos, uint64_t refs_por_indice)
{
    if (bloques_datos == 0 || refs_por_indice == 0)
        return 0;
    return bloques_datos / refs_por_indice + (bloques_datos % refs_por_indice != 0);
}

uint64_t indexada_bloques_necesarios(uint64_t tam_bytes, uint64_t bloque_bytes)
{
    uint64_t r = indexada_refs_por_indice(bloque_bytes);
    uint64_t datos;

    if (tam_bytes == 0 || r == 0)
        return 0;
    datos = indexada_bloques_datos(tam_bytes, bloque_bytes);
    /* r > 0 implica bloque >= 8 bytes, asi datos <= 2^61 y datos + indices no desborda */
    return datos + indexada_bloques_indice(datos, r);
}

/* Rollback: libera los bloques indice ya encadenados y los de datos ya registrados. */
static void deshacer(disco_t *d, uint32_t primero, uint32_t n_idx,
                     const uint32_t *refs, uint32_t n_datos)
{
    uint32_t cur = primero, sig, k;

    for (k = 0; k < n_datos; k++)
        disco_liberar(d, refs[k]);
    for (k = 0; k < n_idx && cur != DISCO_FIN; k++) {
        sig = disco_siguiente(d, cur);
        disco_liberar(d, cur);
        cur = sig;
    }
}

asig_resultado_t indexada_asignar(disco_t *d, archivo_t *a, uint32_t id,
                                  uint64_t tam_bytes, uint64_t bloque_bytes)
{
    uint64_t r = indexada_refs_por_indice(bloque_bytes);
    uint64_t datos, indices;
    uint32_t n_datos, n_idx, r32, i;
    uint32_t asig_datos = 0, asig_idx = 0, restantes = 0;
    uint32_t prev = DISCO_FIN, primero = DISCO_FIN;
    uint32_t *refs;

    if (d == NULL || a == NULL || id == DISCO_LIBRE || tam_bytes == 0 || r == 0)
        return ASIG_RES_ERR_PARAM;

    datos = indexada_bloques_datos(tam_bytes, bloque_bytes);
    indices = indexada_bloques_indice(datos, r);

    /* Hay que poder reservar datos E indices: el indice tambien consume espacio. */
    if (datos + indices > disco_libres(d))
        return ASIG_RES_ERR_SIN_ESPACIO;

    n_datos = (uint32_t)datos;
    n_idx = (uint32_t)indices;
    r32 = r > UINT32_MAX ? UINT32_MAX : (uint32_t)r;

    refs = malloc((size_t)n_datos * sizeof *refs);
    if (refs == NULL)
        return ASIG_RES_ERR_INTERNO;

    /* Recorrido unico: cuando un grupo esta completo, el siguiente bloque libre es un
     * bloque INDICE; los 'restantes' bloques libres que siguen son sus datos. */
    for (i = 0; i < d->total_bloques && (asig_datos < n_datos || restantes > 0); i++) {
        if (!disco_esta_libre(d, i))
            continue;
        if (disco_ocupar(d, i, id) != 0) {
            deshacer(d, primero, asig_idx, refs, asig_datos);
            free(refs);
            return ASIG_RES_ERR_INTERNO;
        }
        if (restantes == 0) {                               /* nuevo bloque indice */
            if (prev == DISCO_FIN) {
                primero = i;
            } else if (disco_set_siguiente(d, prev, i) != 0) {
                disco_liberar(d, i);
                deshacer(d, primero, asig_idx, refs, asig_datos);
                free(refs);
                return ASIG_RES_ERR_INTERNO;
            }
            prev = i;
            asig_idx++;
            restantes = (n_datos - asig_datos < r32) ? n_datos - asig_datos : r32;
        } else {                                            /* bloque de datos */
            refs[asig_datos++] = i;
            restantes--;
        }
    }

    if (asig_datos != n_datos || asig_idx != n_idx) {       /* no deberia ocurrir */
        deshacer(d, primero, asig_idx, refs, asig_datos);
        free(refs);
        return ASIG_RES_ERR_INTERNO;
    }

    a->id = id;
    a->tam_bytes = tam_bytes;
    a->bloques_datos = n_datos;
    a->bloques_total = n_datos + n_idx;
    a->primero = primero;
    a->n_indices = n_idx;
    a->refs = refs;
    a->inicio = 0;
    a->largo = 0;
    return ASIG_RES_OK;
}

int indexada_liberar(disco_t *d, archivo_t *a)
{
    uint32_t cur, sig, k, b;
    int estado = 0;

    if (d == NULL || a == NULL)
        return -1;

    if (a->bloques_total == 0) {          /* archivo vacio o ya liberado */
        free(a->refs);
        a->refs = NULL;
        return 0;
    }

    /* bloques de datos: solo se liberan los que de verdad son del archivo */
    for (k = 0; k < a->bloques_datos; k++) {
        b = a->refs != NULL ? a->refs[k] : DISCO_FIN;
        if (disco_bloque_valido(d, b) && disco_dueno(d, b) == a->id)
            disco_liberar(d, b);
        else
            estado = -1;
    }

    /* bloques indice: leer el enlace ANTES de liberar cada uno */
    cur = a->primero;
    for (k = 0; k < a->n_indices; k++) {
        if (!disco_bloque_valido(d, cur) || disco_dueno(d, cur) != a->id) {
            estado = -1;
            break;
        }
        sig = disco_siguiente(d, cur);
        disco_liberar(d, cur);
        cur = sig;
    }
    if (estado == 0 && cur != DISCO_FIN)
        estado = -1;

    free(a->refs);
    a->refs = NULL;
    a->id = 0;
    a->tam_bytes = 0;
    a->bloques_datos = 0;
    a->bloques_total = 0;
    a->primero = DISCO_FIN;
    a->n_indices = 0;
    a->inicio = 0;
    a->largo = 0;
    return estado;
}

int indexada_localizar(const disco_t *d, const archivo_t *a, uint64_t pos_bytes,
                       uint64_t bloque_bytes, acceso_t *out)
{
    uint64_t r = indexada_refs_por_indice(bloque_bytes);
    uint64_t logico, q, k;
    uint32_t cur, dato;

    if (d == NULL || a == NULL || out == NULL || r == 0 ||
        bloque_bytes == 0 || a->bloques_total == 0 || a->refs == NULL)
        return -1;
    if (pos_bytes >= a->tam_bytes)
        return -1;

    logico = pos_bytes / bloque_bytes;     /* aqui el bloque ENTERO guarda datos */
    if (logico >= a->bloques_datos)
        return -1;
    q = logico / r;                        /* indice que contiene la referencia */
    if (q >= a->n_indices)
        return -1;

    cur = a->primero;
    for (k = 0; k < q; k++) {              /* cada salto = leer un indice y seguir su enlace */
        if (!disco_bloque_valido(d, cur) || disco_dueno(d, cur) != a->id)
            return -1;
        cur = disco_siguiente(d, cur);
    }
    if (!disco_bloque_valido(d, cur) || disco_dueno(d, cur) != a->id)
        return -1;                         /* se lee el indice q (la lectura numero q+1) */

    dato = a->refs[logico];                /* entrada (logico % r) del indice q */
    if (!disco_bloque_valido(d, dato) || disco_dueno(d, dato) != a->id)
        return -1;

    out->pos_bytes = pos_bytes;
    out->bloque_logico = (uint32_t)logico;
    out->bloque_fisico = dato;
    out->accesos = (uint32_t)q + 1 + 1;    /* (q+1) indices + 1 bloque de datos */
    return 0;
}

int indexada_verificar(const disco_t *d, const archivo_t *a, uint64_t bloque_bytes)
{
    uint64_t r = indexada_refs_por_indice(bloque_bytes);
    uint32_t cur, k;

    if (d == NULL || a == NULL || r == 0 || a->bloques_total == 0 || a->refs == NULL)
        return -1;
    if (a->bloques_total != a->bloques_datos + a->n_indices)
        return -1;
    if (a->n_indices != indexada_bloques_indice(a->bloques_datos, r))
        return -1;

    cur = a->primero;
    for (k = 0; k < a->n_indices; k++) {
        if (!disco_bloque_valido(d, cur) || disco_dueno(d, cur) != a->id)
            return -1;
        cur = disco_siguiente(d, cur);
    }
    if (cur != DISCO_FIN)
        return -1;                         /* ciclo o cadena mas larga de lo declarado */

    for (k = 0; k < a->bloques_datos; k++)
        if (!disco_bloque_valido(d, a->refs[k]) || disco_dueno(d, a->refs[k]) != a->id)
            return -1;
    return 0;
}
