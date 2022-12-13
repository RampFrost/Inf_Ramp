include $(SRCS_PATH)config/makefile
include $(SRCS_PATH)driver/makefile
include $(SRCS_PATH)iws/makefile
include $(SRCS_PATH)event/makefile
include $(SRCS_PATH)settings/makefile
include $(SRCS_PATH)query_scheduler/makefile
include $(SRCS_PATH)init/makefile
include $(SRCS_PATH)../iws_libraries/makefile
include $(SRCS_PATH)../iws_libraries/utils/makefile

# You can add more sources here and libraries if needed
INCLUDES += -I$(IWS_STORAGE_PATH)
INCLUDES += -I$(NRF_API_PATH)
SRCS += $(IWS_STORAGE_PATH)iws_storage.c
SRCS += $(IWS_UTILS_PATH)iws.c
SRCS += $(NRF_PATH)nordic/nrf_delay.c

# Use App Scheduler and configure it
APP_SCHEDULER=yes
APP_SCHEDULER_TASKS=10
CFLAGS += -DAPP_SCHEDULER_MAX_EXEC_TIME_US=500
CFLAGS += -DAPP_SCHEDULER_MAX_TASKS=8
# Use App Scheduler and configure it
APP_PERSISTENT=yes

# Use persistent memeory
HAL_PERSISTENT_MEMORY=yes
CFLAGS += -DUSE_PERSISTENT_MEMORY

# define default app version settings
CFLAGS += -DAPP_MAJOR=$(app_major)
CFLAGS += -DAPP_MINOR=$(app_minor)
CFLAGS += -DAPP_MAINTENANCE=$(app_maintenance)
CFLAGS += -DAPP_DEVELOPMENT=$(app_development)

# Use Modbus Lib
# MODBUS_LIB=yes
HAL_UART=yes
USART_MODBUS_USE=yes