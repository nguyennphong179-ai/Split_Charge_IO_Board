#ifndef __CAN_ADDR
#define __CAN_ADDR

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ----------------------------------------------------------*/
#include "stdint.h"
#include "stdbool.h"

#define CAN_Addr_Count     6u

/* CAN address data */
typedef struct
{
    uint8_t address;   /* Node address: 0 -> 63 */
    bool    valid;     /* true after CanAddr_Init() */
} CanAddr_t;


/* Function prototypes ----------------------------------------------*/

/*
 * Read 6 DIP/strap pins ONE TIME at boot.
 * The result is latched and will not change until reset/power cycle.
 */
void CanAddr_Init(void);

/*
 * Return pointer to latched CAN address.
 * This function DOES NOT read GPIO.
 */
const CanAddr_t *CanAddr_Get(void);


#ifdef __cplusplus
}
#endif

#endif /* __CAN_ADDR */