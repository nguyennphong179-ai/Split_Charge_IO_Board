#ifndef __Timer_H
#define __Timer_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stdint.h"
#include "stdbool.h"

#define MaxTIMER 60
/**********************************************************************************************/
typedef struct 
{
	uint8_t En;	//Enable the timer
	uint32_t SV;	//Set value
	uint32_t PV; 	//Present value
	uint8_t Output;
}Tim_1ms_Objt;	//max = 65535ms

typedef struct 
{
	uint32_t Task_start[10]; 
	uint32_t Task_end[10]; 
	uint32_t Task[10]; 
	uint32_t Total;
	uint32_t maxTotal;
}Time_Objt;	//max = 65535ms

/**********************************************************************************************/

extern Tim_1ms_Objt Tim_1ms[MaxTIMER];
extern Time_Objt Tim;
void DWT_Init(void);		//Bat DWT->CYCCNT, phai goi 1 lan truoc khi dung micros_dwt()
uint32_t micros_dwt(void);
uint32_t cycles_dwt(void);			//Doc raw DWT->CYCCNT (chu ky, chua chia us)
uint32_t micros_dwt_since(uint32_t startCycles);	//us da troi qua tu 1 moc cycles_dwt() -- wraparound-safe, dung thay "micros_dwt() - start" khi do khoang thoi gian
void startTim1ms(Tim_1ms_Objt *pTim, uint32_t SV);			//Start timer 1ms
void stopTim1ms(Tim_1ms_Objt *pTim);									//Reset timer 1ms
void scanTimer(void);

#ifdef __cplusplus
}
#endif

#endif /* __Timer_H */
