/* Copyright 2018 Wirepas Ltd. All Rights Reserved.
 *
 * See file LICENSE.txt for full license details.
 *
 */

#include <stdlib.h>

#include "api.h"
#include "node_configuration.h"
#include "../../../libraries/scheduler/app_scheduler.h"
#include "../iws_libraries/utils/iws.h"
#include "config/config.h"
#include "../iws_libraries/utils/iws_defines.h"
#include "settings/reporting_intervals/reporting_intervals.h"
#include "iws/iws_reporting/reporting.h"
#include "board.h"
#include "init/init.h"
#include "event/wm_data_receive/wm_data_receive.h"
#include "iws/iws_app_specific.h"
#include "settings/settings_common.h"

#define MAX_TASK_EXECUTION_TIME 999
#define DELAYED_START           60 * 1000

#define WP_VERSION          0x13EC             // Wirepas firmware version 5100 => 5.1.0.0

node_information_header_t node = {(uint16_t) INFERRIX_MODBUS, FIRMWARE_VER, HARDWARE_VER,
                                  MANUFACTURER_INFERRIX, WP_VERSION, APP_MAJOR, APP_MINOR, APP_MAINTENANCE, APP_DEVELOPMENT};


uint32_t Send_node_info() {
    uint16_t *nodeReporting = Get_node_reporting();
    _send_data((uint8_t *) &node, sizeof(node), APP_ADDR_ANYSINK, INFO_EP, INFO_EP);
    return (uint32_t) (*nodeReporting * 1000);
}

uint32_t Send_tlv_report(void) {
    uint8_t buff[96];
    uint8_t index = 0;

    uint16_t *reportingInterval = Get_tlv_reporting();
    index += Add_one_byte_value(&buff[index], HEARTBEAT_ATTR_ID);
    _send_data(buff, index, APP_ADDR_ANYSINK, TLV_REPORTING, TLV_REPORTING);
    return (uint32_t) (*reportingInterval * 1000);
}


static uint32_t Delayed_start()
{
//    App_Scheduler_addTask_execTime(Send_node_info, APP_SCHEDULER_SCHEDULE_ASAP, 10);
    App_Scheduler_addTask_execTime(Send_tlv_report, APP_SCHEDULER_SCHEDULE_ASAP, 10);
    App_Scheduler_addTask_execTime(Init_modbus_app, APP_SCHEDULER_SCHEDULE_ASAP, MAX_TASK_EXECUTION_TIME);
    return APP_SCHEDULER_STOP_TASK;
}

static void Configure_pin()
{
    nrf_gpio_cfg_default(BOARD_USART_RD_PIN);
    nrf_gpio_pin_set(BOARD_USART_RD_PIN);

    nrf_gpio_cfg(BOARD_USART_RD_PIN,
                 NRF_GPIO_PIN_DIR_OUTPUT,
                 NRF_GPIO_PIN_INPUT_CONNECT,
                 NRF_GPIO_PIN_NOPULL,
                 NRF_GPIO_PIN_S0S1,
                 NRF_GPIO_PIN_NOSENSE);
}


/**
 * \brief   Initialization callback for application
 *
 * This function is called after hardware has been initialized but the
 * stack is not yet running.
 *
 */
void App_init(const app_global_functions_t * functions)
{
    // Open Wirepas public API
    API_Open(functions);

    App_Scheduler_init();

    // Basic configuration of the node with a unique node address
    if (configureNodeFromBuildParameters() != APP_RES_OK)
    {
        // Could not configure the node
        // It should not happen except if one of the config value is invalid
        return;
    }

    lib_settings->setNodeRole(NODE_ROLE_LL_HEADNODE);
    Configure_pin();

    // Launch two periodic task with different period
    App_Scheduler_addTask_execTime(Delayed_start, DELAYED_START, MAX_TASK_EXECUTION_TIME);
    lib_data->setDataReceivedCb(Data_received_cb);
    lib_data->setBcastDataReceivedCb(Data_received_cb);

    lib_settings->setNodeAddress(119018);

    // Start the stack
    lib_state->startStack();
}
