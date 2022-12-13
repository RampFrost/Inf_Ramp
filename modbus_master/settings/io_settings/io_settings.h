//
// Created by Maverick on 23/02/21.
//

#ifndef MODBUS_SETTINGS_H
#define MODBUS_SETTINGS_H

#include "../settings_common.h"
#include "../../../iws_libraries/utils/iws.h"

typedef enum {
    WIN_IO_8DOM = 1,
    WIN_IO_4DOM,
    WIN_IO_4DDAM,
    WIN_IO_8DDM,
    WIN_IO_16DDM,
    WIN_IO_24DIM,
    WIN_IO_24DOM,
    WIN_IO_8DIM,
    WIN_IO_4DIM,
    NO_DEVICE_SELECTED = 0x25
} io_type_e;

typedef struct __attribute__ ((packed)) {
    read_attr_res_t readAttr;
    io_type_e ioType;
    modbus_settings_data_t modSettings;
} read_attr_modbus_settings_t;


settings_e Default_io_setting();

settings_e Save_io_settings();

settings_e Read_io_settings();

modbus_settings_data_t* Get_io_settings();

#endif //MODBUS_SETTINGS_H
