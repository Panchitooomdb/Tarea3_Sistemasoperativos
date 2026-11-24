# Tarea 3 - Sistemas Operativos (UDP, 2026-2)

Simulador de administracion de almacenamiento secundario en C. Compara tres
mecanismos de asignacion de archivos (contigua, enlazada, indexada) sobre un
disco virtual. No se crean archivos fisicos: todo ocurre en estructuras de datos.
Desarrollado y probado en Fedora Linux (GCC, C11, POSIX).

## Requisitos (Fedora)

    sudo dnf install gcc make gdb git libasan libubsan

## Compilar y probar

    make          # genera ./simulador
    make test     # pruebas automaticas (compiladas con ASan/UBSan)
    make debug    # genera ./simulador_debug con sanitizers
    make clean

## Ejecutar

    ./simulador [--semilla N]

Pide: tamano del disco (MB), tamano de bloque (KB), mecanismo
(contigua/enlazada/indexada) y rango de tamanos de archivo (KB).

## Decisiones de diseno (NO son requisitos del enunciado)

- Rango de archivos en KB. Disco: 1..65536 MB. Bloque: 1..1024 KB.
- El disco debe ser multiplo exacto del bloque; entre 4 y 4.194.304 bloques.
- Asignacion contigua: primer ajuste (first-fit). Si no alcanza el espacio
  libre total -> "sin espacio"; si alcanza pero no es contiguo -> "fragmentacion
  externa". Acceso a una posicion = 1 lectura de bloque.
- Asignacion enlazada: puntero de 4 bytes por bloque; capacidad util = bloque - 4.
  Usa los primeros bloques libres (sin exigir continuidad), por lo que NUNCA falla por
  fragmentacion externa. Los punteros se guardan en el disco (disco_siguiente) y las
  posiciones se localizan recorriendo la cadena: accesos = bloque_logico + 1.
- Asignacion indexada: cada referencia ocupa 4 bytes; un bloque indice guarda
  bloque/4 - 1 referencias a datos y su ultima entrada apunta al siguiente indice
  ("indice enlazado"), asi un archivo grande usa varios indices encadenados.
  Se reservan datos E indices (ambos cuentan como ocupados). Nunca falla por
  fragmentacion externa. Accesos = (indices recorridos) + 1 bloque de datos.
- Se cuentan accesos a bloques de DISCO, no operaciones en la RAM del simulador.

## Estado del desarrollo

- [x] Etapa 1: configuracion y validacion
- [x] Etapa 2: disco virtual
- [x] Etapa 3: asignacion contigua
- [x] Etapa 4: asignacion enlazada
- [x] Etapa 5: asignacion indexada
- [ ] Etapa 6: eventos temporizados
- [ ] Etapa 7: fragmentacion y estadisticas
- [ ] Etapa 8: pruebas finales y documentacion definitiva
