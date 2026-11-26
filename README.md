# Tarea 3 - Sistemas Operativos

Simulador de asignacion de archivos en un disco, hecho en C. Compara tres
mecanismos: asignacion contigua, enlazada e indexada. No se crean archivos
reales: el disco y los archivos son solo estructuras de datos en memoria.
Probado en Ubuntu.

Autores: Francisco Grandon y Alexis Godoy.

## Como compilar

Instalar lo necesario (solo la primera vez):

    sudo apt update
    sudo apt install gcc make git

Compilar:

    make

Esto crea el programa `./simulador`. Para correr las pruebas automaticas:

    make test

## Como ejecutar

    ./simulador

El programa pide estos datos, en este orden:

1. Tamano del disco (MB)
2. Tamano de cada bloque (KB)
3. Mecanismo de asignacion (contigua, enlazada, indexada, o 1, 2, 3)
4. Tamano minimo de los archivos (KB)
5. Tamano maximo de los archivos (KB)

Ejemplo para probar: disco de 8 MB, bloque de 4 KB, archivos de 50 a 400 KB.
Con eso cada mecanismo termina en mas o menos 90 a 100 segundos.

Opciones extra (no son obligatorias):

- `./simulador --rapido` no espera 1 segundo entre pasos, sirve para probar rapido.
- `./simulador --semilla 5` repite siempre la misma simulacion (mismos numeros al azar).
- Con Ctrl+C se puede terminar antes y igual se muestran las estadisticas.

## Como funciona

El disco se divide en bloques y se anota cuales estan libres y cuales ocupados.
La simulacion avanza de a un segundo:

- Cada 2 segundos se crea un archivo con un tamano al azar (entre el minimo y el
  maximo). Se le asignan bloques segun el mecanismo elegido, contando tambien el
  espacio de los punteros o bloques indice. Se muestra en que bloques quedo.
- Desde el segundo 30, cada 5 segundos se elimina un archivo al azar (sus bloques
  quedan libres) y se accede a una posicion al azar de otro archivo. De ese acceso
  se muestra el bloque logico, el bloque fisico en el disco y cuantos accesos a
  disco hicieron falta para encontrarlo.
- Despues de cada evento se muestra el estado del disco: bloques ocupados y libres,
  cantidad de huecos libres, el hueco mas grande y la fragmentacion externa.
- La simulacion termina cuando no se puede crear un archivo: porque no hay espacio
  suficiente o, en la asignacion contigua, porque hay espacio libre pero no junto.

Al final se muestra un mapa del disco ('#' ocupado, '.' libre) y estadisticas:
utilizacion del disco, archivos creados y eliminados, fragmentacion externa
(final, maxima y promedio) y promedio de accesos a disco por busqueda.

## Los tres mecanismos

- Contigua: el archivo ocupa bloques seguidos (se usa el primer hueco donde cabe).
  Encontrar un bloque cuesta 1 acceso. Puede fallar por fragmentacion externa.
- Enlazada: cada bloque guarda un puntero de 4 bytes al siguiente bloque del archivo,
  por eso en cada bloque caben 4 bytes menos de datos. Para llegar a un bloque hay
  que recorrer la cadena desde el principio, asi que cuesta (numero de bloque + 1)
  accesos. No sufre fragmentacion externa.
- Indexada: un bloque indice guarda las direcciones de los bloques de datos del
  archivo (4 bytes cada una). Si el archivo es muy grande se usan varios bloques
  indice encadenados. Encontrar un dato cuesta los accesos a los indices mas 1 al
  bloque de datos. No sufre fragmentacion externa.

## Fragmentacion externa

Se calcula asi:

    (bloques libres - hueco mas grande) / bloques libres * 100

Da 0% si todo el espacio libre esta junto, y sube mientras mas repartido este en
huecos chicos. Se mide despues de cada evento y se guarda el maximo y el promedio.

## Archivos del codigo

- `src/main.c`: pide los datos y maneja el tiempo de la simulacion.
- `src/config.c`: lee y valida los datos ingresados.
- `src/disco.c`: el disco virtual (bloques libres y ocupados, huecos, mapa).
- `src/contigua.c`, `src/enlazada.c`, `src/indexada.c`: los tres mecanismos.
- `src/simulador.c`: crear, eliminar y acceder a archivos, fragmentacion y estadisticas.
- `tests/`: pruebas automaticas.
