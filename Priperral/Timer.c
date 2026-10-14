#include "Timer.h"
#include "stm32f4xx.h"

/**********************************************************************************************/
Tim_1ms_Objt Tim_1ms[MaxTIMER];    /* 1ms software timer */
Time_Objt    Tim;

/**********************************************************************************************/
void DWT_Init(void)
{
	/* DWT->CYCCNT khong tu chay -- phai bat trace (DEMCR.TRCENA) roi bat
	 * CYCCNTENA truoc khi dung. Thieu 2 dong nay la ly do micros_dwt() luon
	 * tra ve 0. */
	CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
	DWT->CYCCNT = 0u;
	DWT->CTRL  |= DWT_CTRL_CYCCNTENA_Msk;
}

uint32_t micros_dwt(void)
{
	return DWT->CYCCNT / (SystemCoreClock / 1000000u);
}

uint32_t cycles_dwt(void)
{
	return DWT->CYCCNT;
}

uint32_t micros_dwt_since(uint32_t startCycles)
{
	/* Tru truoc o mien so chu ky tho (uint32_t wraparound-safe: DWT->CYCCNT la
	 * thanh ghi 32-bit chay tu do, tu wrap moi ~21.47s o SYSCLK=200MHz), roi
	 * moi chia doi ra microsecond. Neu doi thu tu (chia truoc, tru sau -- kieu
	 * "micros_dwt() - startUs" cu) thi 2 phep chia rieng le pha vo tinh chat
	 * wraparound-safe cua phep tru unsigned, cho ket qua rac gan UINT32_MAX
	 * moi khi khoang do bi chia doi boi 1 lan CYCCNT wrap. Da xac nhan bang so
	 * lieu that: 1 mau do Ocpp_Loop() ra "avg=854746us max=4273492509us" --
	 * UINT32_MAX(4294967295) - max(4273492509) = 21474786, khop gan dung chu
	 * ky wrap ly thuyet cua CYCCNT o 200MHz (21474836), sai lech ~50 do lam
	 * tron 2 phep chia doc lap. */
	return (DWT->CYCCNT - startCycles) / (SystemCoreClock / 1000000u);
}

void startTim1ms(Tim_1ms_Objt *pTim, uint32_t SV)   /* Start timer 1ms */
{
	if (pTim->En == 0u)   /* if the corresponding timer is in stop mode */
	{
		pTim->En = 1u;
		if (SV < 1u)
		{
			SV = 1u;
		}
		pTim->SV     = SV - 1u;
		pTim->Output = 0u;
		pTim->PV     = 0u;
	}
}

/**********************************************************************************************/
void stopTim1ms(Tim_1ms_Objt *pTim)   /* Reset timer 1ms */
{
	pTim->En     = 0u;
	pTim->SV     = 0u;
	pTim->Output = 0u;
	pTim->PV     = 0u;
}

/**********************************************************************************************/
void scanTimer(void)
{
	uint8_t i;
	for (i = 0u; i < MaxTIMER; i++)   /* Scan all declared timers */
	{
		if (Tim_1ms[i].En == 1u)
		{
			if (++Tim_1ms[i].PV > Tim_1ms[i].SV)   /* Overflow */
			{
				Tim_1ms[i].PV     = 0u;
				Tim_1ms[i].Output = 1u;   /* Turn On the Timer Output */
			}
		}
	}
}

/* HAL_TIM_PeriodElapsedCallback() driving scanTimer() from TIM2 is currently
 * disabled -- original draft kept for reference:
 *
 *   void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
 *   {
 *       if (htim->Instance == TIM2)
 *       {
 *           scanTimer();
 *       }
 *   }
 */