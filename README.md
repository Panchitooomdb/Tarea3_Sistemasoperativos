# Tarea 3 - Sistemas Operativos (UDP, 2026-2)

Simulador de asignacion de archivos (contigua, enlazada e indexada) en C.
Desarrollado y probado en Fedora Linux.

## Requisitos (Fedora)

    sudo dnf install gcc make gdb git libasan libubsan

## Compilar y probar

    make          # genera ./simulador
    make test     # pruebas automaticas con sanitizers
    make debug    # ./simulador_debug con ASan/UBSan
    make clean

## Estado

Etapa 1: lectura y validacion de parametros. (Documentacion final pendiente.)
