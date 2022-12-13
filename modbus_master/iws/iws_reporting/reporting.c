//
// Created by pc on 9/14/2021.
//
#include <api.h>
#include <string.h>
#include "../../../iws_libraries/utils/iws.h"
#include "config.h"
#include "reporting.h"
#include "iws_methods.h"
#include "../../settings/settings.h"

bool pirConnected = false;
bool pirActivated = false;

extern uint16_t solar_light_level;
extern uint8_t energy_level;


uint8_t Add_one_byte_value(uint8_t *buff, uint8_t attrId)
{
    report_one_byte_t report;
    report.attrId = attrId;

    if(attrId == HEARTBEAT_ATTR_ID) {
        report.typeId = TYPE_ID_BOOL;
        report.data = (uint8_t)true;
    }

    memcpy(buff, (uint8_t *)&report, sizeof(report));

    return (sizeof(report));
}

uint8_t Add_two_byte_value (uint8_t *buff, uint8_t attrId)
{
    report_two_byte_t report;
    report.attrId = attrId;

    memcpy(buff, (uint8_t *)&report, sizeof(report));
    return (sizeof(report));
}