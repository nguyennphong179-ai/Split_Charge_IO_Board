#include "Machine.h"
#include "Timer.h"
#include "Can_Addr.h"
#include "DI.h"
#include "stm32f4xx_hal.h"

/**
  * @brief  Called once after all peripherals are initialized.
  * @retval None
  */
void Machine_init(void)
{
  HAL_Delay(1000);
  CanAddr_Init();                  /* Read CAN address DIP switch */

  startTim1ms(&Tim_1ms[0], 1u);    /* 1ms timer */
  startTim1ms(&Tim_1ms[1], 5u);    /* 5ms timer */
}

/**
  * @brief  Called repeatedly from the main loop.
  * @retval None
  */
void Machine_loop(void)
{
  if ((Tim_1ms[0].En == 1u) && (Tim_1ms[0].Output == 1u))
  {
    Tim_1ms[0].Output = 0u;
    /* 1ms task */
  }

  if ((Tim_1ms[1].En == 1u) && (Tim_1ms[1].Output == 1u))
  {
    Tim_1ms[1].Output = 0u;
    /* 5ms task */
    GetDIs_Scan(&DIs);
  }
}
