//
// Created by Maverick on 23/02/21.
//

#ifndef MODBUS_SETTINGS_H
#define MODBUS_SETTINGS_H

#include "../settings_common.h"
#include "../../../iws_libraries/utils/iws.h"
#include "../../query_scheduler/query_scheduler.h"

#define MODBUS_QUERY_STORAGE_SIZE (QUERY_SCHEDULER_MAX_TASKS * MODBUS_SETTINGS_STORAGE_SIZE)
#define MAX_WRITE_DATA_BUFFER 8

typedef struct __attribute__ ((packed)) {
    uint8_t deviceId;
    int8_t status;
    uint16_t attrId;
} device_details_t;

typedef struct __attribute__ ((packed)) {
    uint8_t queryId;
    uint8_t slaveId;
    device_details_t deviceDetails;
    uint8_t  functionCode;
    uint16_t startAddr;
    uint8_t length;
    uint16_t interval;
    bool oneTime;
    bool isEnable;
    bool writeOps;
    uint8_t dataLength;
    uint8_t writeData[MAX_WRITE_DATA_BUFFER];
} modbus_query_data_t;

typedef struct __attribute__ ((packed)) {
    read_attr_res_t readAttr;
    modbus_query_data_t modSettings;
} read_attr_modbus_query_t;

typedef struct {
    uint8_t queryID;
    uint8_t read_rmv; // 1 for read and 0 for delete 2 for read all available query numbers.
} query_read_t;



void Add_Modbus_query(modbus_query_data_t query);
void Remove_Modbus_query(uint8_t queryId);
settings_e remove_AllQueries(void);
void Init_Modbus_settings();
modbus_query_data_t* Get_Modbus_settings();
/**
 * @brief
 * This method configures the modbus master as per slave requirement.
 * @param  write_configure_t configuration data.
 * @return uint8_t status.
 */
settings_e configure_Modbus(write_configure_t config);

/**
 * @brief
 * This method returns the configurations saved in the modbus master .
 * @param  None.
 * @return write_configure_t type configuration data.
 */
write_configure_t getConfiguration(void);

/**
 * @brief MODBUS Timeout Delay continuous data on Tlv configuration.
 * This function implements the configuration mentioned above.
 * @param configure_DelayTlv_t
 * @return settings_e
 */
settings_e configureTimeoutDelayTlv(configure_DelayTlv_t data);

/**
 * @brief MODBUS Timeout Delay continuous data on Tlv configuration Read.
 * This function read the configuration mentioned above.
 * @param None
 * @return Read status
 */
settings_e getConfigureTimeoutDelayTlv(void);

#endif //MODBUS_SETTINGS_H
