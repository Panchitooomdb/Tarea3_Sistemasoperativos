#include "asignacion.h"

const char *asig_mensaje(asig_resultado_t r)
{
    switch (r) {
    case ASIG_RES_OK:               return "asignacion exitosa";
    case ASIG_RES_ERR_PARAM:        return "parametros invalidos";
    case ASIG_RES_ERR_SIN_ESPACIO:  return "no hay espacio libre suficiente en el disco";
    case ASIG_RES_ERR_FRAGMENTACION:return "fragmentacion externa: hay bloques libres suficientes pero no contiguos";
    case ASIG_RES_ERR_INTERNO:      return "error interno de consistencia";
    }
    return "resultado desconocido";
}
