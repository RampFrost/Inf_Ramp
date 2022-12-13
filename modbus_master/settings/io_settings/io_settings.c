//
// Created by Maverick on 23/02/21.
//
/*
 *
#include "modbus_settings.h"
#include "../settings.h"
#include "../settings_common.h"
#include "../../../iws_libraries/storage/iws_storage.h"
#include "app_scheduler.h"
#include "../../../iws_libraries/utils/iws_defines.h"
#include <string.h>
#include <stdio.h>

// Todo: Improve on the storage and read methods. Currently we are having duplicity in both scheduler and here.
// Todo: Ideally we should have one array and store the same in the memory. Currently there are two arrays.
// Todo: This c file needs to be re-written to take care of that aspect. And we should have one array in query_scheduler
// Todo: which stores the modbus query with their schedules.

static modbus_query_data_t modbus_query_list[QUERY_SCHEDULER_MAX_TASKS];

static settings_e Save_Modbus_settings(void) {
    iws_storage_res_e res;
    uint8_t buffer[MODBUS_QUERY_STORAGE_SIZE] = {'\0'};

    uint16_t idx = 0;
    for (uint8_t i = 0; i < QUERY_SCHEDULER_MAX_TASKS; i++) {
        if (idx < MODBUS_QUERY_STORAGE_SIZE) {
            memcpy(&buffer[idx], &modbus_query_list[i], sizeof(modbus_query_data_t));
        }
        idx = idx + sizeof(modbus_query_data_t);
    }

    res = Iws_storage_write(buffer, MODBUS_SETTINGS_STORAGE_START_ADD, MODBUS_QUERY_STORAGE_SIZE);

    if (res != IWS_STORAGE_RES_OK) {
        return SETTINGS_SAVE_ERROR;
    }

    return SETTINGS_OK;
}

static settings_e Read_Modbus_settings() {
    iws_storage_res_e res;
    uint8_t buffer[MODBUS_QUERY_STORAGE_SIZE] = {'\0'};

    res = Iws_storage_read(buffer, MODBUS_SETTINGS_STORAGE_START_ADD, MODBUS_QUERY_STORAGE_SIZE);

    if (res != IWS_STORAGE_RES_OK) {
        return SETTINGS_READ_ERROR;
    }

    for (uint8_t i = 0; i < QUERY_SCHEDULER_MAX_TASKS; i++) {
        memset(&modbus_query_list[i], 0xFF, sizeof(modbus_query_data_t));
    }

    uint16_t idx = 0;
    for (uint8_t i = 0; i < QUERY_SCHEDULER_MAX_TASKS; i++) {
        if (idx < MODBUS_QUERY_STORAGE_SIZE) {
            memcpy(&modbus_query_list[i], &buffer[idx], sizeof(modbus_query_data_t));
            idx = idx + sizeof(modbus_query_data_t);
        }
    }

    return SETTINGS_OK;
}

void Add_Modbus_query(modbus_query_data_t query) {
    for (uint8_t i = 0; i < QUERY_SCHEDULER_MAX_TASKS; i++) {
        if (modbus_query_list[i].queryId == query.queryId || modbus_query_list[i].queryId == 0xFF) {
            memcpy(&modbus_query_list[i], &query, sizeof(modbus_query_data_t));
            break;
        }
    }
    Save_Modbus_settings();
}

void Remove_Modbus_query(uint8_t queryId) {
    for (uint8_t i = 0; i < QUERY_SCHEDULER_MAX_TASKS; i++) {
        if (modbus_query_list[i].queryId == queryId) {
            memset(&modbus_query_list[i], 0xFF, sizeof(modbus_query_data_t));
        }
    }
    Save_Modbus_settings();
}

void Init_Modbus_settings() {
    Read_Modbus_settings();
}

modbus_query_data_t* Get_Modbus_settings() {
    return modbus_query_list;
}


 */