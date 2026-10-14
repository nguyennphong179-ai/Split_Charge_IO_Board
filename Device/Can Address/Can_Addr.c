#include "Can_Addr.h"
#include "main.h"

/* Input from GPIO ---------------------------------------------------*/
#define CAN_Adr1_STS   HAL_GPIO_ReadPin(CAN_Adr1_GPIO_Port, CAN_Adr1_Pin)
#define CAN_Adr2_STS   HAL_GPIO_ReadPin(CAN_Adr2_GPIO_Port, CAN_Adr2_Pin)
#define CAN_Adr3_STS   HAL_GPIO_ReadPin(CAN_Adr3_GPIO_Port, CAN_Adr3_Pin)
#define CAN_Adr4_STS   HAL_GPIO_ReadPin(CAN_Adr4_GPIO_Port, CAN_Adr4_Pin)
#define CAN_Adr5_STS   HAL_GPIO_ReadPin(CAN_Adr5_GPIO_Port, CAN_Adr5_Pin)
#define CAN_Adr6_STS   HAL_GPIO_ReadPin(CAN_Adr6_GPIO_Port, CAN_Adr6_Pin)


/* Private variables ------------------------------------------------*/

/*
 * Static:
 * - Chỉ tồn tại bên trong Can_Addr.c
 * - Giá trị được giữ nguyên trong suốt thời gian MCU chạy
 */
static CanAddr_t s_canAddr =
{
    .address = 0u,
    .valid   = false
};


/* Functions --------------------------------------------------------*/

void CanAddr_Init(void)
{
    uint8_t address = 0u;

    /*
     * DIP1 -> bit 0
     * DIP2 -> bit 1
     * DIP3 -> bit 2
     * DIP4 -> bit 3
     * DIP5 -> bit 4
     * DIP6 -> bit 5
     *
     * => address = 0 ... 63
     */
    address |= (((uint8_t)CAN_Adr1_STS & 0x01u) << 0);
    address |= (((uint8_t)CAN_Adr2_STS & 0x01u) << 1);
    address |= (((uint8_t)CAN_Adr3_STS & 0x01u) << 2);
    address |= (((uint8_t)CAN_Adr4_STS & 0x01u) << 3);
    address |= (((uint8_t)CAN_Adr5_STS & 0x01u) << 4);
    address |= (((uint8_t)CAN_Adr6_STS & 0x01u) << 5);

    /*
     * Latch giá trị.
     */
    s_canAddr.address = address;
    s_canAddr.valid   = true;
}


const CanAddr_t *CanAddr_Get(void)
{
    /*
     * CHỈ trả về địa chỉ của struct đã lưu.
     *
     * KHÔNG HAL_GPIO_ReadPin()
     * KHÔNG cập nhật address
     */
    return &s_canAddr;
}