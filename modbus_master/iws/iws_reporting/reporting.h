//
// Created by pc on 9/14/2021.
//

#ifndef WIREPAS_VER_5_1_REPORTING_H
#define WIREPAS_VER_5_1_REPORTING_H

#include "../../settings/modbus_settings/modbus_settings.h" //added by ram


//typedef struct __attribute__((packed)) {// added by ram
//    uint16_t attrId;
//    uint8_t typeId;
//    status_res_e status;
//} report_t;
//
//typedef struct __attribute__((packed)) { // added by ram
//    report_t readAttr;
//    modbus_query_data_t modSettings;
//} read_attr_modbus_settings_t;


typedef struct __attribute__ ((packed)) {
    uint8_t attrId;
    uint8_t typeId;
    uint8_t data;
} report_one_byte_t;

typedef struct __attribute__ ((packed)) {
    uint8_t attrId;
    uint8_t typeId;
    uint16_t data;
} report_two_byte_t;


typedef struct __attribute__ ((packed)) {
    uint8_t deviceId;
    uint8_t attrId;
    uint8_t typeId;
} report_common_field_t;

typedef struct __attribute__ ((packed)) {
    report_common_field_t field;
    uint8_t data;
} modbus_report_one_u_byte_t;

typedef struct __attribute__ ((packed)) {
    report_common_field_t field;
    int8_t data;
} modbus_report_one_byte_t;

typedef struct __attribute__ ((packed)) {
    report_common_field_t field;
    uint16_t data;
} modbus_report_two_u_byte_t;

typedef struct __attribute__ ((packed)) {
    report_common_field_t field;
    int16_t data;
} modbus_report_two_byte_t;

typedef struct __attribute__ ((packed)) {
    report_common_field_t field;
    uint32_t data;
} modbus_report_four_u_byte_t;

typedef struct __attribute__ ((packed)) {
    report_common_field_t field;
    int32_t data;
} modbus_report_four_byte_t;

uint8_t Add_one_byte_value(uint8_t *, uint8_t);
uint8_t Add_two_byte_value(uint8_t *, uint8_t);
uint8_t Add_four_byte_value(uint8_t *, uint8_t);

#endif //WIREPAS_VER_5_1_REPORTING_H
