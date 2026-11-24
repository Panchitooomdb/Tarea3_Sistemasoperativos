#ifndef DISCO_H
#define DISCO_H

#include <stdint.h>
#include <stdio.h>

/* Valor de 'dueno' que indica bloque libre. Los IDs de archivo parten en 1. */
#define DISCO_LIBRE 0u

/* Marca de "sin siguiente" en la tabla de punteros (fin de cadena o bloque libre). */
#define DISCO_FIN 0xFFFFFFFFu

/* Disco virtual: un arreglo que dice, por cada bloque fisico, que archivo
 * lo ocupa (o DISCO_LIBRE). No se guardan datos reales. */
typedef struct {
    uint32_t  total_bloques;
    uint32_t  libres;      /* contador mantenido en sincronia con 'dueno' */
    uint32_t *dueno;       /* dueno[i] = id del archivo duenyo del bloque i */
    uint32_t *siguiente;   /* siguiente[i] = puntero guardado "dentro" del bloque i
                              (lo usa la asignacion enlazada); DISCO_FIN si no hay */
} disco_t;

/* Crea un disco con todos los bloques libres. NULL si total==0 o no hay memoria. */
disco_t *disco_crear(uint32_t total_bloques);

/* Libera toda la memoria. Acepta NULL. */
void disco_destruir(disco_t *d);

/* 1 si el numero de bloque existe en el disco, 0 si no. */
int disco_bloque_valido(const disco_t *d, uint32_t bloque);

/* 1 si el bloque existe y esta libre; 0 si esta ocupado o no existe
 * (asi un algoritmo de asignacion nunca elige un bloque invalido). */
int disco_esta_libre(const disco_t *d, uint32_t bloque);

/* Id del archivo duenyo; DISCO_LIBRE si esta libre o no existe. */
uint32_t disco_dueno(const disco_t *d, uint32_t bloque);

/* Puntero almacenado en el bloque. DISCO_FIN si es el ultimo, esta libre o no existe. */
uint32_t disco_siguiente(const disco_t *d, uint32_t bloque);

/* Escribe el puntero del bloque. 0 si ok; -1 si el bloque no existe o esta libre,
 * o si 'sig' no es DISCO_FIN ni un bloque existente. */
int disco_set_siguiente(disco_t *d, uint32_t bloque, uint32_t sig);

/* Marca el bloque como ocupado por id_archivo.
 * 0 si ok; -1 si el bloque no existe, ya estaba ocupado o id_archivo es 0. */
int disco_ocupar(disco_t *d, uint32_t bloque, uint32_t id_archivo);

/* Libera el bloque. 0 si ok; -1 si no existe o ya estaba libre. */
int disco_liberar(disco_t *d, uint32_t bloque);

uint32_t disco_libres(const disco_t *d);
uint32_t disco_ocupados(const disco_t *d);

/* Largo de la mayor secuencia de bloques libres consecutivos (0 si no hay). */
uint32_t disco_mayor_libre_contiguo(const disco_t *d);

/* Cantidad de secuencias ("huecos") de bloques libres consecutivos. */
uint32_t disco_cantidad_huecos(const disco_t *d);

/* Comprueba que el contador 'libres' coincida con el mapa real.
 * 0 si es consistente, -1 si no. */
int disco_verificar(const disco_t *d);

/* Dibuja el mapa: '.' libre, '#' ocupado, '+' mixto (si un caracter agrupa
 * varios bloques en discos grandes). */
void disco_imprimir_mapa(const disco_t *d, FILE *out);

#endif
