#include "disco.h"

#include <inttypes.h>
#include <stdlib.h>

#define MAPA_COLUMNAS    64u
#define MAPA_MAX_CELDAS  2048u

disco_t *disco_crear(uint32_t total_bloques)
{
    disco_t *d;

    if (total_bloques == 0)
        return NULL;

    d = malloc(sizeof *d);
    if (d == NULL)
        return NULL;

    /* calloc deja todo en 0 == DISCO_LIBRE */
    d->dueno = calloc(total_bloques, sizeof *d->dueno);
    if (d->dueno == NULL) {
        free(d);
        return NULL;
    }
    d->total_bloques = total_bloques;
    d->libres = total_bloques;
    return d;
}

void disco_destruir(disco_t *d)
{
    if (d == NULL)
        return;
    free(d->dueno);
    free(d);
}

int disco_bloque_valido(const disco_t *d, uint32_t bloque)
{
    return d != NULL && bloque < d->total_bloques;
}

int disco_esta_libre(const disco_t *d, uint32_t bloque)
{
    return disco_bloque_valido(d, bloque) && d->dueno[bloque] == DISCO_LIBRE;
}

uint32_t disco_dueno(const disco_t *d, uint32_t bloque)
{
    if (!disco_bloque_valido(d, bloque))
        return DISCO_LIBRE;
    return d->dueno[bloque];
}

int disco_ocupar(disco_t *d, uint32_t bloque, uint32_t id_archivo)
{
    if (id_archivo == DISCO_LIBRE || !disco_esta_libre(d, bloque))
        return -1;
    d->dueno[bloque] = id_archivo;
    d->libres--;
    return 0;
}

int disco_liberar(disco_t *d, uint32_t bloque)
{
    if (!disco_bloque_valido(d, bloque) || d->dueno[bloque] == DISCO_LIBRE)
        return -1;
    d->dueno[bloque] = DISCO_LIBRE;
    d->libres++;
    return 0;
}

uint32_t disco_libres(const disco_t *d)
{
    return d == NULL ? 0 : d->libres;
}

uint32_t disco_ocupados(const disco_t *d)
{
    return d == NULL ? 0 : d->total_bloques - d->libres;
}

uint32_t disco_mayor_libre_contiguo(const disco_t *d)
{
    uint32_t i, actual = 0, mayor = 0;

    if (d == NULL)
        return 0;
    for (i = 0; i < d->total_bloques; i++) {
        if (d->dueno[i] == DISCO_LIBRE) {
            actual++;
            if (actual > mayor)
                mayor = actual;
        } else {
            actual = 0;
        }
    }
    return mayor;
}

uint32_t disco_cantidad_huecos(const disco_t *d)
{
    uint32_t i, huecos = 0;

    if (d == NULL)
        return 0;
    for (i = 0; i < d->total_bloques; i++) {
        /* un hueco empieza donde hay libre y el anterior no lo es */
        if (d->dueno[i] == DISCO_LIBRE && (i == 0 || d->dueno[i - 1] != DISCO_LIBRE))
            huecos++;
    }
    return huecos;
}

int disco_verificar(const disco_t *d)
{
    uint32_t i, contados = 0;

    if (d == NULL || d->dueno == NULL)
        return -1;
    for (i = 0; i < d->total_bloques; i++) {
        if (d->dueno[i] == DISCO_LIBRE)
            contados++;
    }
    if (contados != d->libres)
        return -1;
    return 0;
}

void disco_imprimir_mapa(const disco_t *d, FILE *out)
{
    uint64_t por_celda, celdas, c, i, inicio, fin, ocupados;
    char simbolo;

    if (d == NULL)
        return;

    por_celda = ((uint64_t)d->total_bloques + MAPA_MAX_CELDAS - 1) / MAPA_MAX_CELDAS;
    celdas = ((uint64_t)d->total_bloques + por_celda - 1) / por_celda;

    fprintf(out, "Mapa del disco (1 caracter = %" PRIu64 " bloque(s); "
                 "'.' libre, '#' ocupado, '+' mixto):\n", por_celda);

    for (c = 0; c < celdas; c++) {
        inicio = c * por_celda;
        fin = inicio + por_celda;
        if (fin > d->total_bloques)
            fin = d->total_bloques;

        ocupados = 0;
        for (i = inicio; i < fin; i++) {
            if (d->dueno[i] != DISCO_LIBRE)
                ocupados++;
        }
        if (ocupados == 0)
            simbolo = '.';
        else if (ocupados == fin - inicio)
            simbolo = '#';
        else
            simbolo = '+';

        fputc(simbolo, out);
        if ((c + 1) % MAPA_COLUMNAS == 0 || c + 1 == celdas)
            fputc('\n', out);
    }
}
