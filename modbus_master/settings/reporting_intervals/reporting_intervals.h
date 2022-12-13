//
// Created by Maverick on 23/02/21.
//

#ifndef REPORTING_INTERVALS_H
#define REPORTING_INTERVALS_H

#include "api.h"
#include "../settings_common.h"

settings_e Save_node_reporting();

settings_e Save_tlv_reporting();

uint16_t* Get_node_reporting();

uint16_t* Get_tlv_reporting();


void Init_reporting_intervals();

#endif //REPORTING_INTERVALS_H
