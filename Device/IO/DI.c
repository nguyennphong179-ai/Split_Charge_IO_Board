#include "DI.h"
#include "main.h"


/* Input from GPIO */
#define DI1_STS     HAL_GPIO_ReadPin(DI1_GPIO_Port, DI1_Pin)
#define DI2_STS     HAL_GPIO_ReadPin(DI2_GPIO_Port, DI2_Pin)
#define DI3_STS     HAL_GPIO_ReadPin(DI3_GPIO_Port, DI3_Pin)
#define DI4_STS     HAL_GPIO_ReadPin(DI4_GPIO_Port, DI4_Pin)
#define DI5_STS     HAL_GPIO_ReadPin(DI5_GPIO_Port, DI5_Pin)
#define DI6_STS     HAL_GPIO_ReadPin(DI6_GPIO_Port, DI6_Pin)
#define DI7_STS     HAL_GPIO_ReadPin(DI7_GPIO_Port, DI7_Pin)
#define DI8_STS     HAL_GPIO_ReadPin(DI8_GPIO_Port, DI8_Pin)
#define DI9_STS     HAL_GPIO_ReadPin(DI9_GPIO_Port, DI9_Pin)
#define DI10_STS    HAL_GPIO_ReadPin(DI10_GPIO_Port, DI10_Pin)


/*
 * Active-low mask.
 *
 * Board hiện tại:
 * - GPIO pull-down
 * - DI active-high
 *
 * => Không đảo polarity.
 */
static const uint16_t active_low_mask = 0x0000U;


/*
 * Debounce counter riêng cho 10 DI.
 *
 * Counter:
 * 0 -> 4
 *
 * GetDIs_Scan() chạy mỗi 5 ms.
 * 4 count = 20 ms.
 *
 * Chỉ dùng nội bộ DI.c.
 */
static uint8_t debounce_counter[DI_Count] =
{
    0U, 0U, 0U, 0U, 0U,
    0U, 0U, 0U, 0U, 0U
};


/* Global DI object */
DIs_Objt DIs =
{
    .Di =
    {
        {0U, 0U, DI_DEBOUNCE_MS},
        {0U, 0U, DI_DEBOUNCE_MS},
        {0U, 0U, DI_DEBOUNCE_MS},
        {0U, 0U, DI_DEBOUNCE_MS},
        {0U, 0U, DI_DEBOUNCE_MS},
        {0U, 0U, DI_DEBOUNCE_MS},
        {0U, 0U, DI_DEBOUNCE_MS},
        {0U, 0U, DI_DEBOUNCE_MS},
        {0U, 0U, DI_DEBOUNCE_MS},
        {0U, 0U, DI_DEBOUNCE_MS}
    }
};


/*
 * Get 10 DI states.
 *
 * Call every 5 ms from Tim_1ms[1].
 */
void GetDIs_Scan(DIs_Objt *pObjt)
{
    uint8_t raw_state[DI_Count];
    uint8_t logical_state;
    uint8_t old_stable;
    uint8_t index;


    /*
     * Read raw GPIO.
     */
    raw_state[0] = (uint8_t)DI1_STS;
    raw_state[1] = (uint8_t)DI2_STS;
    raw_state[2] = (uint8_t)DI3_STS;
    raw_state[3] = (uint8_t)DI4_STS;
    raw_state[4] = (uint8_t)DI5_STS;
    raw_state[5] = (uint8_t)DI6_STS;
    raw_state[6] = (uint8_t)DI7_STS;
    raw_state[7] = (uint8_t)DI8_STS;
    raw_state[8] = (uint8_t)DI9_STS;
    raw_state[9] = (uint8_t)DI10_STS;


    for (index = 0U; index < DI_Count; index++)
    {
        /*
         * Save previous stable state.
         */
        old_stable = pObjt->Di[index].stable;


        /*
         * Normalize polarity ONE TIME.
         *
         * active-high:
         * active_low_mask bit = 0
         *
         * raw 0 -> logical 0
         * raw 1 -> logical 1
         */
        logical_state =
            raw_state[index] ^
            (uint8_t)((active_low_mask >> index) & 0x01U);


        /*
         * Debounce:
         *
         * ON  -> counter + 1, max = 4
         * OFF -> counter - 1, min = 0
         */
        if (logical_state != 0U)
        {
            if (debounce_counter[index] < DI_DEBOUNCE_COUNT)
            {
                debounce_counter[index]++;
            }
        }
        else
        {
            if (debounce_counter[index] > 0U)
            {
                debounce_counter[index]--;
            }
        }


        /*
         * counter = 4:
         * input confirmed ON.
         */
        if (debounce_counter[index] >= DI_DEBOUNCE_COUNT)
        {
            debounce_counter[index] = DI_DEBOUNCE_COUNT;

            pObjt->Di[index].stable = 1U;
        }

        /*
         * counter = 0:
         * input confirmed OFF.
         */
        else if (debounce_counter[index] == 0U)
        {
            pObjt->Di[index].stable = 0U;
        }

        /*
         * counter = 1, 2 or 3:
         * keep old stable value.
         */
        else
        {
            /* Keep stable state */
        }


        /*
         * changed =
         * stable(current) XOR stable(previous)
         */
        pObjt->Di[index].changed =
            pObjt->Di[index].stable ^ old_stable;
    }
}


/*
 * Get debounced DI state.
 *
 * Polarity is already normalized in GetDIs_Scan().
 * Do NOT invert again here.
 */
bool GetInput(uint8_t *pDi)
{
    bool result = false;


    if (pDi != NULL)
    {
        if (pDi[0] != 0U)
        {
            if (pDi[1] < DI_Count)
            {
                result =
                    (DIs.Di[pDi[1]].stable != 0U);
            }
        }
    }


    return result;
}