//
// Created by Maverick on 16/12/21.
//

#include "init.h"
#include "../query_scheduler/query_scheduler.h"
#include "../settings/settings.h"
#include "../../iws_libraries/utils/iws_defines.h"
#include "../../../../libraries/scheduler/app_scheduler.h"

uint32_t Init_modbus_app()
{
    Init_settings();
    DELAY_IN_MS(5);
    Query_Scheduler_init();
    return APP_SCHEDULER_STOP_TASK;
}