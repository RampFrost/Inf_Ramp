//
// Created by Maverick on 24/02/21.
//

#include "settings.h"
#include "modbus_settings/modbus_settings.h"
#include "reporting_intervals/reporting_intervals.h"
#include "../../iws_libraries/storage/iws_storage.h"

bool debug = false;

bool Is_debug() {
   return debug;
}

void Set_debug(bool debugging) {
    debug = debugging;
}

void Init_settings()
{
    Iws_storage_init();
    Init_reporting_intervals();
    Init_Modbus_settings();
}


void Factory_reset()
{
    ;
}