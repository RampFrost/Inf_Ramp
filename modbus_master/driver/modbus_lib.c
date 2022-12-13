/**
 * @file modbus_lib.c
 * 
 * @author Anil Joseph
 * 
 * @brief MODBUS RTU library implemented on NRF52 MCU
 *
 * Modbus RTU is an open serial protocol derived from the Master/Slave architecture.
 * It is a widely accepted serial level protocol due to its ease of use and reliability.
 * Modbus RTU messages are a simple 16-bit structure with a CRC (Cyclic-Redundant Checksum).
 * The simplicity of these messages is to ensure reliability and relatively easy to deploy
 * and maintain compared to other standards. 
 * 
 * This MODBUS library is designed & developed to use NRF52 UART peripheral as a physical medium for serial 
 * communication and makes use of a compact, binary representation of the data for protocol communication.
 * 
 */

// DOM-IGNORE-BEGIN
/******************************************************************************************************************** 
Copyright (c) ** NEED TO ADD COPYRIGHT NOTE HERE **. All rights reserved.

********************************************************************************************************************/

// *****************************************************************************************************************
// *****************************************************************************************************************
// Section: Included Files
// *****************************************************************************************************************
// *****************************************************************************************************************
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include "hal_api.h"
#include "api.h"
#include "modbus_lib.h"
#include "../../../../mcu/hal_api/usart.h"
//#include "../../../../mcu/hal_api/"
#include "../../../../libraries/scheduler/app_scheduler.h"
#include "../../iws_libraries/utils/iws_defines.h"
#include "../../iws_libraries/utils/iws.h"
#include "board.h"
#include "../settings/settings.h"
#include "../settings/modbus_settings/modbus_settings.h"
// *****************************************************************************************************************
// *****************************************************************************************************************
// Section: Pre-processor/Macro Definitions
// *****************************************************************************************************************
// *****************************************************************************************************************
#define MODBUS_RTU_FRAME_SIZE           (8)
#define MODBUS_MASTER_REPLY_TIMEOUT     (1000) //1 sec
#define QUERY_SIZE 60
// *****************************************************************************************************************
// *****************************************************************************************************************

/* Struct to ensure no repetitions. */
typedef struct {
    uint8_t queryID;
    uint8_t Arr[MODBUS_MAX_REGISTER_SIZE];
} CheckData;

// Section: Static / Global Variables
// *****************************************************************************************************************
// *****************************************************************************************************************

/* MODBUS RTU Handle */
static MODBUS_HANDLER modbusHandler;

/* MODBUS Master Query */
static MODBUS_MASTER_QUERY modbusMasterQuery;
/* Time out Period */
//static uint16_t ModbusMasterReplyTimeout;
/* MODBUS Frame Flags */
static bool modbusRtuSlaveModeValidFrameReceived = false;
static bool modbusRtuMasterModeValidFrameReceived = true;
static bool modbusRtuMasterReplyTimeoutActive = true;
static uint8_t modbusRtuMasterReplyActualSize = 0;
static uint8_t modbusMasterReplyCalculatedLength = 0;
static uint32_t mRxBufferIdx;
static bool rxSlaveStatus = true;
static bool timeOutFirstRun = true;
static bool sendDataTLV = false;
write_configure_t configSetting;
/* MODBUS RX Frame Buffer */
static uint8_t modbusSlaveRxFrameBuffer[MODBUS_RTU_FRAME_SIZE] = {0};
/* Modbus_TLV_Data*/
Modbus_TLV_Data_t  modbus_TLV_Data;


/* Temp array to eliminate repetitive data on TLV*/
static CheckData dataRepeated[QUERY_SIZE];

/* MODBUS supported function list */
const unsigned char modbusFunctions[] =
        {
                MB_FC_READ_COILS,
                MB_FC_READ_DISCRETE_INPUT,
                MB_FC_READ_HOLDING_REGISTER,
                MB_FC_READ_INPUT_REGISTER,
                MB_FC_WRITE_COIL,
                MB_FC_WRITE_REGISTER,
                MB_FC_WRITE_MULTIPLE_COILS,
                MB_FC_WRITE_MULTIPLE_REGISTERS
        };

// *****************************************************************************************************************

// *****************************************************************************************************************
// Section: Static Function Declarations
// *****************************************************************************************************************
// *****************************************************************************************************************
static void modbus_rtu_uart_callback(uint8_t *chars, size_t n);
static uint32_t modbusMasterReplyTimeoutCallBack();
static uint8_t getModbusRxBuffer(MODBUS_HANDLER *modH);
static bool sendTxBuffer(MODBUS_HANDLER *modH);
static void buildException(uint8_t u8exception, MODBUS_HANDLER *modH);
static uint8_t validateRequest(MODBUS_HANDLER *modH);
static int8_t transmitMasterQuery(MODBUS_HANDLER *modH, MODBUS_MASTER_QUERY *f_masterQuery);
static int8_t validateAnswer(MODBUS_HANDLER *modH);
static uint16_t word(uint8_t H, uint8_t l);
static uint16_t calcCRC(uint8_t *Buffer, uint8_t u8length);
static void calculateModbusMasterReplyLength(MODBUS_MASTER_QUERY *f_masterQuery);
static int8_t process_FC1(MODBUS_HANDLER *modH);
static int8_t process_FC3(MODBUS_HANDLER *modH);
static int8_t process_FC5(MODBUS_HANDLER *modH);
static int8_t process_FC6(MODBUS_HANDLER *modH);
static int8_t process_FC15(MODBUS_HANDLER *modH);
static int8_t process_FC16(MODBUS_HANDLER *modH);
static void get_FC1(MODBUS_HANDLER *modH);
static void get_FC3(MODBUS_HANDLER *modH);
static void get_FC5(MODBUS_HANDLER *modH);
static void byte_Count(MODBUS_HANDLER *modH);
static bool compareData(uint8_t queryNum);
__STATIC_INLINE void writeRxMasterBuffer(uint8_t ch);
__STATIC_INLINE void writeRxSlaveBuffer(uint8_t ch);

// *****************************************************************************************************************
// *****************************************************************************************************************
// Section: Function Definitions
// *****************************************************************************************************************
// *****************************************************************************************************************

/**
 * @brief Initializes the MODBUS library.
 * 
 * This routine initializes the NRF52 MODBUS library based on the MODBUS handler parameters
 * configured by the application.
 *
 * @param MODBUS_HANDLER MODBUS handler pointer
 * 
 * @param app_uart_comm_params_t UART configuration parameters
 * 
 * @return None
 */
void modbusRtuInitialize(MODBUS_HANDLER *f_modbusHandler) {
    bool status = true;
    DEBUG_SEND(Is_debug(), "modbus RtuInitialize ");
    write_configure_t configure = getConfiguration();
    memset(&dataRepeated[0], '\0', sizeof(dataRepeated));
    DEBUG_SEND(Is_debug(), "timeout is");
    DEBUG_SEND(Is_debug(), timeoutDelayTlv.timeoutPeriod);
    DEBUG_SEND(Is_debug(), "Delay is ");
    DEBUG_SEND(Is_debug(), timeoutDelayTlv.delay);
    if (timeoutDelayTlv.continuousOnTlv){
        DEBUG_SEND(Is_debug(), "continuous data on TLV");
    }
    else {
        DEBUG_SEND(Is_debug(), "changed data on TLV");
    }

    /* Initialize the UART peripheral */
    switch(configure.baudrate) {
        case 0:
            f_modbusHandler->baudRate = 9600;
            break;
        case 1:
            f_modbusHandler->baudRate = 19200;
            break;
        case 2:
            f_modbusHandler->baudRate = 38400;
            break;
        case 3:
            f_modbusHandler->baudRate = 57600;
            break;
        case 4:
            f_modbusHandler->baudRate = 115200;
            break;
        default:
            DEBUG_SEND(Is_debug(), "default baud");
            break;
    }

//    if(configure.parity == 2) {
//        NRF_UART0->CONFIG = 14; //for even parity.
//        DEBUG_SEND(Is_debug(), "even");
//    }
//    else if(configure.parity == 0) {
//        NRF_UART0->CONFIG = 0; //for no parity.
//        DEBUG_SEND(Is_debug(), "no");
//    }
    status = Usart_init(f_modbusHandler->baudRate, UART_FLOW_CONTROL_NONE);
    Usart_setEnabled(true);
    Usart_receiverOn();
    Usart_enableReceiver(modbus_rtu_uart_callback);

    IWS_VALIDATE(status == true);

    /* initialize the statistics */
    f_modbusHandler->u8BufferSize = 0;
    f_modbusHandler->u16InCnt = f_modbusHandler->u16OutCnt = f_modbusHandler->u16errCnt = 0;

    /* Assign the initialized MODBUS structure */
    modbusHandler = *f_modbusHandler;
}

/**
 * @brief MODBUS Master Reply Timeout Callback.
 * 
 * This call back function will be triggered if the slave didn't respond for a master query within 1 sec.
 */
static uint32_t modbusMasterReplyTimeoutCallBack() {
    MODBUS_HANDLER *modH = &modbusHandler;
    if (timeOutFirstRun) {
        timeOutFirstRun = false;
        return (uint32_t) (timeoutDelayTlv.timeoutPeriod);
    }

    DEBUG_SEND(Is_debug(), "Timeout achieved");
    modbusRtuMasterReplyTimeoutActive = true;
    modH->i8state = COM_IDLE;
    modH->i8lastError = NO_REPLY;
    modH->u16errCnt++;
    modH->masterQueryActive = false;
    modbusRtuMasterModeValidFrameReceived = false;
    modbusMasterReplyCalculatedLength = 0;

    if(modbusRtuMasterReplyTimeoutActive){
        modbus_TLV_Data.detail.status = ERR_TIME_OUT;
    _send_data((uint8_t *) &modbus_TLV_Data, 6, APP_ADDR_ANYSINK, MODBUS_TLV_EP, MODBUS_TLV_EP);

    }
    return APP_SCHEDULER_STOP_TASK;
}

/**
 * @brief UART peripheral call back function.
 * 
 * This call back function handles the UART events & pass it on to MODBUS RTU library.
 *
 * @param app_uart_evt_t UART event handle
 * 
 * @return None
 */
static void modbus_rtu_uart_callback(uint8_t *chars, size_t n) {
    MODBUS_HANDLER *modH = &modbusHandler;
    uint8_t ch;
    uint8_t send_Bytes = 6;

    if (n == 0 || n >= UART_RX_BUF_SIZE)
        return;

    while (n--) {
        ch = *(chars++);

        if (modbusHandler.uiModbusType == MODBUS_SLAVE_RTU) {
            writeRxSlaveBuffer(ch);
            /* UART RX buffer has some data ready to be read */
            if ((modbusRtuSlaveModeValidFrameReceived == false) && (mRxBufferIdx >= MODBUS_RTU_FRAME_SIZE)) {
                if (rxSlaveStatus != false) {
                    modbusRtuSlaveModeValidFrameReceived = true;
                } else {
                    modbusRtuSlaveModeValidFrameReceived = false;
                }
                mRxBufferIdx = 0;
            }
        } else if (modbusHandler.uiModbusType == MODBUS_MASTER_RTU) {
            writeRxMasterBuffer(ch);
            /* UART RX buffer has some data ready to be read */

            if ((modbusRtuMasterModeValidFrameReceived == false) &&//should not be true every time for write
                (mRxBufferIdx >= modbusMasterReplyCalculatedLength)) {

                modbusRtuMasterReplyActualSize = mRxBufferIdx;
                modbusRtuMasterModeValidFrameReceived = (modbusRtuMasterReplyActualSize > 0) ? true : false;
                mRxBufferIdx = 0;
            }
        } else {
            /* No need to handle any other events other than above */
        }
    }

    if (modbusHandler.uiModbusType == MODBUS_MASTER_RTU) {
        if (((modbusRtuMasterReplyActualSize > 0) && (modbusRtuMasterModeValidFrameReceived != false)) ||
            (modbusRtuMasterReplyTimeoutActive != false)) {
            App_Scheduler_cancelTask(modbusMasterReplyTimeoutCallBack);
            if (modbusRtuMasterReplyTimeoutActive != false) {
                modH->i8state = COM_IDLE;
                modH->i8lastError = NO_REPLY;
                modH->u16errCnt++;
                modH->masterQueryActive = false;
                modbus_TLV_Data.detail.status = NO_REPLY;
            } else {
                modH->u8BufferSize = modbusRtuMasterReplyActualSize;
                if (modH->u8BufferSize < 6) {
                    modH->i8state = COM_IDLE;
                    modH->i8lastError = ERR_BAD_SIZE;
                    modH->u16errCnt++;
                    modH->masterQueryActive = false;

                    modbus_TLV_Data.detail.status = ERR_BAD_SIZE;
                    _send_data((uint8_t *) &modbus_TLV_Data, send_Bytes, APP_ADDR_ANYSINK, MODBUS_TLV_EP, MODBUS_TLV_EP);
                } else  {
                    // validate message: id, CRC, FCT, exception

                    int8_t u8exception = validateAnswer(modH);
                    if (u8exception != 0) {
                        modH->i8state = COM_IDLE;
                        modH->i8lastError = u8exception;
                    }

                    modH->i8lastError = u8exception;
                    //uint8_t u8byte = modH->au8Buffer[2];

                    // process answer

                    switch (modH->au8Buffer[FUNC]) {
                        case MB_FC_READ_COILS:
                        case MB_FC_READ_DISCRETE_INPUT: {
                            //call get_FC1 to transfer the incoming message to au16regs buffer
                            get_FC1(modH);
                            if (u8exception != 0) {
                                modbus_TLV_Data.detail.status = ERR_EXCEPTION;
                            } else {
                                modbus_TLV_Data.detail.status = ERR_OK;
                            }
                            byte_Count(&modbusHandler);
                            memcpy(modbus_TLV_Data.arrData, modH->au16regs, modbus_TLV_Data.byte_No);
                            send_Bytes += modbus_TLV_Data.byte_No;
                            if (timeoutDelayTlv.continuousOnTlv) {
                                _send_data((uint8_t * ) & modbus_TLV_Data, send_Bytes, APP_ADDR_ANYSINK, MODBUS_TLV_EP,
                                           MODBUS_TLV_EP);
                            }
                            else {
                                if (modbusMasterQuery.oneTime) {
                                    _send_data((uint8_t * ) & modbus_TLV_Data, send_Bytes, APP_ADDR_ANYSINK, MODBUS_TLV_EP,
                                           MODBUS_TLV_EP);
                                }
                                else {
                                    for (uint8_t indx = 0; indx < QUERY_SIZE; indx++) {
                                        if (dataRepeated[indx].queryID == modbusMasterQuery.queryId) {
                                            sendDataTLV = compareData(modbusMasterQuery.queryId);
                                            break;
                                        }
                                        else if ((dataRepeated[indx].queryID != modbusMasterQuery.queryId) &&
                                               (dataRepeated[indx].queryID == 0x00)) {
                                            dataRepeated[indx].queryID = modbusMasterQuery.queryId;
                                            memcpy(&dataRepeated[indx].Arr, &modbus_TLV_Data.arrData,modbus_TLV_Data.byte_No);
                                            sendDataTLV = false;
                                            break;
                                        }
                                    }
                                    if (sendDataTLV == false)
                                        _send_data((uint8_t * ) & modbus_TLV_Data, send_Bytes, APP_ADDR_ANYSINK,
                                               MODBUS_TLV_EP,
                                               MODBUS_TLV_EP);
                                }
                            }
                        }
                        break;

                        case MB_FC_READ_INPUT_REGISTER:
                        case MB_FC_READ_HOLDING_REGISTER: {
                            // call get_FC3 to transfer the incoming message to au16regs buffer
                            get_FC3(modH);
                            if (u8exception != 0) {
                                modbus_TLV_Data.detail.status = ERR_EXCEPTION;
                            } else
                                modbus_TLV_Data.detail.status = ERR_OK;
                            byte_Count(&modbusHandler);
                            memcpy(modbus_TLV_Data.arrData, modH->au16regs, modbus_TLV_Data.byte_No);
                            send_Bytes += modbus_TLV_Data.byte_No;
                            if (timeoutDelayTlv.continuousOnTlv) {
                                _send_data((uint8_t * ) & modbus_TLV_Data, send_Bytes, APP_ADDR_ANYSINK, MODBUS_TLV_EP,
                                           MODBUS_TLV_EP);
                            } else {
                                if (modbusMasterQuery.oneTime) {
                                    _send_data((uint8_t * ) & modbus_TLV_Data, send_Bytes, APP_ADDR_ANYSINK,
                                               MODBUS_TLV_EP,
                                               MODBUS_TLV_EP);
                                } else {
                                    for (uint8_t indx = 0; indx < QUERY_SIZE; indx++) {
                                        if (dataRepeated[indx].queryID == modbusMasterQuery.queryId) {
                                            sendDataTLV = compareData(modbusMasterQuery.queryId);
                                            break;
                                        }
                                        else if (dataRepeated[indx].queryID == 0) {
                                            dataRepeated[indx].queryID = modbusMasterQuery.queryId;
                                            memcpy(&dataRepeated[indx].Arr, &modbus_TLV_Data.arrData,
                                                   modbus_TLV_Data.byte_No);
                                            sendDataTLV = false;
                                            break;
                                        }
                                    }
                                    if (sendDataTLV == false) {
                                        _send_data((uint8_t * ) & modbus_TLV_Data, send_Bytes, APP_ADDR_ANYSINK,
                                                   MODBUS_TLV_EP,
                                                   MODBUS_TLV_EP);
                                    }
                                }
                            }
                        }
                        break;

                        case MB_FC_WRITE_COIL:
                        case MB_FC_WRITE_REGISTER:
                        case MB_FC_WRITE_MULTIPLE_REGISTERS:
                        case MB_FC_WRITE_MULTIPLE_COILS: {
                            // call get_FC5 to transfer the incoming message to au16regs buffer.
                            get_FC5(modH);
                            if(u8exception != 0){
                                modbus_TLV_Data.detail.status = ERR_EXCEPTION;
                            }
                            else
                                modbus_TLV_Data.detail.status = ERR_OK;

                            byte_Count(&modbusHandler);
                            memcpy(modbus_TLV_Data.arrData,modH->au16regs,modbus_TLV_Data.byte_No);
                            send_Bytes += modbus_TLV_Data.byte_No;
                            _send_data((uint8_t * ) & modbus_TLV_Data, send_Bytes, APP_ADDR_ANYSINK, MODBUS_TLV_EP,
                                           MODBUS_TLV_EP);
                            // as the data flow is from master to slave.
                        }
                        break;

                        default:
                            break;
                    }
                    modH->i8state = COM_IDLE;
                    modH->masterQueryActive = false;
                }
            }
            modbusRtuMasterModeValidFrameReceived = false;
        }
    }
}


/**
 * @brief Updates MODBUS frame data to internal buffers
 * 
 * @param None
 * 
 * @return No.of bytes copied to MODBUS internal buffers;
 */
static uint8_t getModbusRxBuffer(MODBUS_HANDLER *modH) {
    modH->u8BufferSize = MODBUS_RTU_FRAME_SIZE;
    uint8_t *modBusRxPacket = modbusSlaveRxFrameBuffer;
    memcpy(modH->au8Buffer, modBusRxPacket, modH->u8BufferSize);
    modH->u16InCnt++;
    return modH->u8BufferSize;
}

/**
 * @brief MODBUS Master Round Robin Task
 * 
 * This function implements the MODBUS master operation.
 *
 * @param None
 * 
 * @return None
 */
void RunModbusMasterTask(void) {
    MODBUS_HANDLER *modH = &modbusHandler;
    if (modH->masterQueryActive != false) {
        modbusRtuMasterReplyTimeoutActive = false;
        modbusRtuMasterModeValidFrameReceived = false;
        modbusRtuMasterReplyActualSize = 0;

        /* Format and Send query */
        bool transmitStatus = transmitMasterQuery(modH, &modbusMasterQuery);
        IWS_VALIDATE(transmitStatus == ERR_OK);
    }
}

/**
 * @brief MODBUS Slave Round Robin Task
 * 
 * This function implements the MODBUS slave operation.
 *
 * @param None
 * 
 * @return None
 */
void RunModbusSlaveTask(void) {
    MODBUS_HANDLER *modH = &modbusHandler;

    if (modbusRtuSlaveModeValidFrameReceived != false) {
        modH->i8lastError = 0;
        modH->u8BufferSize = MODBUS_RTU_FRAME_SIZE;

        getModbusRxBuffer(modH);

        // check slave id 
        if (modH->au8Buffer[ID] == modH->u8id) {
            // validate message: CRC, FCT, address and size
            uint8_t u8exception = validateRequest(modH);
            if (u8exception > 0) {
                if (u8exception != NO_REPLY) {
                    buildException(u8exception, modH);
                    sendTxBuffer(modH);
                }
                modH->i8lastError = u8exception;
            } else {
                modH->i8lastError = 0;

                // process message
                switch (modH->au8Buffer[FUNC]) {
                    case MB_FC_READ_COILS:
                    case MB_FC_READ_DISCRETE_INPUT: {
                        modH->i8state = process_FC1(modH);
                    }
                        break;

                    case MB_FC_READ_INPUT_REGISTER:
                    case MB_FC_READ_HOLDING_REGISTER: {
                        modH->i8state = process_FC3(modH);
                    }
                        break;

                    case MB_FC_WRITE_COIL: {
                        modH->i8state = process_FC5(modH);
                    }
                        break;

                    case MB_FC_WRITE_REGISTER: {
                        modH->i8state = process_FC6(modH);
                    }
                        break;

                    case MB_FC_WRITE_MULTIPLE_COILS: {
                        modH->i8state = process_FC15(modH);
                    }
                        break;

                    case MB_FC_WRITE_MULTIPLE_REGISTERS: {
                        modH->i8state = process_FC16(modH);
                    }
                        break;

                    default:
                        break;
                }
            }
        } else {
            /* MODBUS slave ID is invalid. Ignore this frame */
        }

        modbusRtuSlaveModeValidFrameReceived = false;
    }
}

/**
 * @brief
 * This method validates slave incoming messages
 *
 * @param modH Modbus handler
 * 
 * @return 0 if OK, EXCEPTION if anything fails
 */
static uint8_t validateRequest(MODBUS_HANDLER *modH) {
    // check message crc vs calculated crc
    uint16_t u16MsgCRC = ((modH->au8Buffer[modH->u8BufferSize - 2] << 8) | \
                          (modH->au8Buffer[modH->u8BufferSize - 1])); // combine the crc Low & High bytes

    if (calcCRC(modH->au8Buffer, modH->u8BufferSize - 2) != u16MsgCRC) {
        modH->u16errCnt++;
        return NO_REPLY;
    }

    // check fct code
    bool isSupported = false;
    for (uint8_t i = 0; i < sizeof(modbusFunctions); i++) {
        if (modbusFunctions[i] == modH->au8Buffer[FUNC]) {
            isSupported = 1;
            break;
        }
    }
    if (!isSupported) {
        modH->u16errCnt++;
        return EXC_FUNC_CODE;
    }

    // check start address & nb range
    uint16_t u16regs = 0;
    switch (modH->au8Buffer[FUNC]) {
        case MB_FC_READ_COILS:
        case MB_FC_READ_DISCRETE_INPUT:
        case MB_FC_WRITE_MULTIPLE_COILS:
            u16regs = word(modH->au8Buffer[ADD_HI], modH->au8Buffer[ADD_LO]) / 16;
            u16regs += word(modH->au8Buffer[NB_HI], modH->au8Buffer[NB_LO]) / 16;
            if (u16regs > modH->u16regsize) return EXC_ADDR_RANGE;
            break;
        case MB_FC_WRITE_COIL:
            u16regs = word(modH->au8Buffer[ADD_HI], modH->au8Buffer[ADD_LO]) / 16;
            if (u16regs > modH->u16regsize) return EXC_ADDR_RANGE;
            break;
        case MB_FC_WRITE_REGISTER :
            u16regs = word(modH->au8Buffer[ADD_HI], modH->au8Buffer[ADD_LO]);
            if (u16regs > modH->u16regsize) return EXC_ADDR_RANGE;
            break;
        case MB_FC_READ_HOLDING_REGISTER :
        case MB_FC_READ_INPUT_REGISTER :
        case MB_FC_WRITE_MULTIPLE_REGISTERS :
            u16regs = word(modH->au8Buffer[ADD_HI], modH->au8Buffer[ADD_LO]);
            u16regs += word(modH->au8Buffer[NB_HI], modH->au8Buffer[NB_LO]);
            if (u16regs > modH->u16regsize) return EXC_ADDR_RANGE;
            break;
    }

    return 0; // OK, no exception code thrown
}

/**
 * @brief
 * This method creates a word from 2 bytes
 *
 * @return uint16_t (word)
 * @ingroup H  Most significant byte
 * @ingroup L  Less significant byte
 */
static uint16_t word(uint8_t H, uint8_t L) {
    bytesFields W;
    W.u8[0] = L;
    W.u8[1] = H;

    return W.u16[0];
}

/**
 * @brief
 * This method calculates CRC
 *
 * @return uint16_t calculated CRC value for the message
 * @ingroup Buffer
 * @ingroup u8length
 */
static uint16_t calcCRC(uint8_t *Buffer, uint8_t u8length) {
    unsigned int temp, temp2, flag;
    temp = 0xFFFF;
    for (unsigned char i = 0; i < u8length; i++) {
        temp = temp ^ Buffer[i];
        for (unsigned char j = 1; j <= 8; j++) {
            flag = temp & 0x0001;
            temp >>= 1;
            if (flag)
                temp ^= 0xA001;
        }
    }
    // Reverse byte order.
    temp2 = temp >> 8;
    temp = (temp << 8) | temp2;
    temp &= 0xFFFF;
    // the returned value is already swapped
    // crcLo byte is first & crcHi byte is last
    return temp;

}


/**
 * @brief
 * This method builds an exception message
 *
 * @ingroup u8exception exception number
 * @ingroup modH modbus handler
 */
void buildException(uint8_t u8exception, MODBUS_HANDLER *modH) {
    uint8_t u8func = modH->au8Buffer[FUNC];  // get the original FUNC code

    modH->au8Buffer[ID] = modH->u8id;
    modH->au8Buffer[FUNC] = u8func + 0x80;
    modH->au8Buffer[2] = u8exception;
    modH->u8BufferSize = EXCEPTION_SIZE;
}

/**
 * @brief
 * This method transmits au8Buffer to Serial line.
 *
 * @param modH Modbus handler
 * 
 * @return status 
 */
static bool sendTxBuffer(MODBUS_HANDLER *modH) {
    bool status = true;
    uint32_t ret = 0;
    // append CRC to message
    uint16_t u16crc = calcCRC(modH->au8Buffer, modH->u8BufferSize);
    modH->au8Buffer[modH->u8BufferSize] = u16crc >> 8;
    modH->u8BufferSize++;
    modH->au8Buffer[modH->u8BufferSize] = u16crc & 0x00ff;
    modH->u8BufferSize++;

    uint8_t arr[modH->u8BufferSize];
    memcpy(arr, modH->au8Buffer, modH->u8BufferSize);
    DEBUG_SEND(Is_debug(), "Query being sent");
    DEBUG_SEND(Is_debug(), arr);

    nrf_gpio_pin_clear(BOARD_USART_RD_PIN);
    DELAY_IN_MS(25);

    ret = Usart_sendBuffer((void *) modH->au8Buffer, modH->u8BufferSize);

    if (ret != modH->u8BufferSize) {
        status = false;
        DEBUG_SEND(Is_debug(), "data was not sent through usart");
    }
    modH->u8BufferSize = 0;
    // increase message counter
    modH->u16OutCnt++;
    return status;
}

/**
 * @brief
 * This method processes functions 1 & 2
 * This method reads a bit array and transfers it to the master
 *
 * @return u8BufferSize Response to master length
 * @ingroup discrete
 */
static int8_t process_FC1(MODBUS_HANDLER *modH) {
    uint8_t u8currentRegister, u8currentBit, u8bytesno, u8bitsno;
    uint8_t u8CopyBufferSize;
    uint16_t u16currentCoil, u16coil;

    // get the first and last coil from the message
    uint16_t u16StartCoil = word(modH->au8Buffer[ADD_HI], modH->au8Buffer[ADD_LO]);
    uint16_t u16Coilno = word(modH->au8Buffer[NB_HI], modH->au8Buffer[NB_LO]);

    // put the number of bytes in the outcoming message
    u8bytesno = (uint8_t)(u16Coilno / 8);
    if (u16Coilno % 8 != 0) u8bytesno++;

    modH->au8Buffer[ADD_HI] = u8bytesno;
    modH->u8BufferSize = ADD_LO;
    modH->au8Buffer[modH->u8BufferSize + u8bytesno - 1] = 0;

    // read each coil from the register map and put its value inside the outcoming message
    u8bitsno = 0;

    for (u16currentCoil = 0; u16currentCoil < u16Coilno; u16currentCoil++) {
        u16coil = u16StartCoil + u16currentCoil;
        u8currentRegister = (uint8_t)(u16coil / 16);
        u8currentBit = (uint8_t)(u16coil % 16);

        IWS_BIT_WRITE(modH->au8Buffer[modH->u8BufferSize], u8bitsno,
                      IWS_BIT_READ(modH->au16regs[u8currentRegister], u8currentBit));
        u8bitsno++;
        if (u8bitsno > 7) {
            u8bitsno = 0;
            modH->u8BufferSize++;
        }
    }
    // send outcoming message
    if (u16Coilno % 8 != 0) modH->u8BufferSize++;
    u8CopyBufferSize = modH->u8BufferSize + 2;
    sendTxBuffer(modH);
    return u8CopyBufferSize;
}


/**
 * @brief
 * This method processes functions 3 & 4
 * This method reads a word array and transfers it to the master
 *
 * @return u8BufferSize Response to master length
 * @ingroup register
 */
static int8_t process_FC3(MODBUS_HANDLER *modH) {

    uint8_t u8StartAdd = word(modH->au8Buffer[ADD_HI], modH->au8Buffer[ADD_LO]);
    uint8_t u8regsno = word(modH->au8Buffer[NB_HI], modH->au8Buffer[NB_LO]);
    uint8_t u8CopyBufferSize;
    uint8_t i;

    modH->au8Buffer[2] = u8regsno * 2;
    modH->u8BufferSize = 3;

    for (i = u8StartAdd; i < u8StartAdd + u8regsno; i++) {
        modH->au8Buffer[modH->u8BufferSize] = IWS_U16_HI8(modH->au16regs[i]);
        modH->u8BufferSize++;
        modH->au8Buffer[modH->u8BufferSize] = IWS_U16_LO8(modH->au16regs[i]);
        modH->u8BufferSize++;
    }
    u8CopyBufferSize = modH->u8BufferSize + 2;
    sendTxBuffer(modH);

    return u8CopyBufferSize;
}

/**
 * @brief
 * This method processes function 5
 * This method writes a value assigned by the master to a single bit
 *
 * @return u8BufferSize Response to master length
 * @ingroup discrete
 */
static int8_t process_FC5(MODBUS_HANDLER *modH) {
    uint8_t u8currentRegister, u8currentBit;
    uint8_t u8CopyBufferSize;
    uint16_t u16coil = word(modH->au8Buffer[ADD_HI], modH->au8Buffer[ADD_LO]);

    // point to the register and its bit
    u8currentRegister = (uint8_t)(u16coil / 16);
    u8currentBit = (uint8_t)(u16coil % 16);

    // write to coil
    IWS_BIT_WRITE(modH->au16regs[u8currentRegister], u8currentBit, modH->au8Buffer[NB_HI] == 0xff);

    // send answer to master
    modH->u8BufferSize = 6;
    u8CopyBufferSize = modH->u8BufferSize + 2;
    sendTxBuffer(modH);

    return u8CopyBufferSize;
}

/**
 * @brief
 * This method processes function 6
 * This method writes a value assigned by the master to a single word
 *
 * @return u8BufferSize Response to master length
 * @ingroup register
 */
static int8_t process_FC6(MODBUS_HANDLER *modH) {

    uint8_t u8add = word(modH->au8Buffer[ADD_HI], modH->au8Buffer[ADD_LO]);
    uint8_t u8CopyBufferSize;
    uint16_t u16val = word(modH->au8Buffer[NB_HI], modH->au8Buffer[NB_LO]);

    modH->au16regs[u8add] = u16val;

    // keep the same header
    modH->u8BufferSize = RESPONSE_SIZE;

    u8CopyBufferSize = modH->u8BufferSize + 2;
    sendTxBuffer(modH);

    return u8CopyBufferSize;
}

/**
 * @brief
 * This method processes function 15
 * This method writes a bit array assigned by the master
 *
 * @return u8BufferSize Response to master length
 * @ingroup discrete
 */
int8_t process_FC15(MODBUS_HANDLER *modH) {
    uint8_t u8currentRegister, u8currentBit, u8frameByte, u8bitsno;
    uint8_t u8CopyBufferSize;
    uint16_t u16currentCoil, u16coil;
    bool bTemp;
    // get the first and last coil from the message
    uint16_t u16StartCoil = word(modH->au8Buffer[ADD_HI], modH->au8Buffer[ADD_LO]);
    uint16_t u16Coilno = word(modH->au8Buffer[NB_HI], modH->au8Buffer[NB_LO]);

    // read each coil from the register map and put its value inside the outcoming message
    u8bitsno = 0;
    u8frameByte = 7;
    for (u16currentCoil = 0; u16currentCoil < u16Coilno; u16currentCoil++) {
        u16coil = u16StartCoil + u16currentCoil;
        u8currentRegister = (uint8_t)(u16coil / 16);
        u8currentBit = (uint8_t)(u16coil % 16);

        bTemp = IWS_BIT_READ(modH->au8Buffer[u8frameByte], u8bitsno);

        IWS_BIT_WRITE(modH->au16regs[u8currentRegister], u8currentBit, bTemp);

        u8bitsno++;

        if (u8bitsno > 7) {
            u8bitsno = 0;
            u8frameByte++;
        }
    }

    // send outcoming message
    // it's just a copy of the incoming frame until 6th byte
    modH->u8BufferSize = 6;
    u8CopyBufferSize = modH->u8BufferSize + 2;
    sendTxBuffer(modH);
    return u8CopyBufferSize;
}

/**
 * @brief
 * This method processes function 16
 * This method writes a word array assigned by the master
 *
 * @return u8BufferSize Response to master length
 * @ingroup register
 */
static int8_t process_FC16(MODBUS_HANDLER *modH) {
    uint8_t u8StartAdd = modH->au8Buffer[ADD_HI] << 8 | modH->au8Buffer[ADD_LO];
    uint8_t u8regsno = modH->au8Buffer[NB_HI] << 8 | modH->au8Buffer[NB_LO];
    uint8_t u8CopyBufferSize;
    uint8_t i;
    uint16_t temp;

    // build header
    modH->au8Buffer[NB_HI] = 0;
    modH->au8Buffer[NB_LO] = u8regsno;
    modH->u8BufferSize = RESPONSE_SIZE;

    // write registers
    for (i = 0; i < u8regsno; i++) {
        temp = word(
                modH->au8Buffer[(BYTE_CNT + 1) + i * 2],
                modH->au8Buffer[(BYTE_CNT + 2) + i * 2]);

        modH->au16regs[u8StartAdd + i] = temp;
    }
    u8CopyBufferSize = modH->u8BufferSize + 2;
    sendTxBuffer(modH);

    return u8CopyBufferSize;
}

/**
 * @brief
 * This method initiates a MODBUS master query
 *
 * @param  MODBUS handler pointer
 *         MODBUS master structure
 * @return bool status
 */
bool postModbusMasterQuery(MODBUS_MASTER_QUERY *f_masterQuery) {
    MODBUS_HANDLER *modH = &modbusHandler;
    bool status = false;

    modbus_TLV_Data.detail = f_masterQuery->deviceDetail;
    modbus_TLV_Data.slaveID = f_masterQuery->u8id;
    modbus_TLV_Data.byte_No = 0;

    if ((f_masterQuery != NULL) && (modH->masterQueryActive == false) && (modH->i8state == COM_IDLE)) {
        modH->masterQueryActive = true;
        memset(&modbusMasterQuery, 0, sizeof(modbusMasterQuery));
        memcpy(&modbusMasterQuery, f_masterQuery, sizeof(modbusMasterQuery));
        status = true;
    }

    return status;
}

/**
 * @brief
 * *** Only Modbus Master ***
 * Generate a query to a slave with a MODBUS_MASTER_QUERY structure
 * The Master must be in COM_IDLE mode. After it, its state would be COM_WAITING.
 * This method has to be called only in loop() section.
 *
 * @see modbus_t
 * @param modH  modbus handler
 * @param MODBUS_MASTER_QUERY  modbus query structure (id, fct, ...)
 */
static int8_t transmitMasterQuery(MODBUS_HANDLER *modH, MODBUS_MASTER_QUERY *f_masterQuery) {
    uint8_t u8regsno, u8bytesno;
    int8_t error = ERR_OK;

    if (modH->u8id != 0)
        error = ERR_NOT_MASTER;
    if (modH->i8state != COM_IDLE)
        error = ERR_POLLING;
    if ((f_masterQuery->u8id == 0) || (f_masterQuery->u8id > 247))
        error = ERR_BAD_SLAVE_ID;

    if (error) {
        modH->i8lastError = error;
        return error;
    }

    modH->au16regs = f_masterQuery->au16reg;

    // telegram header
    modH->au8Buffer[ID] = f_masterQuery->u8id;
    modH->au8Buffer[FUNC] = f_masterQuery->u8fct;
    modH->au8Buffer[ADD_HI] = IWS_U16_HI8(f_masterQuery->u16RegAdd);
    modH->au8Buffer[ADD_LO] = IWS_U16_LO8(f_masterQuery->u16RegAdd);

    switch (f_masterQuery->u8fct) {
        case MB_FC_READ_COILS:
        case MB_FC_READ_DISCRETE_INPUT:
        case MB_FC_READ_HOLDING_REGISTER:
        case MB_FC_READ_INPUT_REGISTER:
            modH->au8Buffer[NB_HI] = IWS_U16_HI8(f_masterQuery->u16CoilsNo);
            modH->au8Buffer[NB_LO] = IWS_U16_LO8(f_masterQuery->u16CoilsNo);
            modH->u8BufferSize = 6;
            break;
        case MB_FC_WRITE_COIL:
            modH->au8Buffer[NB_HI] = IWS_U16_HI8(f_masterQuery->au16reg[0]); //
            modH->au8Buffer[NB_LO] = IWS_U16_LO8(f_masterQuery->au16reg[0]);
            modH->u8BufferSize = 6;
            break;
        case MB_FC_WRITE_REGISTER:
            modH->au8Buffer[NB_HI] = IWS_U16_HI8(f_masterQuery->au16reg[0]);
            modH->au8Buffer[NB_LO] = IWS_U16_LO8(f_masterQuery->au16reg[0]);
            modH->u8BufferSize = 6;
            break;
        case MB_FC_WRITE_MULTIPLE_COILS:
            u8regsno = f_masterQuery->u16CoilsNo / 16;
            u8bytesno = u8regsno * 2;
            if ((f_masterQuery->u16CoilsNo % 16) != 0) {
                u8bytesno++;
                u8regsno++;
            }
            modH->au8Buffer[NB_HI] = IWS_U16_HI8(f_masterQuery->u16CoilsNo);
            modH->au8Buffer[NB_LO] = IWS_U16_LO8(f_masterQuery->u16CoilsNo);
            modH->au8Buffer[BYTE_CNT] = u8bytesno;
            modH->u8BufferSize = 7;

            for (uint16_t i = 0; i < u8bytesno; i++) {
                if (i % 2) {
                    modH->au8Buffer[modH->u8BufferSize] = IWS_U16_LO8(f_masterQuery->au16reg[i / 2]);
                } else {
                    modH->au8Buffer[modH->u8BufferSize] = IWS_U16_HI8(f_masterQuery->au16reg[i / 2]);
                }
                modH->u8BufferSize++;
            }
            break;

        case MB_FC_WRITE_MULTIPLE_REGISTERS:
            modH->au8Buffer[NB_HI] = IWS_U16_HI8(f_masterQuery->u16CoilsNo);
            modH->au8Buffer[NB_LO] = IWS_U16_LO8(f_masterQuery->u16CoilsNo);
            modH->au8Buffer[BYTE_CNT] = (uint8_t)(f_masterQuery->u16CoilsNo * 2);
            modH->u8BufferSize = 7;

            for (uint16_t i = 0; i < f_masterQuery->u16CoilsNo; i++) {
                modH->au8Buffer[modH->u8BufferSize] = IWS_U16_HI8(f_masterQuery->au16reg[i]);
                modH->u8BufferSize++;

                modH->au8Buffer[modH->u8BufferSize] = IWS_U16_LO8(f_masterQuery->au16reg[i]);
                modH->u8BufferSize++;
            }
            break;
    }
    sendTxBuffer(modH);

    if (f_masterQuery->u8fct == MB_FC_WRITE_COIL || f_masterQuery->u8fct == MB_FC_WRITE_REGISTER
    || f_masterQuery->u8fct == MB_FC_WRITE_MULTIPLE_COILS || f_masterQuery->u8fct == MB_FC_WRITE_MULTIPLE_REGISTERS) {

        calculateModbusMasterReplyLength(f_masterQuery);
        modH->i8state = COM_IDLE;
        modH->i8lastError = 0;
        modH->masterQueryActive = false;
        timeOutFirstRun = true;
        App_Scheduler_addTask_execTime(modbusMasterReplyTimeoutCallBack, APP_SCHEDULER_SCHEDULE_ASAP, 500);
    } else {
        modH->i8state = COM_WAITING;
        modH->i8lastError = 0;
        calculateModbusMasterReplyLength(f_masterQuery);
        /* Start the MODBUS Query reply timeout timer */
        timeOutFirstRun = true;
        App_Scheduler_addTask_execTime(modbusMasterReplyTimeoutCallBack, APP_SCHEDULER_SCHEDULE_ASAP, 500);
    }

    return error;
}

/**
 * @brief
 * This method validates master incoming messages
 *
 * @param Modbus Handler pointer
 * @return 0 if OK, EXCEPTION if anything fails
 */
static int8_t validateAnswer(MODBUS_HANDLER *modH) {
    int8_t errCode = ERR_OK;

    // check message crc vs calculated crc
    uint16_t u16MsgCRC = ((modH->au8Buffer[modH->u8BufferSize - 2] << 8) | modH->au8Buffer[modH->u8BufferSize - 1]);
    if (calcCRC(modH->au8Buffer, modH->u8BufferSize - 2) != u16MsgCRC) {
        modH->u16errCnt++;
        errCode = ERR_BAD_CRC;
        DEBUG_SEND(Is_debug(), "bad CRC");
    }
    // check exception
    if ((modH->au8Buffer[FUNC] & 0x80) != 0) {
        modH->u16errCnt++;
        errCode = ERR_EXCEPTION;
        DEBUG_SEND(Is_debug(), "error Fn Code");
    }
    // check fct code
    bool isSupported = false;
    for (uint8_t i = 0; i < sizeof(modbusFunctions); i++) {
        if (modbusFunctions[i] == modH->au8Buffer[FUNC]) {
            isSupported = 1;
            break;
        }
    }
    if (!isSupported) {
        modH->u16errCnt++;
        errCode = EXC_FUNC_CODE;
        DEBUG_SEND(Is_debug(), "wrong fun code");
    }

    return errCode;
}

/**
 * This method processes functions 1 & 2 (for master)
 * This method puts the slave answer into master data buffer
 *
 * @param MODBUS_HANDLER
 */
static void get_FC1(MODBUS_HANDLER *modH) {
    uint8_t u8byte, i;

    u8byte = 3;

    for (i = 0; i < modH->au8Buffer[2]; i++) {
        if (i % 2) {
            modH->au16regs[i / 2] = word(modH->au8Buffer[i + u8byte], IWS_U16_LO8(modH->au16regs[i / 2]));
        } else {
            modH->au16regs[i / 2] = word(IWS_U16_HI8(modH->au16regs[i / 2]), modH->au8Buffer[i + u8byte]);
        }
    }
}

/**
 * This method processes functions 3 & 4 (for master)
 * This method puts the slave answer into master data buffer
 *
 * @param MODBUS_HANDLER
 */
static void get_FC3(MODBUS_HANDLER *modH) {
    uint8_t u8byte, i;
    u8byte = 3;

    for (i = 0; i < modH->au8Buffer[2] / 2; i++) {
        modH->au16regs[i] = word(modH->au8Buffer[u8byte], modH->au8Buffer[u8byte + 1]);
        u8byte += 2;
    }
}

/**
 * This method processes functions 5,6,15 & 16 (for master)
 * This method puts the slave answer into master data buffer
 *
 * @param MODBUS_HANDLER
 */
static void get_FC5(MODBUS_HANDLER *modH) {
    uint8_t u8byte = 2,i;
    for(i = 0;i<5;i++) {
        if(i<2)
            modH->au16regs[i] = modH->au8Buffer[i];
        else {
            modH->au16regs[i] = word(modH->au8Buffer[u8byte], modH->au8Buffer[u8byte+1]);
            u8byte += 2;
        }
    }
}


/**
 * This method calculates the MODBUS master reply length
 *  
 * @param MODBUS_MASTER_QUERY MODBUS Maste Query
 */
static void calculateModbusMasterReplyLength(MODBUS_MASTER_QUERY *f_masterQuery) {
    modbusMasterReplyCalculatedLength = 0;

    switch (f_masterQuery->u8fct) {
        case MB_FC_READ_COILS:
        case MB_FC_READ_DISCRETE_INPUT: {
            if (f_masterQuery->u16CoilsNo > 8 ) {
                modbusMasterReplyCalculatedLength = (6 + ((f_masterQuery->u16CoilsNo / 8) - 1));
            } else {
                modbusMasterReplyCalculatedLength = 6;
            }
        }
            break;

        case MB_FC_READ_HOLDING_REGISTER:
        case MB_FC_READ_INPUT_REGISTER: {
            modbusMasterReplyCalculatedLength = (5 + (f_masterQuery->u16CoilsNo * 2));
        }
            break;

        case MB_FC_WRITE_COIL:
        case MB_FC_WRITE_REGISTER:
        case MB_FC_WRITE_MULTIPLE_COILS:
        case MB_FC_WRITE_MULTIPLE_REGISTERS: {
            modbusMasterReplyCalculatedLength = 8;
        }
            break;

        default:
            break;
    }
}

__STATIC_INLINE void writeRxMasterBuffer(uint8_t ch) {
    if (mRxBufferIdx < MAX_SIZE_COMMS_BUFFER) {
        modbusHandler.au8Buffer[mRxBufferIdx++] = ch;
    }
}

__STATIC_INLINE void writeRxSlaveBuffer(uint8_t ch) {
    if (mRxBufferIdx < MODBUS_RTU_FRAME_SIZE) {
        modbusSlaveRxFrameBuffer[mRxBufferIdx++] = ch;
        rxSlaveStatus = true;
    } else {
        rxSlaveStatus = false;
    }
}

void byte_Count(MODBUS_HANDLER *modH)
{
    uint8_t num = modbusMasterQuery.u16CoilsNo;

    modbus_TLV_Data.byte_No = 0;

    switch (modH->au8Buffer[FUNC]) {
        case MB_FC_READ_COILS:
        case MB_FC_READ_DISCRETE_INPUT: {
            if (num < 8) {
                modbus_TLV_Data.byte_No = 1;
            } else if ((num % 8) == 0) {
                modbus_TLV_Data.byte_No = (num / 8);
            } else
                modbus_TLV_Data.byte_No = (num / 8) + 1;
        }
            break;
        case MB_FC_READ_HOLDING_REGISTER:
        case MB_FC_READ_INPUT_REGISTER: {
            modbus_TLV_Data.byte_No = num*2;
        }
        break;
        case MB_FC_WRITE_COIL:
        case MB_FC_WRITE_REGISTER:
        case MB_FC_WRITE_MULTIPLE_COILS:
        case MB_FC_WRITE_MULTIPLE_REGISTERS: {
            modbus_TLV_Data.byte_No = 7;
        }
    }
}

static bool compareData(uint8_t queryNum) {
    bool flag = false;
    for(uint8_t indx=0;indx<QUERY_SIZE;indx++) {
        if(dataRepeated[indx].queryID == queryNum) {
            for(uint8_t index=0;index<modbus_TLV_Data.byte_No;index++) {
                if(modbus_TLV_Data.arrData[index] == dataRepeated[indx].Arr[index]) {
                    if(index == (modbus_TLV_Data.byte_No - 1)) {
                        flag = true;

                    }
                }
                else {
                    dataRepeated[indx].queryID = queryNum;
                    memcpy(&dataRepeated[indx].Arr,&modbus_TLV_Data.arrData,modbus_TLV_Data.byte_No);
                    flag = false;
                    break;
                }
            }
            break;
        }
    }
    return flag;
}


