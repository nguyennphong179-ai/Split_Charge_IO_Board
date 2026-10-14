#ifndef __CANBUS
#define __CANBUS

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ----------------------------------------------------------*/
#include "stdint.h"
#include "stdbool.h"


/*
 * Driver CAN 2.0B cho mạch I/O — STM32F407VET6, CAN1, 125 kbps.
 *
 * Định dạng CAN ID 29-bit (theo CAN.docx):
 *
 *   Bit    | 28..26   | 25..19  | 18..14  | 13..7     | 6..0
 *   Trường | Priority | Dest ID | Command | Source ID | Dự phòng (= 0)
 *
 * Dải Node ID (7 bit):
 *   0x00        MCU trung tâm (Master)
 *   0x01..0x3F  MCU súng
 *   0x40..0x5F  PM
 *   0x60..0x7E  Mạch I/O
 *
 * Mạch I/O:  node_id = CANBUS_IO_NODE_BASE + DIP (0..30)
 *            DIP 31..63 -> ngoài dải I/O -> KHÔNG tham gia bus.
 */


/* Config ------------------------------------------------------------*/

#define CANBUS_BITRATE              125000UL

/*
 * Dải Node ID của mạch I/O.
 */
#define CANBUS_IO_NODE_BASE         0x60U
#define CANBUS_IO_NODE_LAST         0x7EU

#define CANBUS_MASTER_NODE_ID       0x00U

/*
 * Heartbeat 0x5: master gửi tới Dest = 0x00 (địa chỉ MCU trung tâm) cho
 * tất cả slave cùng nghe. Nhiều frame khác cũng có Dest = 0x00 (node khác
 * trả lời master), nên filter bank riêng chỉ nhận khi
 * Dest = 0x00 VÀ Command = 0x5.
 */
#define CANBUS_HEARTBEAT_DEST       0x00U

/*
 * Kích thước hàng đợi RX (ISR -> main loop).
 * PHẢI là lũy thừa của 2.
 */
#define CANBUS_RX_QUEUE_SIZE        16U

/*
 * Thời gian chờ sau khi phát hiện bus-off (hoặc khởi tạo lỗi)
 * trước khi chạy lại chuỗi khởi tạo.
 *
 * Sau khi khởi tạo lại, bxCAN còn tự chờ 128 x 11 bit lặn
 * (~11.3 ms @ 125 kbps) theo ISO 11898 mới tham gia bus.
 */
#define CANBUS_RECOVERY_DELAY_MS    100U


/* Protocol ----------------------------------------------------------*/

/* Priority (bit 28..26) */
#define CANBUS_PRIO_URGENT          0x0U    /* Khẩn cấp / lỗi          */
#define CANBUS_PRIO_CONTROL         0x1U    /* Lệnh điều khiển K, PM   */
#define CANBUS_PRIO_STATUS          0x2U    /* Báo cáo trạng thái      */

/* Command Type (bit 18..14) */
#define CANBUS_CMD_URGENT_EVENT     0x0U
#define CANBUS_CMD_WRITE_CONTROL    0x1U
#define CANBUS_CMD_READ_REQUEST     0x2U
#define CANBUS_CMD_CONTROL_RESP     0x3U
#define CANBUS_CMD_POLLING_RESP     0x4U
#define CANBUS_CMD_HEARTBEAT_REQ    0x5U
#define CANBUS_CMD_HEARTBEAT_ACK    0x6U

/* Vị trí / độ rộng các trường trong CAN ID */
#define CANBUS_ID_PRIO_POS          26U
#define CANBUS_ID_PRIO_MASK         0x07UL
#define CANBUS_ID_DEST_POS          19U
#define CANBUS_ID_DEST_MASK         0x7FUL
#define CANBUS_ID_CMD_POS           14U
#define CANBUS_ID_CMD_MASK          0x1FUL
#define CANBUS_ID_SRC_POS           7U
#define CANBUS_ID_SRC_MASK          0x7FUL

/* Tách trường từ CAN ID nhận được */
#define CANBUS_ID_GET_PRIO(id)      ((uint8_t)(((id) >> CANBUS_ID_PRIO_POS) & CANBUS_ID_PRIO_MASK))
#define CANBUS_ID_GET_DEST(id)      ((uint8_t)(((id) >> CANBUS_ID_DEST_POS) & CANBUS_ID_DEST_MASK))
#define CANBUS_ID_GET_CMD(id)       ((uint8_t)(((id) >> CANBUS_ID_CMD_POS)  & CANBUS_ID_CMD_MASK))
#define CANBUS_ID_GET_SRC(id)       ((uint8_t)(((id) >> CANBUS_ID_SRC_POS)  & CANBUS_ID_SRC_MASK))


/* Types -------------------------------------------------------------*/

typedef enum
{
    CANBUS_STATE_UNINIT = 0,        /* Chưa gọi CanBus_Init()                 */
    CANBUS_STATE_ERROR_ACTIVE,      /* Bình thường, TEC/REC < 96              */
    CANBUS_STATE_ERROR_WARNING,     /* TEC hoặc REC >= 96                     */
    CANBUS_STATE_ERROR_PASSIVE,     /* TEC hoặc REC >= 128                    */
    CANBUS_STATE_BUS_OFF,           /* TEC > 255, đang chờ tự phục hồi        */
    CANBUS_STATE_INIT_FAIL,         /* Khởi tạo HAL lỗi, sẽ tự thử lại        */
    CANBUS_STATE_CONFIG_ERROR       /* Địa chỉ DIP ngoài dải / sai baudrate:
                                       KHÔNG tham gia bus, KHÔNG thử lại      */

} CanBus_State_t;


typedef struct
{
    CanBus_State_t state;

    /*
     * tx_id: ID gốc cho bản tin node GỬI ĐI
     *        Source = node_id, Dest = Master, Priority = 0, Command = 0.
     *        CanBus_Send() OR thêm Priority + Command vào.
     *
     * rx_id: ID gốc cho bản tin node NHẬN
     *        Dest = node_id, các trường khác = 0.
     *        Filter phần cứng chỉ so khớp trường Dest.
     */
    uint32_t       tx_id;
    uint32_t       rx_id;

    uint32_t       err_count;   /* Tổng sự kiện lỗi: warning, passive,
                                   bus-off, RX FIFO overrun, đầy queue RX */

} CanBus_Status_t;


typedef struct
{
    uint32_t id;                /* CAN ID 29-bit đầy đủ                   */
    uint8_t  dlc;
    uint8_t  data[8];

} CanBus_Frame_t;


/* Inline -----------------------------------------------------------*/

/*
 * Ghép CAN ID 29-bit từ các trường.
 * Đây là NƠI DUY NHẤT tạo CAN ID — không hardcode ID ở chỗ khác.
 */
static inline uint32_t CanBus_MakeId(uint8_t prio, uint8_t dest,
                                     uint8_t cmd,  uint8_t src)
{
    return (((uint32_t)prio & CANBUS_ID_PRIO_MASK) << CANBUS_ID_PRIO_POS) |
           (((uint32_t)dest & CANBUS_ID_DEST_MASK) << CANBUS_ID_DEST_POS) |
           (((uint32_t)cmd  & CANBUS_ID_CMD_MASK)  << CANBUS_ID_CMD_POS)  |
           (((uint32_t)src  & CANBUS_ID_SRC_MASK)  << CANBUS_ID_SRC_POS);
}


/* Function prototypes ----------------------------------------------*/

/*
 * Công thức thống nhất: địa chỉ DIP -> node_id -> tx_id / rx_id.
 * Trả về false nếu địa chỉ nằm ngoài dải mạch I/O (0x60..0x7E).
 */
bool CanBus_CalcIds(uint8_t address, uint32_t *pTxId, uint32_t *pRxId);

/*
 * Gọi 1 lần sau MX_CAN1_Init().
 * Kiểm tra baudrate thực tế, lấy address từ CanAddr_Get(),
 * tính tx_id/rx_id, cấu hình filter, bật ngắt RX FIFO0 + lỗi, start CAN.
 */
bool CanBus_Init(void);

/*
 * Gọi định kỳ trong main loop (vd. mỗi 5 ms).
 * Cập nhật state, phát hiện bus-off và tự chạy lại chuỗi khởi tạo.
 */
void CanBus_Process(void);

/*
 * Gửi 1 frame tới Master với Source = node_id.
 * Không chặn: trả về false nếu bus không sẵn sàng hoặc hết mailbox.
 */
bool CanBus_Send(uint8_t prio, uint8_t cmd, const uint8_t *pData, uint8_t len);

/*
 * Lấy 1 frame từ hàng đợi RX. Trả về false nếu hàng đợi rỗng.
 * Mọi frame trong hàng đợi đều có Dest = node_id (đã lọc bằng phần cứng).
 */
bool CanBus_Read(CanBus_Frame_t *pFrame);

const CanBus_Status_t *CanBus_GetStatus(void);


#ifdef __cplusplus
}
#endif

#endif /* __CANBUS */