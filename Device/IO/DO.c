#include "DO.h"
#include "main.h"
#include <stddef.h>


/* Global DO object */
DO_Status_t DOs =
{
    .cmd = 0U,
    .readback = 0U,
    .mismatch = 0U
};


/* Private function */
static void DO_WriteGPIO(uint8_t ch, GPIO_PinState pinState);


/*
 * Set one DO channel.
 *
 * ch:
 *      0 -> DO1
 *      1 -> DO2
 *      ...
 *      9 -> DO10
 *
 * Board:
 *      active-high
 *
 * state = true  -> GPIO SET   -> Relay ON
 * state = false -> GPIO RESET -> Relay OFF
 */
void DO_SetOutput(uint8_t ch, bool state)
{
    GPIO_PinState pinState;


    /*
     * Check channel.
     */
    if (ch >= DO_Count)
    {
        return;
    }


    /*
     * DO is active-high.
     */
    if (state == true)
    {
        pinState = GPIO_PIN_SET;
    }
    else
    {
        pinState = GPIO_PIN_RESET;
    }


    /*
     * Write physical GPIO immediately.
     */
    DO_WriteGPIO(ch, pinState);


    /*
     * Update corresponding bit in cmd.
     */
    if (state == true)
    {
        DOs.cmd |=
            (uint16_t)(1U << ch);
    }
    else
    {
        DOs.cmd &=
            (uint16_t)(~((uint16_t)(1U << ch)));
    }


    /*
     * Keep only 10 valid DO bits.
     */
    DOs.cmd &= DO_ALL_MASK;
}

/*
 * Set all 10 DO channels from bitmask.
 *
 * mask:
 *      bit 0 -> DO1
 *      bit 1 -> DO2
 *      ...
 *      bit 9 -> DO10
 *
 * Example:
 *      mask = 0x0015
 *
 *      DO1 = ON
 *      DO2 = OFF
 *      DO3 = ON
 *      DO4 = OFF
 *      DO5 = ON
 *      others = OFF
 *
 * Board:
 *      active-high
 */
void DO_SetOutputs(uint16_t mask)
{
    uint8_t ch;
    GPIO_PinState pinState;


    /*
     * Keep only 10 valid DO bits.
     */
    mask &= DO_ALL_MASK;


    /*
     * Update all 10 physical outputs.
     */
    for (ch = 0U; ch < DO_Count; ch++)
    {
        if ((mask & ((uint16_t)1U << ch)) != 0U)
        {
            pinState = GPIO_PIN_SET;
        }
        else
        {
            pinState = GPIO_PIN_RESET;
        }


        DO_WriteGPIO(ch, pinState);
    }


    /*
     * Update software command status.
     */
    DOs.cmd = mask;
}

/*
 * Scan DO feedback.
 *
 * Board hiện tại KHÔNG có hardware feedback.
 *
 * Vì vậy:
 * - Không đọc lại chân GPIO output rồi gọi đó là feedback.
 * - Không thể xác định relay vật lý có thực sự đóng/mở hay không.
 *
 * readback ở board hiện tại chỉ mirror command để giữ
 * cấu trúc dữ liệu đồng nhất.
 *
 * mismatch luôn bằng 0 vì không có feedback vật lý
 * để so sánh.
 */
void DO_GetDOs_Scan(DO_Status_t *pObjt)
{
    if (pObjt == NULL)
    {
        return;
    }


#if (DO_HAS_FEEDBACK == 1U)

    /*
     * Nếu phiên bản phần cứng sau này có feedback,
     * đọc 10 chân feedback tại đây.
     *
     * Sau đó:
     *
     * pObjt->mismatch =
     *      (pObjt->cmd ^ pObjt->readback)
     *      & DO_ALL_MASK;
     */

#else

    /*
     * No hardware feedback.
     *
     * Đây chỉ là software shadow,
     * KHÔNG phải trạng thái relay thực tế.
     */
    pObjt->readback = pObjt->cmd;

    pObjt->mismatch = 0U;

#endif
}


/*
 * Set all 10 DO channels to safe state immediately.
 *
 * Safe state của board hiện tại:
 *
 *      ALL RELAYS OFF
 *
 * Active-high output:
 *
 *      GPIO RESET -> Relay OFF
 *
 * Hàm này không phụ thuộc cmd hiện tại.
 */
void DO_SetSafeState(DO_Status_t *pObjt)
{
    uint8_t ch;


    if (pObjt == NULL)
    {
        return;
    }


    /*
     * Force all relay outputs OFF immediately.
     */
    for (ch = 0U; ch < DO_Count; ch++)
    {
        DO_WriteGPIO(ch, GPIO_PIN_RESET);
    }


    /*
     * Update software status.
     */
    pObjt->cmd = 0U;


    /*
     * No hardware feedback.
     */
    pObjt->readback = 0U;

    pObjt->mismatch = 0U;
}

/*
 * Write one physical DO GPIO.
 */
static void DO_WriteGPIO(uint8_t ch, GPIO_PinState pinState)
{
    if (ch == 0U)
    {
        HAL_GPIO_WritePin(DO1_GPIO_Port,
                          DO1_Pin,
                          pinState);
    }
    else if (ch == 1U)
    {
        HAL_GPIO_WritePin(DO2_GPIO_Port,
                          DO2_Pin,
                          pinState);
    }
    else if (ch == 2U)
    {
        HAL_GPIO_WritePin(DO3_GPIO_Port,
                          DO3_Pin,
                          pinState);
    }
    else if (ch == 3U)
    {
        HAL_GPIO_WritePin(DO4_GPIO_Port,
                          DO4_Pin,
                          pinState);
    }
    else if (ch == 4U)
    {
        HAL_GPIO_WritePin(DO5_GPIO_Port,
                          DO5_Pin,
                          pinState);
    }
    else if (ch == 5U)
    {
        HAL_GPIO_WritePin(DO6_GPIO_Port,
                          DO6_Pin,
                          pinState);
    }
    else if (ch == 6U)
    {
        HAL_GPIO_WritePin(DO7_GPIO_Port,
                          DO7_Pin,
                          pinState);
    }
    else if (ch == 7U)
    {
        HAL_GPIO_WritePin(DO8_GPIO_Port,
                          DO8_Pin,
                          pinState);
    }
    else if (ch == 8U)
    {
        HAL_GPIO_WritePin(DO9_GPIO_Port,
                          DO9_Pin,
                          pinState);
    }
    else if (ch == 9U)
    {
        HAL_GPIO_WritePin(DO10_GPIO_Port,
                          DO10_Pin,
                          pinState);
    }
    else
    {
        /* Invalid channel */
    }
}