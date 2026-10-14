#include "CanBus.h"
#include "Can_Addr.h"
#include "can.h"
#include "main.h"
#include <stddef.h>
#include <string.h>


#if ((CANBUS_RX_QUEUE_SIZE & (CANBUS_RX_QUEUE_SIZE - 1U)) != 0U)
#error "CANBUS_RX_QUEUE_SIZE phai la luy thua cua 2"
#endif

#define CANBUS_RX_QUEUE_MASK    (CANBUS_RX_QUEUE_SIZE - 1U)


/* Private variables ------------------------------------------------*/

static CanBus_Status_t s_status =
{
    .state     = CANBUS_STATE_UNINIT,
    .tx_id     = 0U,
    .rx_id     = 0U,
    .err_count = 0U
};


/*
 * Hàng đợi RX:
 * - ISR ghi (s_rxHead)
 * - main loop đọc (s_rxTail)
 *
 * 1 producer / 1 consumer -> không cần tắt ngắt.
 */
static CanBus_Frame_t   s_rxQueue[CANBUS_RX_QUEUE_SIZE];
static volatile uint8_t s_rxHead = 0U;
static volatile uint8_t s_rxTail = 0U;


/*
 * Cờ / bộ đếm từ ISR.
 * ISR chỉ set cờ + đếm, xử lý thật nằm trong CanBus_Process().
 */
static volatile bool     s_busOffFlag = false;
static volatile uint32_t s_errEvents  = 0U;


/*
 * Mốc thời gian lần bus-off / lần thử khởi tạo gần nhất.
 */
static uint32_t s_recoveryTick = 0U;


/* Private functions ------------------------------------------------*/

static bool     CanBus_CheckBitrate(void);
static uint32_t CanBus_FilterReg(uint32_t extId, bool rtr);
static bool     CanBus_ConfigFilterBank(uint32_t bank, uint32_t id, uint32_t mask);
static bool     CanBus_HwStart(void);
static bool     CanBus_Recover(void);


/* Functions --------------------------------------------------------*/

/*
 * Công thức thống nhất address -> CAN ID.
 *
 * DIP (0..63) -> node_id = 0x60 + DIP
 * Hợp lệ khi node_id nằm trong 0x60..0x7E (DIP 0..30).
 *
 * tx_id = MakeId(0, Master, 0, node_id)
 * rx_id = MakeId(0, node_id, 0, 0)
 */
bool CanBus_CalcIds(uint8_t address, uint32_t *pTxId, uint32_t *pRxId)
{
    uint16_t nodeId = (uint16_t)CANBUS_IO_NODE_BASE + (uint16_t)address;


    if (nodeId > CANBUS_IO_NODE_LAST)
    {
        return false;
    }

    if (pTxId != NULL)
    {
        *pTxId = CanBus_MakeId(0U, CANBUS_MASTER_NODE_ID, 0U, (uint8_t)nodeId);
    }

    if (pRxId != NULL)
    {
        *pRxId = CanBus_MakeId(0U, (uint8_t)nodeId, 0U, 0U);
    }

    return true;
}


bool CanBus_Init(void)
{
    const CanAddr_t *pAddr;


    s_rxHead       = 0U;
    s_rxTail       = 0U;
    s_busOffFlag   = false;
    s_errEvents    = 0U;
    s_recoveryTick = HAL_GetTick();


    /*
     * Sai baudrate -> tham gia bus sẽ phá cả bus.
     * Dừng hẳn, không thử lại.
     */
    if (CanBus_CheckBitrate() == false)
    {
        s_status.state = CANBUS_STATE_CONFIG_ERROR;
        return false;
    }


    /*
     * CanAddr_Init() có khóa chỉ đọc DIP 1 lần,
     * gọi lại ở đây không làm thay đổi address đã latch.
     */
    CanAddr_Init();

    pAddr = CanAddr_Get();

    if ((pAddr->valid == false) ||
        (CanBus_CalcIds(pAddr->address, &s_status.tx_id, &s_status.rx_id) == false))
    {
        /*
         * Địa chỉ DIP ngoài dải mạch I/O.
         * KHÔNG tham gia bus -> tránh trùng ID với MCU súng / PM.
         */
        s_status.state = CANBUS_STATE_CONFIG_ERROR;
        return false;
    }


    if (CanBus_HwStart() == false)
    {
        /*
         * Không treo ở Error_Handler(),
         * CanBus_Process() sẽ tự thử lại.
         */
        s_status.state = CANBUS_STATE_INIT_FAIL;
        return false;
    }

    s_status.state = CANBUS_STATE_ERROR_ACTIVE;

    return true;
}


void CanBus_Process(void)
{
    uint32_t now;
    uint32_t esr;


    if ((s_status.state == CANBUS_STATE_UNINIT) ||
        (s_status.state == CANBUS_STATE_CONFIG_ERROR))
    {
        return;
    }

    now = HAL_GetTick();

    s_status.err_count = s_errEvents;


    /*
     * Đang bus-off / khởi tạo lỗi:
     * chờ CANBUS_RECOVERY_DELAY_MS rồi chạy lại chuỗi khởi tạo.
     */
    if ((s_status.state == CANBUS_STATE_BUS_OFF) ||
        (s_status.state == CANBUS_STATE_INIT_FAIL))
    {
        if ((now - s_recoveryTick) >= CANBUS_RECOVERY_DELAY_MS)
        {
            s_recoveryTick = now;

            if (CanBus_Recover() == true)
            {
                s_busOffFlag   = false;
                s_status.state = CANBUS_STATE_ERROR_ACTIVE;
            }
            else
            {
                /* Giữ trạng thái lỗi, lần sau thử lại */
                s_status.state = CANBUS_STATE_INIT_FAIL;
            }
        }

        return;
    }


    /*
     * Phát hiện bus-off:
     * - từ cờ ISR (HAL_CAN_ErrorCallback)
     * - hoặc đọc trực tiếp ESR.BOFF (dự phòng nếu mất ngắt)
     */
    esr = hcan1.Instance->ESR;

    if ((s_busOffFlag == true) || ((esr & CAN_ESR_BOFF) != 0U))
    {
        s_busOffFlag   = false;
        s_status.state = CANBUS_STATE_BUS_OFF;
        s_recoveryTick = now;
        return;
    }


    /*
     * Cập nhật mức lỗi từ phần cứng.
     */
    if ((esr & CAN_ESR_EPVF) != 0U)
    {
        s_status.state = CANBUS_STATE_ERROR_PASSIVE;
    }
    else if ((esr & CAN_ESR_EWGF) != 0U)
    {
        s_status.state = CANBUS_STATE_ERROR_WARNING;
    }
    else
    {
        s_status.state = CANBUS_STATE_ERROR_ACTIVE;
    }
}


bool CanBus_Send(uint8_t prio, uint8_t cmd, const uint8_t *pData, uint8_t len)
{
    CAN_TxHeaderTypeDef header;
    uint32_t            mailbox;


    if ((len > 8U) || ((pData == NULL) && (len > 0U)))
    {
        return false;
    }


    /*
     * Chỉ gửi khi CAN còn tham gia bus.
     */
    if ((s_status.state != CANBUS_STATE_ERROR_ACTIVE)  &&
        (s_status.state != CANBUS_STATE_ERROR_WARNING) &&
        (s_status.state != CANBUS_STATE_ERROR_PASSIVE))
    {
        return false;
    }


    /*
     * Không chặn: hết mailbox thì trả về false ngay.
     */
    if (HAL_CAN_GetTxMailboxesFreeLevel(&hcan1) == 0U)
    {
        return false;
    }


    /*
     * tx_id đã có Source = node_id, Dest = Master.
     * Chỉ thêm Priority + Command.
     */
    header.StdId              = 0U;
    header.ExtId              = s_status.tx_id |
                                CanBus_MakeId(prio, 0U, cmd, 0U);
    header.IDE                = CAN_ID_EXT;
    header.RTR                = CAN_RTR_DATA;
    header.DLC                = len;
    header.TransmitGlobalTime = DISABLE;


    if (HAL_CAN_AddTxMessage(&hcan1, &header, (uint8_t *)pData, &mailbox) != HAL_OK)
    {
        return false;
    }

    return true;
}


bool CanBus_Read(CanBus_Frame_t *pFrame)
{
    uint8_t tail;


    if (pFrame == NULL)
    {
        return false;
    }

    tail = s_rxTail;

    if (tail == s_rxHead)
    {
        return false;   /* Queue rỗng */
    }

    *pFrame = s_rxQueue[tail];

    /*
     * Đảm bảo copy xong rồi mới nhả slot cho ISR.
     */
    __DMB();

    s_rxTail = (uint8_t)((tail + 1U) & CANBUS_RX_QUEUE_MASK);

    return true;
}


const CanBus_Status_t *CanBus_GetStatus(void)
{
    return &s_status;
}


/* Private functions ------------------------------------------------*/

/*
 * Tính baudrate thực tế từ PCLK1 và cấu hình CubeMX:
 *
 *   bitrate = PCLK1 / (Prescaler x (1 + BS1 + BS2))
 *
 * STM32F407, HSE 8 MHz, PLL M=4 N=168 P=2, AHB /2, APB1 /2
 * -> PCLK1 = 42 MHz
 * -> 125 kbps: Prescaler = 21, BS1 = 13 TQ, BS2 = 2 TQ (16 TQ, SP 87.5%)
 */
static bool CanBus_CheckBitrate(void)
{
    uint32_t pclk1;
    uint32_t bs1;
    uint32_t bs2;
    uint32_t div;


    pclk1 = HAL_RCC_GetPCLK1Freq();

    bs1 = ((hcan1.Init.TimeSeg1 >> CAN_BTR_TS1_Pos) & 0x0FUL) + 1UL;
    bs2 = ((hcan1.Init.TimeSeg2 >> CAN_BTR_TS2_Pos) & 0x07UL) + 1UL;
    div = hcan1.Init.Prescaler * (1UL + bs1 + bs2);

    if (div == 0U)
    {
        return false;
    }

    return ((pclk1 % div) == 0U) && ((pclk1 / div) == CANBUS_BITRATE);
}


/*
 * Chuyển Extended ID sang format thanh ghi filter 32-bit (RM0090):
 *
 *   [31:3] = EXID[28:0], bit 2 = IDE, bit 1 = RTR
 */
static uint32_t CanBus_FilterReg(uint32_t extId, bool rtr)
{
    return ((extId & 0x1FFFFFFFUL) << 3) |
           CAN_ID_EXT |
           ((rtr == true) ? CAN_RTR_REMOTE : 0U);
}


static bool CanBus_ConfigFilterBank(uint32_t bank, uint32_t id, uint32_t mask)
{
    CAN_FilterTypeDef filter;


    filter.FilterBank           = bank;
    filter.FilterMode           = CAN_FILTERMODE_IDMASK;
    filter.FilterScale          = CAN_FILTERSCALE_32BIT;
    filter.FilterIdHigh         = (id >> 16) & 0xFFFFU;
    filter.FilterIdLow          = id & 0xFFFFU;
    filter.FilterMaskIdHigh     = (mask >> 16) & 0xFFFFU;
    filter.FilterMaskIdLow      = mask & 0xFFFFU;
    filter.FilterFIFOAssignment = CAN_FILTER_FIFO0;
    filter.FilterActivation     = CAN_FILTER_ENABLE;
    filter.SlaveStartFilterBank = 14U;

    return (HAL_CAN_ConfigFilter(&hcan1, &filter) == HAL_OK);
}


/*
 * Cấu hình filter + ngắt + start CAN.
 *
 * Filter bank 0, mask mode 32-bit, chỉ so khớp:
 * - IDE = 1 (Extended)
 * - RTR = 0 (Data frame)
 * - Dest ID (bit 25..19) = node_id
 * Priority / Command / Source bỏ qua (tầng ứng dụng tự kiểm tra).
 *
 * => Phần cứng chỉ đưa vào FIFO0 đúng các frame gửi cho node này.
 */
static bool CanBus_HwStart(void)
{
    uint32_t mask;
    uint32_t hbId;
    uint32_t hbMask;


    /*
     * Bank 0: nhận mọi frame gửi riêng cho node này (Dest = node_id).
     * Mask chỉ phủ trường Dest; Priority/Command/Source bỏ qua.
     */
    mask = CanBus_FilterReg(CanBus_MakeId(0U, (uint8_t)CANBUS_ID_DEST_MASK, 0U, 0U), true);

    if (CanBus_ConfigFilterBank(0U,
                                CanBus_FilterReg(s_status.rx_id, false),
                                mask) == false)
    {
        return false;
    }

    /*
     * Bank 1: heartbeat 0x5 (Dest = 0x00 VÀ Command = 0x5).
     * Mask phủ cả trường Dest và Command -> không nhận nhầm các frame
     * khác cũng có Dest = 0x00 (vd. node khác trả lời master).
     */
    hbId   = CanBus_FilterReg(
        CanBus_MakeId(0U, CANBUS_HEARTBEAT_DEST, CANBUS_CMD_HEARTBEAT_REQ, 0U), false);
    hbMask = CanBus_FilterReg(
        CanBus_MakeId(0U, (uint8_t)CANBUS_ID_DEST_MASK, (uint8_t)CANBUS_ID_CMD_MASK, 0U), true);

    if (CanBus_ConfigFilterBank(1U, hbId, hbMask) == false)
    {
        return false;
    }


    /*
     * RX dùng ngắt FIFO0, không polling.
     * Lỗi: warning / passive / bus-off / FIFO overrun.
     *
     * KHÔNG bật CAN_IT_LAST_ERROR_CODE: khi bus không có node ACK,
     * mỗi lần retransmit sẽ sinh 1 ngắt -> dễ bão ngắt.
     */
    if (HAL_CAN_ActivateNotification(&hcan1,
                                     CAN_IT_RX_FIFO0_MSG_PENDING |
                                     CAN_IT_RX_FIFO0_OVERRUN     |
                                     CAN_IT_ERROR_WARNING        |
                                     CAN_IT_ERROR_PASSIVE        |
                                     CAN_IT_BUSOFF               |
                                     CAN_IT_ERROR) != HAL_OK)
    {
        return false;
    }


    if (HAL_CAN_Start(&hcan1) != HAL_OK)
    {
        return false;
    }

    return true;
}


/*
 * Chạy lại chuỗi khởi tạo sau bus-off (RM0090, ABOM = 0):
 *
 * - HAL_CAN_DeInit(): stop CAN, tắt NVIC/clock (MspDeInit),
 *   set MCR.RESET -> reset bxCAN, xóa TEC/REC.
 * - HAL_CAN_Init(): dùng lại hcan1.Init do CubeMX sinh
 *   (DeInit không xóa Init), gọi lại MspInit.
 * - Cấu hình lại filter + ngắt + start. Sau khi rời Init mode,
 *   bxCAN tự chờ 128 x 11 bit lặn rồi mới tham gia bus.
 *
 * KHÔNG gọi MX_CAN1_Init() vì hàm đó nhảy vào Error_Handler()
 * (treo cứng) nếu lỗi -> mất khả năng tự phục hồi.
 */
static bool CanBus_Recover(void)
{
    (void)HAL_CAN_DeInit(&hcan1);

    if (HAL_CAN_Init(&hcan1) != HAL_OK)
    {
        return false;
    }

    return CanBus_HwStart();
}


/* HAL callbacks ----------------------------------------------------*/

void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
    CAN_RxHeaderTypeDef header;
    uint8_t             data[8];
    uint8_t             head;
    uint8_t             next;
    uint8_t             dlc;


    if (hcan->Instance != CAN1)
    {
        return;
    }


    /*
     * Đọc hết FIFO0 (tối đa 3 frame).
     */
    while (HAL_CAN_GetRxFifoFillLevel(hcan, CAN_RX_FIFO0) > 0U)
    {
        if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &header, data) != HAL_OK)
        {
            break;
        }

        head = s_rxHead;
        next = (uint8_t)((head + 1U) & CANBUS_RX_QUEUE_MASK);

        if (next == s_rxTail)
        {
            s_errEvents++;   /* Queue đầy, bỏ frame mới */
            continue;
        }

        dlc = (header.DLC > 8U) ? 8U : (uint8_t)header.DLC;

        s_rxQueue[head].id  = header.ExtId;
        s_rxQueue[head].dlc = dlc;
        memset(s_rxQueue[head].data, 0, sizeof(s_rxQueue[head].data));
        memcpy(s_rxQueue[head].data, data, dlc);

        /*
         * Ghi xong dữ liệu rồi mới công bố head mới.
         */
        __DMB();

        s_rxHead = next;
    }
}


void HAL_CAN_ErrorCallback(CAN_HandleTypeDef *hcan)
{
    uint32_t code;


    if (hcan->Instance != CAN1)
    {
        return;
    }

    code = HAL_CAN_GetError(hcan);


    if ((code & HAL_CAN_ERROR_BOF) != 0U)
    {
        s_busOffFlag = true;
    }

    if ((code & (HAL_CAN_ERROR_EWG |
                 HAL_CAN_ERROR_EPV |
                 HAL_CAN_ERROR_BOF |
                 HAL_CAN_ERROR_RX_FOV0)) != 0U)
    {
        s_errEvents++;
    }

    (void)HAL_CAN_ResetError(hcan);
}