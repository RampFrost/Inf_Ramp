//
// Created by Maverick on 22/02/21.
//

#ifndef CONFIG_H
#define CONFIG_H

#define DEBUG_SINK_MESSAGE                  0x7100
#define NODE_ATTR_ID                        0x5B00
#define TLV_ATTR_ID  	                    0x9E00    // TLV Reporting
#define MODBUS_SETTINGS_ATTR_ID             0xA000
#define MODBUS_UART_CONFIGURATIONS          0xB000 // To configure parity(not now) and baudrate.
#define MODBUS_DELAY_TLV_CONFIGURATIONS     0xC000 // to configure
#define FACTORY_RESET_ATTR_ID               0x9900
#define MODBUS_QUERY_REMOVE                 0x6200 //Modbus all query remove attribute
#define HEARTBEAT_ATTR_ID  	                0x01    // Heartbeat Attribute
#define MODBUS_TLV_ATTR_ID                0x5500 // Modbus attribute
#endif // CONFIG_H
