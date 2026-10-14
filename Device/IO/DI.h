#ifndef __DI
#define __DI

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------*/
#include "stdint.h"
#include "stdbool.h"


/* Define ------*/
#define DI_Count            10U

#define DI_DEBOUNCE_COUNT   4U
#define DI_DEBOUNCE_MS      20U


typedef struct
{
    uint8_t stable;
    uint8_t changed;
    uint16_t debounce_ms;

} DI_Status_t;


typedef struct
{
    DI_Status_t Di[DI_Count];

} DIs_Objt;


extern DIs_Objt DIs;


/* Functions */
void GetDIs_Scan(DIs_Objt *pObjt);
bool GetInput(uint8_t *pDi);


#ifdef __cplusplus
}
#endif

#endif /* __DI */