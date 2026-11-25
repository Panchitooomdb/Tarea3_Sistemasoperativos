#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "contigua.h"
#include "enlazada.h"
#include "indexada.h"
#include "simulador.h"

int simulador_iniciar(simulador_t *sim, const config_t *cfg)
{
    memset(sim, 0, sizeof *sim);
    sim->cfg = *cfg;
    sim->siguiente_id = 1;

    sim->disco = disco_crear((uint32_t)cfg->total_bloques);
    if (sim->disco == NULL)
        return -1;

    srand(cfg->semilla);
    return 0;
}

void simulador_destruir(simulador_t *sim)
{
    uint32_t i;

    /* Los archivos indexados tienen memoria propia (refs), hay que liberarla */
    for (i = 0; i < sim->n_archivos; i++) {
        if (sim->cfg.mecanismo == ASIG_INDEXADA)
            indexada_liberar(sim->disco, &sim->archivos[i]);
    }
    free(sim->archivos);
    sim->archivos = NULL;
    sim->n_archivos = 0;

    disco_destruir(sim->disco);
    sim->disco = NULL;
}

/* Agrega un archivo al final del arreglo (agrandandolo si hace falta). */
static int agregar_archivo(simulador_t *sim, const archivo_t *a)
{
    archivo_t *nuevo;
    uint32_t nueva_cap;

    if (sim->n_archivos == sim->capacidad) {
        nueva_cap = sim->capacidad == 0 ? 16 : sim->capacidad * 2;
        nuevo = realloc(sim->archivos, nueva_cap * sizeof(archivo_t));
        if (nuevo == NULL)
            return -1;
        sim->archivos = nuevo;
        sim->capacidad = nueva_cap;
    }
    sim->archivos[sim->n_archivos] = *a;
    sim->n_archivos++;
    return 0;
}

/* Numero aleatorio entre min y max (ambos incluidos). */
static uint64_t aleatorio_entre(uint64_t min, uint64_t max)
{
    if (max <= min)
        return min;
    return min + (uint64_t)rand() % (max - min + 1);
}

/* Muestra donde quedo guardado el archivo (depende del mecanismo). */
static void imprimir_ubicacion(const simulador_t *sim, const archivo_t *a)
{
    uint32_t cur, k;

    if (sim->cfg.mecanismo == ASIG_CONTIGUA) {
        printf("    ubicacion: bloques %" PRIu32 " a %" PRIu32 "\n",
               a->inicio, a->inicio + a->largo - 1);
    } else if (sim->cfg.mecanismo == ASIG_ENLAZADA) {
        printf("    cadena de bloques:");
        cur = a->primero;
        for (k = 0; k < a->bloques_total && cur != DISCO_FIN; k++) {
            if (k == 10) {
                printf(" ...");
                break;
            }
            printf(" %" PRIu32, cur);
            cur = disco_siguiente(sim->disco, cur);
        }
        printf("\n");
    } else {
        printf("    primer bloque indice: %" PRIu32 " (%" PRIu32 " indices), datos:",
               a->primero, a->n_indices);
        for (k = 0; k < a->bloques_datos; k++) {
            if (k == 10) {
                printf(" ...");
                break;
            }
            printf(" %" PRIu32, a->refs[k]);
        }
        printf("\n");
    }
}

asig_resultado_t simulador_crear_archivo(simulador_t *sim)
{
    archivo_t a;
    asig_resultado_t res;
    uint64_t tam_kb, tam_bytes, bloque_bytes;
    uint32_t id;

    memset(&a, 0, sizeof a);
    id = sim->siguiente_id;
    bloque_bytes = sim->cfg.bloque_bytes;

    tam_kb = aleatorio_entre(sim->cfg.archivo_min_kb, sim->cfg.archivo_max_kb);
    tam_bytes = tam_kb * 1024;

    if (sim->cfg.mecanismo == ASIG_CONTIGUA)
        res = contigua_asignar(sim->disco, &a, id, tam_bytes, bloque_bytes);
    else if (sim->cfg.mecanismo == ASIG_ENLAZADA)
        res = enlazada_asignar(sim->disco, &a, id, tam_bytes, bloque_bytes);
    else
        res = indexada_asignar(sim->disco, &a, id, tam_bytes, bloque_bytes);

    if (res != ASIG_RES_OK) {
        printf("[CREAR] archivo %" PRIu32 " (%" PRIu64 " KB): FALLO -> %s\n",
               id, tam_kb, asig_mensaje(res));
        return res;
    }

    if (agregar_archivo(sim, &a) != 0) {
        /* sin memoria para anotarlo: se deshace la asignacion */
        if (sim->cfg.mecanismo == ASIG_CONTIGUA)
            contigua_liberar(sim->disco, &a);
        else if (sim->cfg.mecanismo == ASIG_ENLAZADA)
            enlazada_liberar(sim->disco, &a);
        else
            indexada_liberar(sim->disco, &a);
        return ASIG_RES_ERR_INTERNO;
    }

    sim->siguiente_id++;
    sim->creados++;
    printf("[CREAR] archivo %" PRIu32 " (%" PRIu64 " KB) usa %" PRIu32 " bloques\n",
           id, tam_kb, a.bloques_total);
    imprimir_ubicacion(sim, &a);
    return ASIG_RES_OK;
}

int simulador_eliminar_aleatorio(simulador_t *sim)
{
    uint32_t pos;
    archivo_t *a;
    uint32_t id, bloques;

    if (sim->n_archivos == 0)
        return -1;

    pos = (uint32_t)(rand() % sim->n_archivos);
    a = &sim->archivos[pos];
    id = a->id;
    bloques = a->bloques_total;

    if (sim->cfg.mecanismo == ASIG_CONTIGUA)
        contigua_liberar(sim->disco, a);
    else if (sim->cfg.mecanismo == ASIG_ENLAZADA)
        enlazada_liberar(sim->disco, a);
    else
        indexada_liberar(sim->disco, a);

    /* Para no dejar un hueco en el arreglo, se mueve el ultimo archivo a su lugar */
    sim->archivos[pos] = sim->archivos[sim->n_archivos - 1];
    sim->n_archivos--;
    sim->eliminados++;

    printf("[ELIMINAR] archivo %" PRIu32 ": se liberaron %" PRIu32 " bloques\n", id, bloques);
    return 0;
}

int simulador_acceder_aleatorio(simulador_t *sim)
{
    archivo_t *a;
    acceso_t acc;
    uint64_t pos;
    int r;

    if (sim->n_archivos == 0)
        return -1;

    a = &sim->archivos[rand() % sim->n_archivos];
    pos = aleatorio_entre(0, a->tam_bytes - 1);

    if (sim->cfg.mecanismo == ASIG_CONTIGUA)
        r = contigua_localizar(a, pos, sim->cfg.bloque_bytes, &acc);
    else if (sim->cfg.mecanismo == ASIG_ENLAZADA)
        r = enlazada_localizar(sim->disco, a, pos, sim->cfg.bloque_bytes, &acc);
    else
        r = indexada_localizar(sim->disco, a, pos, sim->cfg.bloque_bytes, &acc);

    if (r != 0) {
        printf("[ACCESO] archivo %" PRIu32 ": no se pudo localizar la posicion %" PRIu64 "\n",
               a->id, pos);
        return -1;
    }

    sim->total_accesos += acc.accesos;
    sim->n_busquedas++;
    printf("[ACCESO] archivo %" PRIu32 ", posicion %" PRIu64 " B -> bloque logico %" PRIu32
           ", bloque fisico %" PRIu32 ", accesos a disco: %" PRIu32 "\n",
           a->id, pos, acc.bloque_logico, acc.bloque_fisico, acc.accesos);
    return 0;
}
