//
// Created by sudhi on 11/8/2021.
//

#include <string.h>
#include <stdio.h>
#include "../config/config.h"
#include "../settings/settings.h"
#include "../settings/modbus_settings/modbus_settings.h"
#include "../../iws_libraries/utils/iws.h"
#include "../settings/reporting_intervals/reporting_intervals.h"
#include "../../../../libraries/scheduler/app_scheduler.h"
#include "../query_scheduler/query_scheduler.h"
#include "../driver/modbus_lib.h"
#include "iws_methods.h"
#include "iws_app_specific.h"
#include "../../iws_libraries/utils/iws_defines.h"
#include "iws_reporting/reporting.h"
#include "../../iws_libraries/nrf/_nrf_api/nrf_delay.h"


typedef struct {
    uint16_t attrId;
    status_res_e status;
    uint8_t typeID;
    uint8_t queryNumbers[QUERY_SCHEDULER_MAX_TASKS];
} QUERY_NUMS;

/**
 *  brief/         List Attribute Response
 */
void _attr_list() {
    list_attr_res_t attrList[] = { NODE_ATTR_ID, TLV_ATTR_ID, DEBUG_SINK_MESSAGE, MODBUS_SETTINGS_ATTR_ID };
    _send_data((uint8_t *) attrList, sizeof(attrList), APP_ADDR_ANYSINK, LIST_ATTR, LIST_ATTR_RES);
}

/** Read Functions*/
static void Iws_read_debug_send()
{
    read_attr_boolean_res_t debugSinkResponse;
    debugSinkResponse.readAttr.attrId = DEBUG_SINK_MESSAGE;
    debugSinkResponse.readAttr.status = STATUS_RES_SUCCESS;
    debugSinkResponse.readAttr.typeId = TYPE_ID_BOOL;
    debugSinkResponse.data = Is_debug();

    _send_data_QOS_high((uint8_t *)&debugSinkResponse, sizeof(read_attr_boolean_res_t), APP_ADDR_ANYSINK, READ_ATTR, READ_ATTR_RES);
}

//static void Iws_read_modbus_settings() {// uncommented by ram
//    modbus_query_data_t *modSettingsData = Get_Modbus_settings();//modbus_settings_data_t and Get_mod_settings replaced by ram.
//    read_attr_modbus_settings_t modbusSettings;
//    modbusSettings.readAttr.attrId = MODBUS_SETTINGS_ATTR_ID;
//    modbusSettings.readAttr.typeId = TYPE_ID_MODBUS_SETTINGS;
//    modbusSettings.readAttr.status = STATUS_RES_SUCCESS;
//    modbusSettings.modSettings = *modSettingsData;
//
//    _send_data_QOS_high((uint8_t * ) & modbusSettings, sizeof(read_attr_modbus_settings_t), APP_ADDR_ANYSINK, READ_ATTR,
//                        READ_ATTR_RES);
//}

static void Iws_read_error(uint16_t attributeId)
{
    read_error_res_t res;
    res.attrId = attributeId;
    res.status = STATUS_RES_ATTR_NOT_SUPPORTED;

    _send_data_QOS_high((uint8_t *)&res, sizeof(read_error_res_t), APP_ADDR_ANYSINK, READ_ATTR, READ_ATTR_RES);
}

/**
 *  brief/         Read Attribute Response
 */
void _read_attr(const app_lib_data_received_t *data) {
    uint16_t attributeId;
    uint8_t lsByteOfAttrid = 0, msByteOfAttrid = 0;

    //query_scheduler_res_e res;

    lsByteOfAttrid = *(data->bytes);
    msByteOfAttrid = *(data->bytes + 1);
    attributeId = getVal_ws((uint16_t) msByteOfAttrid, (uint16_t) lsByteOfAttrid);
    query_read_t var;
    if (attributeId == DEBUG_SINK_MESSAGE) {
        Iws_read_debug_send();
    } else if (attributeId == MODBUS_SETTINGS_ATTR_ID) {//uncommented by ram
        memcpy(&var, data->bytes + 3, data->num_bytes - 3);
        read_attr_modbus_query_t readResponse;

        modbus_query_data_t *modbusQueryData = Get_Modbus_settings();

        if(var.read_rmv == 1) {// to read the content of the query.
            uint8_t index;
            readResponse.readAttr.attrId = attributeId;
            readResponse.readAttr.typeId = TYPE_ID_MODBUS_SETTINGS;
            for (index = 0; index < QUERY_SCHEDULER_MAX_TASKS; index++) {
                if (modbusQueryData[index].queryId == var.queryID) {
                    //memcpy(&readResponse.modSettings, modbusQueryData, sizeof(modbus_query_data_t));
                    readResponse.modSettings = modbusQueryData[index]; //check if this works.
                    readResponse.readAttr.status = STATUS_RES_SUCCESS;
                    break;
                }
                //modbusQueryData += sizeof(modbus_query_data_t);
            }

            _send_data_QOS_high((uint8_t * ) & readResponse, sizeof(readResponse), APP_ADDR_ANYSINK,
                                READ_ATTR, READ_ATTR_RES);
        }
    }
    //Iws_read_modbus_settings();
    else {
        Iws_read_error(attributeId);
    }
}


/**
 *  brief/         Write Attribute Response
 */
void _write_attr(const app_lib_data_received_t *data) {
    write_attr_res_t writeRes = {
            .status = STATUS_RES_SUCCESS
    };

    writeRes.attrId = getVal_ws((uint16_t) *(data->bytes + 1), (uint16_t) *(data->bytes));
    if (writeRes.attrId == DEBUG_SINK_MESSAGE) {
        Set_debug((bool) *(data->bytes + 3));
    } else if (writeRes.attrId == MODBUS_SETTINGS_ATTR_ID) {
        query_scheduler_res_e res;
        modbus_query_data_t modbusSettings;
        memcpy(&modbusSettings, &data->bytes[3] , data->num_bytes-3);
        DEBUG_SEND(Is_debug(), modbusSettings);
        MODBUS_MASTER_QUERY query = {
                .queryId = modbusSettings.queryId,
                .u8id = modbusSettings.slaveId,
                .deviceDetail = {modbusSettings.deviceDetails.deviceId,modbusSettings.deviceDetails.status,modbusSettings.deviceDetails.attrId},
                //.deviceDetail.deviceId = modbusSettings.deviceDetails.deviceId,
                //.deviceDetail.attrId = modbusSettings.deviceDetails.attrId,
                .u8fct = modbusSettings.functionCode,
                .u16RegAdd = modbusSettings.startAddr,
                .u16CoilsNo = modbusSettings.length,
                .interval = modbusSettings.interval,
                .oneTime = modbusSettings.oneTime,
                //.dataType = modbusSettings.dataType,
                .writeOps = modbusSettings.writeOps,
                .dataLength = modbusSettings.dataLength,
        };
        memcpy(&query.writeData, &modbusSettings.writeData, modbusSettings.dataLength);
        if((modbusSettings.oneTime == 0) && (modbusSettings.interval == 0) && (modbusSettings.isEnable == 1)) {
            DEBUG_SEND(Is_debug(), "interval is zero");
            writeRes.status = STATUS_RES_UNSUCCESSFUL;
         }
        else {
             if (modbusSettings.isEnable)
             {
                 DEBUG_SEND(Is_debug(), "Add task");
                // Add_Modbus_query(modbusSettings);
                 res = Query_Scheduler_addTask(query);

             } else {
                 DEBUG_SEND(Is_debug(), "Remove task");
                 Remove_Modbus_query(modbusSettings.queryId);
                 res = Query_Scheduler_cancelTask(query);
             }

             if (res != QUERY_SCHEDULER_RES_OK) {
                 writeRes.status = STATUS_RES_UNSUCCESSFUL;
             }
             else
                 writeRes.status = STATUS_RES_SUCCESS;
        }
    } else if(writeRes.attrId == MODBUS_QUERY_REMOVE) {
        settings_e res;
        res = remove_AllQueries();
        if(res == SETTINGS_OK) {
            writeRes.status = STATUS_RES_SUCCESS;
        }
        else
            writeRes.status = STATUS_RES_UNSUCCESSFUL;

        if(writeRes.status == STATUS_RES_SUCCESS) {
            lib_state->stopStack();
            nrf_delay_ms(100);
            lib_state->startStack();
        }
    }
    else if(writeRes.attrId == MODBUS_UART_CONFIGURATIONS) {
        write_configure_t config;
        settings_e res;
        memcpy(&config, &data->bytes[3], data->num_bytes-3);
        res = configure_Modbus(config);
        if(res != SETTINGS_OK) {
            writeRes.status = STATUS_RES_UNSUCCESSFUL;
        }
        else
            writeRes.status = STATUS_RES_SUCCESS;

        if(writeRes.status == STATUS_RES_SUCCESS) {
            lib_state->stopStack();
            nrf_delay_ms(100);
            lib_state->startStack();
        }
    }
    else if (writeRes.attrId == MODBUS_DELAY_TLV_CONFIGURATIONS) {
        configure_DelayTlv_t var;
        settings_e res;
        memcpy(&var, &data->bytes[3], data->num_bytes-3);
        timeoutDelayTlv = var;
        res = configureTimeoutDelayTlv(var);
        if (res == SETTINGS_OK)
            writeRes.status = STATUS_RES_SUCCESS;
        else
            writeRes.status = STATUS_RES_UNSUCCESSFUL;
    }
    else if (writeRes.attrId == FACTORY_RESET_ATTR_ID) {
        Factory_reset();
        writeRes.status = STATUS_RES_SUCCESS;
    }
    else {
        writeRes.status = STATUS_RES_ATTR_NOT_SUPPORTED;
    }

    if (data->src_endpoint == SINK_EP_INFERRIX)
        _send_data((uint8_t *) &writeRes, sizeof(writeRes), APP_ADDR_ANYSINK, WRITE_ATTR, WRITE_ATTR_RES);
}


/**
 *  brief/       Configure Reporting Response
 *
 */
void _configure_reporting(const app_lib_data_received_t *data) {
    uint8_t lsByteOfAttrid, msByteOfAttrid;     //Two Octet data of attribute id
    uint8_t lsByteOfIntrvl, msByteOfIntrvl;     //Two Octet data of interval
    uint16_t interval;
    uint8_t i = 0, size;
    config_report_res_t configReport[3];

    while (i <= data->num_bytes) {
        if (i % 4 == 0) {
            lsByteOfAttrid = *(data->bytes + i - 4);
            msByteOfAttrid = *(data->bytes + i - 3);
            lsByteOfIntrvl = *(data->bytes + i - 2);
            msByteOfIntrvl = *(data->bytes + i - 1);

            configReport[i / 4 - 1].attrId = getVal_ws((uint16_t) msByteOfAttrid, (uint16_t) lsByteOfAttrid);
            interval = getVal_ws((uint16_t) lsByteOfIntrvl, (uint16_t) msByteOfIntrvl);
            configReport[i / 4 - 1].status = STATUS_RES_SUCCESS;

            if (configReport[i / 4 - 1].attrId == TLV_ATTR_ID) {
                App_Scheduler_cancelTask(Send_tlv_report);
                uint16_t *tlv = Get_tlv_reporting();
                *tlv = interval;
                Save_tlv_reporting();
                App_Scheduler_addTask(Send_tlv_report, APP_SCHEDULER_SCHEDULE_ASAP);
            } else if (configReport[i / 4 - 1].attrId == NODE_ATTR_ID) {
                App_Scheduler_cancelTask(Send_node_info);
                uint16_t *nodeReporting = Get_node_reporting();
                *nodeReporting = interval;
                Save_node_reporting();
                App_Scheduler_addTask(Send_node_info, APP_SCHEDULER_SCHEDULE_ASAP);
            }
            else {
                configReport[i / 4 - 1].status = STATUS_RES_ATTR_NOT_SUPPORTED;
            }
        }
        i++;
    }

    size = (data->num_bytes / 4) * sizeof(configReport[0]);
    if (data->src_endpoint == SINK_EP_INFERRIX)
        _send_data((uint8_t *) configReport, size, APP_ADDR_ANYSINK, CONFIGURE_REPORTING, CONFIGURE_REPORTING_RES);
}


/**
 *  brief/  Read config. reporting interval Response
 *
 */
void _read_configure_reporting(const app_lib_data_received_t *data) {
    uint8_t i = 0, size = 0;
    read_configure_res_t readConfig[4];
    uint8_t lsByteOfAttrid = 0, msByteOfAttrid = 0;

    while (i < data->num_bytes) {
        if (i % 2 != 0) {
            lsByteOfAttrid = *(data->bytes + i - 1);
            msByteOfAttrid = *(data->bytes + i);
            readConfig[i / 2].attrId = getVal_ws((uint16_t) msByteOfAttrid, (uint16_t) lsByteOfAttrid);
            readConfig[i / 2].status = STATUS_RES_SUCCESS;

            if (readConfig[i / 2].attrId == NODE_ATTR_ID) {
                readConfig[i / 2].intrvl = reserveTwoByteData(*Get_node_reporting());
            } else if (readConfig[i / 2].attrId == TLV_ATTR_ID) {
                readConfig[i / 2].intrvl = reserveTwoByteData(*Get_tlv_reporting());
            } else {
                // IWS_LOG("Read Configure not matched\n\r");
                readConfig[i / 2].status = STATUS_RES_ATTR_NOT_SUPPORTED;
                readConfig[i / 2].intrvl = 0;
            }
        }
        i++;
    }

    size = (data->num_bytes / 2) * sizeof(readConfig[0]);
    if (data->src_endpoint == SINK_EP_INFERRIX)
        _send_data((uint8_t *) readConfig, size, APP_ADDR_ANYSINK, READ_REPORTING_CONFIG, READ_REPORTING_CONFIG_RES);
}


/**
 *  Enable particular Attribute
 * */
void _enable_particular_attr(const app_lib_data_received_t *data) {
    uint8_t i = 0;
    enable_attr_res_t enableAttrRes[3];
    uint8_t lsByteOfAttrid = 0, msByteOfAttrid = 0;
    uint8_t dataArray[10] = {0}, position = 0;

    while (i < data->num_bytes) {
        if (i % 2 != 0) {
            lsByteOfAttrid = *(data->bytes + i - 1);
            msByteOfAttrid = *(data->bytes + i);
            enableAttrRes[i / 2].attrId = getVal_ws((uint16_t) msByteOfAttrid, (uint16_t) lsByteOfAttrid);
            enableAttrRes[i / 2].status = STATUS_RES_SUCCESS;
            if (enableAttrRes[i / 2].attrId == NODE_ATTR_ID) {
                App_Scheduler_cancelTask(Send_node_info);
                memcpy(&dataArray[position], (uint8_t *) &enableAttrRes[i / 2], sizeof(enable_attr_res_t));
                position = position + sizeof(enable_attr_res_t);
                App_Scheduler_addTask(Send_node_info, APP_SCHEDULER_SCHEDULE_ASAP);
            } else if (enableAttrRes[i / 2].attrId == TLV_ATTR_ID) {
                App_Scheduler_cancelTask(Send_tlv_report);
                memcpy(&dataArray[position], (uint8_t *) &enableAttrRes[i / 2], sizeof(enable_attr_res_t));
                position = position + sizeof(enable_attr_res_t);
                App_Scheduler_addTask(Send_tlv_report, APP_SCHEDULER_SCHEDULE_ASAP);
            } else {
                enableAttrRes[i / 2].status = STATUS_RES_ATTR_NOT_SUPPORTED;
                memcpy(&dataArray[position], (uint8_t *) &enableAttrRes[i / 2], sizeof(enable_attr_res_t));
                position = position + sizeof(enable_attr_res_t);
            }
        }
        i++;
    }

    if (data->src_endpoint == SINK_EP_INFERRIX)
        _send_data(dataArray, position, APP_ADDR_ANYSINK, ENABLE_ATTR, ENABLE_ATTR_RES);
}


/**
 *   Disable particular Attribute
 * */
void _disable_particular_attr(const app_lib_data_received_t *data) {
    uint8_t i = 0;
    enable_attr_res_t enableAttrRes[3];
    uint8_t lsByteOfAttrid = 0, msByteOfAttrid = 0;
    uint8_t dataArray[10] = {0}, position = 0;

    while (i < data->num_bytes) {
        if (i % 2 != 0) {
            lsByteOfAttrid = *(data->bytes + i - 1);
            msByteOfAttrid = *(data->bytes + i);
            enableAttrRes[i / 2].attrId = getVal_ws((uint16_t) msByteOfAttrid, (uint16_t) lsByteOfAttrid);
            enableAttrRes[i / 2].status = STATUS_RES_SUCCESS;
            if (enableAttrRes[i / 2].attrId == NODE_ATTR_ID) {
                App_Scheduler_cancelTask(Send_node_info);
                memcpy(&dataArray[position], (uint8_t *) &enableAttrRes[i / 2], sizeof(enable_attr_res_t));
                position = position + sizeof(enable_attr_res_t);
            } else if (enableAttrRes[i / 2].attrId == TLV_ATTR_ID) {
                App_Scheduler_cancelTask(Send_tlv_report);
                memcpy(&dataArray[position], (uint8_t *) &enableAttrRes[i / 2], sizeof(enable_attr_res_t));
                position = position + sizeof(enable_attr_res_t);
            } else {
                enableAttrRes[i / 2].status = STATUS_RES_ATTR_NOT_SUPPORTED;
                memcpy(&dataArray[position], (uint8_t *) &enableAttrRes[i / 2], sizeof(enable_attr_res_t));
                position = position + sizeof(enable_attr_res_t);
            }
        }
        i++;
    }

    if (data->src_endpoint == SINK_EP_INFERRIX)
        _send_data(dataArray, position, APP_ADDR_ANYSINK, DISABLE_ATTR, DISABLE_ATTR_RES);
}

void _reboot_node(const app_lib_data_received_t *data) {
    lib_state->stopStack();
}


//    else if (writeRes.attrId == MODBUS_TLV_COUNT_ATTR_ID) {
//        settings_e res;
//        Count_Tlv CountValue;
//        memcpy(&CountValue, &data->bytes[3], data->num_bytes-3);
//        res = updateTlvCount_query(CountValue);
//        if(res != SETTINGS_OK) {
//            writeRes.status = STATUS_RES_UNSUCCESSFUL;
//        }
//        else
//            writeRes.status = STATUS_RES_SUCCESS;
//    }