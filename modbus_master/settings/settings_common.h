//
// Created by Maverick on 22/02/21.
//

#ifndef SETTINGS_COMMON_H
#define SETTINGS_COMMON_H

#define TLV_INTERVAL_START_ADD                    0
#define NODE_INFO_INTERVAL_START_ADD              2
#define MODBUS_SETTINGS_STORAGE_START_ADD         4
#define MODBUS_SETTINGS_STORAGE_SIZE              24 // it's the size of Modbus_query_data_t changed from 23 to 24.
#define UART_CONFIGURATION_STORAGE_START          1444
#define UART_CONFIGURATIOIN_STORAGE_SIZE          3
#define TLV_TIMEOUT_DELAY_STORAGE_START           1447
#define TLV_TIMEOUT_DELAY_STORAGE_SIZE            5
#define NODE_ROLE_LL_HEADNODE                   app_lib_settings_create_role(APP_LIB_SETTINGS_ROLE_HEADNODE, APP_LIB_SETTINGS_ROLE_FLAG_LL)

typedef enum {
    SETTINGS_OK = 0x01,
    SETTINGS_SAVE_ERROR = 0x02,
    SETTINGS_READ_ERROR = 0x03,
} settings_e;


#endif // SETTINGS_COMMON_H
