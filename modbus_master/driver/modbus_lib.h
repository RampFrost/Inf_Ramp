#ifndef MODBUS_LIB_H
#define MODBUS_LIB_H

#ifdef __cplusplus
extern "C"
{
#endif

// *****************************************************************************
// *****************************************************************************
// Section: Included Files
// *****************************************************************************
// *****************************************************************************
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
//#include <../query_scheduler/query_scheduler.h>
// *****************************************************************************
// *****************************************************************************
// Section: Pre-processor Definitions
// *****************************************************************************
// *****************************************************************************

#define T35  5
#define MAX_SIZE_COMMS_BUFFER 64 //!< maximum size for the communication buffer in bytes
#define TIMEOUT_MODBUS 1000
#define MAX_TELEGRAMS 2 //Max number of Telegrams for master
#define MAX_WRITE_DATA_BUFFER 8

#define RESPONSE_SIZE  (6)
#define EXCEPTION_SIZE (3)
#define CHECKSUM_SIZE  (2)

#define UART_TX_BUF_SIZE 256                         /**< UART TX buffer size. */
#define UART_RX_BUF_SIZE 256                         /**< UART RX buffer size. */




// *****************************************************************************
// *****************************************************************************
// Section: Type Definitions
// *****************************************************************************
// *****************************************************************************

/**
 * @struct MODBUS_MASTER_QUERY
 * @brief
 * Master query structure:
 * This includes all the necessary fields to make the Master generate a Modbus query.
 * The master needs to fill all the fields in this structure before posting a query to slave.
 */
typedef struct __attribute__((packed)) {
    uint8_t deviceId;
    int8_t status; // added by ram
    uint16_t attrId;
} device_detail_t;

typedef struct {
    uint8_t queryId;                   /*!< Modbus Query Id for the scheduler */
    uint8_t u8id;                      /*!< Slave address between 1 and 247. 0 means broadcast */
    device_detail_t deviceDetail;      /*!< IWS Internal Device Id to identify device types */
    uint8_t u8fct;                     /*!< Function code: 1, 2, 3, 4, 5, 6, 15 or 16 */
    uint16_t u16RegAdd;                /*!< Address of the first register to access at slave/s */
    uint16_t u16CoilsNo;               /*!< Number of coils or registers to access */
    uint16_t *au16reg;                 /*!< Pointer to memory image in master */
    uint16_t interval;                 /*!< Polling interval for the query */
    bool oneTime;                      /*!< Write query */
    //uint8_t dataType;                  /*!< Data type of the response */
    bool writeOps;
    uint8_t dataLength;
    uint8_t writeData[MAX_WRITE_DATA_BUFFER];    /*!< Write data for write operation */
} MODBUS_MASTER_QUERY;

/**
 * @struct MODBUS_MASTER_HANDLER
 * @brief This structure contains all the necessary information needed to 
 *        initialize, configure & use the MODBUS library.
 */
typedef struct {
    uint8_t uiModbusType;                           /*!< MODBUS Mode : Master or Slave */
    uint32_t baudRate;                              /*!< UART BaudRate */
    uint8_t u8id;                                   /*!< Slave ID */
    int8_t i8lastError;                             /*!< Last error type */
    uint8_t au8Buffer[MAX_SIZE_COMMS_BUFFER];       /*!< MODBUS 8 bit comms buffer for Tx & Rx */
    uint8_t u8BufferSize;                           /*!< Size of MODBUS comms buffer */
    uint16_t *au16regs;                             /*!< Pointer to application array for data transmission & reception */
    uint16_t u16InCnt, u16OutCnt, u16errCnt;        /*!< MODBUS debug statistics */
    uint16_t u16regsize;                            /*!< Size of application array */
    int8_t i8state;                                 /*!< MODBUS library state */
    bool masterQueryActive;                         /*!< Indicates whether an active master query is present */
} MODBUS_HANDLER;


/**
 * @enum MODBUS_DEVICE_TYPE
 * @brief
 * Modbus Device Types
 * Indicates all the supported device types.
 */
typedef enum {
    MODBUS_SLAVE_RTU  = 0,
    MODBUS_MASTER_RTU = 1
} MODBUS_DEVICE_TYPE;

/**
 * @enum MODBUS_MESSAGE_TYPE
 * @brief
 * Indexes to telegram frame positions
 */
typedef enum {
    ID       = 0, //!< ID field
    FUNC     = 1, //!< Function code position
    ADD_HI   = 2, //!< Address high byte
    ADD_LO   = 3, //!< Address low byte
    NB_HI    = 4, //!< Number of coils or registers high byte
    NB_LO    = 5, //!< byte counter
    BYTE_CNT = 6, //!< Number of coils or registers low byte
} MODBUS_MESSAGE_TYPE;

/**
 * @enum MODBUS_FUNCTION_CODE
 * @brief
 * Modbus function codes summary.
 * These are the implement function codes either for Master or for Slave.
 *
 */
typedef enum {
    MB_FC_NONE                     = 0,  /*!< null operator */
    MB_FC_READ_COILS               = 1,  /*!< FCT=1  -> read coils */
    MB_FC_READ_DISCRETE_INPUT      = 2,  /*!< FCT=2  -> read digital/discrete inputs */
    MB_FC_READ_HOLDING_REGISTER    = 3,  /*!< FCT=3  -> read holding registers */
    MB_FC_READ_INPUT_REGISTER      = 4,  /*!< FCT=4  -> read input registers */
    MB_FC_WRITE_COIL               = 5,  /*!< FCT=5  -> write single coil or output */
    MB_FC_WRITE_REGISTER           = 6,  /*!< FCT=6  -> write single register */
    MB_FC_WRITE_MULTIPLE_COILS     = 15, /*!< FCT=15 -> write multiple coils or outputs */
    MB_FC_WRITE_MULTIPLE_REGISTERS = 16  /*!< FCT=16 -> write multiple registers */
} MODBUS_FUNCTION_CODE;

typedef enum {
    COM_IDLE = 0,
    COM_WAITING = 1
} MODBUS_COMM_STATES;

typedef enum {
    ONE_BYTE = 1,
    TWO_BYTE = 2,
    FOUR_BYTE = 4
} DATA_TYPES;

typedef enum {
    ERR_OK              =  0,
    ERR_NOT_MASTER      = -1,
    ERR_POLLING         = -2,
    ERR_BUFF_OVERFLOW   = -3,
    ERR_BAD_CRC         = -4,
    ERR_EXCEPTION       = -5,
    ERR_BAD_SIZE        = -6,
    ERR_BAD_ADDRESS     = -7,
    ERR_TIME_OUT        = -8,
    ERR_BAD_SLAVE_ID    = -9
} MODBUS_ERR_LIST;

typedef enum {
    NO_REPLY = 15,//255,
    EXC_FUNC_CODE = 1,
    EXC_ADDR_RANGE = 2,
    EXC_REGS_QUANT = 3,
    EXC_EXECUTE = 4
} MODBUS_EXCEPTION;

typedef union {
    uint8_t   u8[4];
    uint16_t u16[2];
    uint32_t u32;
} bytesFields ;

// *****************************************************************************
// *****************************************************************************
// Section: External Function Declarations
// *****************************************************************************
// *****************************************************************************

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
void modbusRtuInitialize(MODBUS_HANDLER *f_modbusHandler);

/**
 * @brief MODBUS Round Robin Task
 * 
 * This function implements the MODBUS roun robin task.
 *
 * @param None
 * 
 * @return None
 */
void processModbusRoundRobinTask(void);

/**
 * @brief
 * This method initiates a MODBUS master query
 *
 * @param  MODBUS handler pointer
 *         MODBUS master structure
 * @return bool status
 */
bool postModbusMasterQuery(MODBUS_MASTER_QUERY *f_masterQuery);

void RunModbusMasterTask(void);
void RunModbusSlaveTask(void);

/****************************Modbus_TLV_Data Structure**********************/// added by ram.
typedef struct __attribute__((packed))
{
    uint8_t slaveID;
    device_detail_t detail;
    uint8_t byte_No;
    uint8_t arrData[58];
} Modbus_TLV_Data_t;

/**
 * @struct write_configure_t
 * @brief This structure contains the necessary information needed to
 * configure the modbus as per the slave's requirement.
 */
typedef struct {
    uint8_t baudrate;// exact value of baud,0-9600,1-19200,2-38400,3-57600,4-115200.
    uint8_t parity;// 00 for no parity 1 for odd and 2 for even parity.
    //uint8_t stopbit; // 00 means 1 sec or the time in seconds should be provided.
} write_configure_t;

/**
 * @struct configure_setting_t
 * @brief This structure contains the necessary information needed to
 * configure the modbus as per the slave's requirement.
 */
typedef struct {
    uint16_t timeoutPeriod; // default 1000ms.
    uint16_t delay; // default preset 400ms.
    bool continuousOnTlv; //1 for continuous 0 for changed status default preset 0.
} configure_DelayTlv_t;

configure_DelayTlv_t timeoutDelayTlv;



/**********************************/

#ifdef __cplusplus
}
#endif

#endif /* MODBUS_LIB_H */
