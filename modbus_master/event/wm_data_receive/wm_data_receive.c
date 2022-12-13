//
// Created by Maverick on 24/02/21.
//

#include "../../settings/modbus_settings/modbus_settings.h"
#include "../../../iws_libraries/utils/iws_methods.h"
#include "../../../iws_libraries/storage/iws_storage.h"
#include "board.h"
#include "../../../iws_libraries/utils/iws.h"
#include "../../../iws_libraries/utils/iws_defines.h"

app_lib_data_receive_res_e Data_received_cb(const app_lib_data_received_t *data) {
    if (data->dest_endpoint == LIST_ATTR) {
        _attr_list(data);
    } else if (data->dest_endpoint == READ_ATTR) {
        if (data->src_endpoint == SINK_EP_INFERRIX)
            _read_attr(data);
    } else if (data->dest_endpoint == WRITE_ATTR) {
        _write_attr(data);
    } else if (data->dest_endpoint == CONFIGURE_REPORTING) {
        _configure_reporting(data);
    } else if (data->dest_endpoint == READ_REPORTING_CONFIG) {
        _read_configure_reporting(data);
    } else if (data->dest_endpoint == ENABLE_ATTR) {
        _enable_particular_attr(data);
    } else if (data->dest_endpoint == DISABLE_ATTR) {
        _disable_particular_attr(data);
    } else if (data->dest_endpoint == MEMORY_DATA) {
        DEBUG_SEND(true, "In memory read");
        if (STORAGE_AREA > 96) {
            DEBUG_SEND(true, "Storage greater than 96 bytes");
            uint8_t buffer[96];
            Iws_storage_read(buffer, 0, 96);
            _send_data(buffer, sizeof(buffer), APP_ADDR_ANYSINK, MEMORY_DATA, MEMORY_DATA);
        }
    } else { ;
    }

    return APP_LIB_DATA_RECEIVE_RES_HANDLED;
}
