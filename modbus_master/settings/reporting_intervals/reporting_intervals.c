//
// Created by Maverick on 23/02/21.
//
#include "reporting_intervals.h"
#include "../settings.h"
#include "../../../iws_libraries/utils/iws_defines.h"
#include "../../../iws_libraries/storage/iws_storage.h"

static uint16_t callback_time_tlv = 5 * 60;
static uint16_t callback_time_nodeInfo = 10 * 60;

static settings_e Save_to_memory(uint16_t interval, uint16_t offset) {
    app_lib_mem_area_res_e res;
    uint8_t buffer[sizeof(uint16_t)] = {'\0'};

    buffer[0] = IWS_U16_LO8(interval);
    buffer[1] = IWS_U16_HI8(interval);

    res = Iws_storage_read(buffer, offset, sizeof(uint16_t));

    if (res != APP_LIB_MEM_AREA_RES_OK) {
        return SETTINGS_SAVE_ERROR;
    }

    return SETTINGS_OK;
}

static settings_e Read_from_memory(uint16_t *interval, uint16_t offset) {
    app_lib_mem_area_res_e res;
    uint8_t buffer[sizeof(uint16_t)] = {'\0'};

    res = Iws_storage_read(buffer, offset, sizeof(uint16_t));

    if (res != APP_LIB_MEM_AREA_RES_OK) {
        return SETTINGS_READ_ERROR;
    }

    if (buffer[0] != 0xFF && buffer[1] != 0xFF) {
        *interval = IWS_U16_CONCAT_U8(buffer[0], buffer[1]);
    } else {
        *interval = callback_time_tlv;
    }
    return SETTINGS_OK;
}

static settings_e Read_node_reporting() {
    return Read_from_memory(&callback_time_nodeInfo, NODE_INFO_INTERVAL_START_ADD);
}

static settings_e Read_tlv_reporting() {
    return Read_from_memory(&callback_time_tlv, TLV_INTERVAL_START_ADD);
}

settings_e Save_node_reporting() {
    return Save_to_memory(reserveTwoByteData(callback_time_nodeInfo), NODE_INFO_INTERVAL_START_ADD);
}

settings_e Save_tlv_reporting() {
    return Save_to_memory(reserveTwoByteData(callback_time_tlv), TLV_INTERVAL_START_ADD);
}

uint16_t* Get_node_reporting() {
    return &callback_time_nodeInfo;
}

uint16_t* Get_tlv_reporting() {
    return &callback_time_tlv;
}

void Init_reporting_intervals() {
    Read_tlv_reporting();
    Read_node_reporting();
}
