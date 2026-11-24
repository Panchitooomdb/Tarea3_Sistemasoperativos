#include "contigua.h"

uint64_t contigua_bloques_necesarios(uint64_t tam_bytes, uint64_t bloque_bytes)
{
    if (tam_bytes == 0 || bloque_bytes == 0)
        return 0;
    /* forma que no desborda: equivale a ceil(tam / bloque) */
    return tam_bytes / bloque_bytes + (tam_bytes % bloque_bytes != 0);
}

int contigua_buscar(const disco_t *d, uint32_t n, uint32_t *inicio)
{
    uint32_t i, racha = 0;

    if (d == NULL || inicio == NULL || n == 0)
        return -1;

    for (i = 0; i < d->total_bloques; i++) {
        if (d->dueno[i] == DISCO_LIBRE) {
            racha++;
            if (racha == n) {
                *inicio = i - n + 1;
                return 0;
            }
        } else {
            racha = 0;
        }
    }
    return -1;
}

asig_resultado_t contigua_asignar(disco_t *d, archivo_t *a, uint32_t id,
                                  uint64_t tam_bytes, uint64_t bloque_bytes)
{
    uint64_t n;
    uint32_t inicio, j;

    if (d == NULL || a == NULL || id == DISCO_LIBRE || tam_bytes == 0 || bloque_bytes == 0)
        return ASIG_RES_ERR_PARAM;

    n = contigua_bloques_necesarios(tam_bytes, bloque_bytes);

    /* 1) Falta de espacio total: ni siquiera sumando todos los libres alcanza. */
    if (n > disco_libres(d))
        return ASIG_RES_ERR_SIN_ESPACIO;

    /* 2) Alcanzan en total, pero se necesita una secuencia consecutiva. */
    if (contigua_buscar(d, (uint32_t)n, &inicio) != 0)
        return ASIG_RES_ERR_FRAGMENTACION;

    /* 3) Reservar. Si algo falla (no deberia), se deshace lo hecho. */
    for (j = 0; j < (uint32_t)n; j++) {
        if (disco_ocupar(d, inicio + j, id) != 0) {
            while (j > 0)
                disco_liberar(d, inicio + --j);
            return ASIG_RES_ERR_INTERNO;
        }
    }

    a->id = id;
    a->tam_bytes = tam_bytes;
    a->bloques_datos = (uint32_t)n;
    a->bloques_total = (uint32_t)n;   /* contigua no usa bloques extra */
    a->inicio = inicio;
    a->largo = (uint32_t)n;
    return ASIG_RES_OK;
}

int contigua_liberar(disco_t *d, archivo_t *a)
{
    uint32_t j;
    int estado = 0;

    if (d == NULL || a == NULL)
        return -1;

    for (j = 0; j < a->largo; j++) {
        /* solo se libera un bloque si de verdad pertenece a este archivo */
        if (disco_dueno(d, a->inicio + j) != a->id ||
            disco_liberar(d, a->inicio + j) != 0)
            estado = -1;
    }

    a->id = 0;
    a->tam_bytes = 0;
    a->bloques_datos = 0;
    a->bloques_total = 0;
    a->inicio = 0;
    a->largo = 0;
    return estado;
}

int contigua_localizar(const archivo_t *a, uint64_t pos_bytes,
                       uint64_t bloque_bytes, acceso_t *out)
{
    uint64_t logico;

    if (a == NULL || out == NULL || bloque_bytes == 0 || a->largo == 0)
        return -1;
    if (pos_bytes >= a->tam_bytes)
        return -1;

    logico = pos_bytes / bloque_bytes;
    if (logico >= a->largo)
        return -1;

    out->pos_bytes = pos_bytes;
    out->bloque_logico = (uint32_t)logico;
    out->bloque_fisico = a->inicio + (uint32_t)logico;
    out->accesos = 1;
    return 0;
}
