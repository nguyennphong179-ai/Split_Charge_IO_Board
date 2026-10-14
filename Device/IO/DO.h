#ifndef __DO
#define __DO

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------*/
#include "stdint.h"
#include "stdbool.h"


/* Define ------*/
#define DO_Count            10U

#define DO_ALL_MASK         0x03FFU //0011 1111 1111B

/*
 * Board hiện tại không có hardware feedback cho DO.
 */
#define DO_HAS_FEEDBACK     0U


/*
 * DO status
 *
 * bit 0 -> DO1
 * bit 1 -> DO2
 * ...
 * bit 9 -> DO10
 *
 * cmd:
 *      Lệnh hiện tại đã ghi ra GPIO.
 *
 * readback:
 *      Nếu có hardware feedback -> trạng thái feedback.
 *      Board hiện tại không có feedback.
 *
 * mismatch:
 *      1 tại bit tương ứng nếu readback != cmd.
 *      Board hiện tại không có feedback nên mismatch = 0.
 */
typedef struct
{
    uint16_t cmd;
    uint16_t readback;
    uint16_t mismatch;

} DO_Status_t;


/* Global DO status */
extern DO_Status_t DOs;

/*•  DO_SetOutput(ch, state): ghi GPIO + cập nhật bit tương ứng trong `cmd`.
•  DO_GetDOs_Scan(): đọc feedback (nếu có) vào `readback`; so với `cmd` để tính `mismatch`.
•  DO_SetSafeState(): ghi toàn bộ 10 kênh về mức an toàn, không phụ thuộc `cmd` hiện tại.
*/
/* Functions */
/* Functions */
void DO_SetOutput(uint8_t ch, bool state);

void DO_SetOutputs(uint16_t mask);

void DO_GetDOs_Scan(DO_Status_t *pObjt);

void DO_SetSafeState(DO_Status_t *pObjt);


#ifdef __cplusplus
}
#endif

#endif /* __DO */