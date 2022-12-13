/* Copyright 2018 Wirepas Ltd. All Rights Reserved.
 *
 * See file LICENSE.txt for full license details.
 *
 */
#include "query_scheduler.h"
#include "../../../../libraries/scheduler/app_scheduler.h"
#include "util.h"
#include "../driver/modbus_lib.h"
#include "../settings/modbus_settings/modbus_settings.h"
#include <string.h>
#include "../../iws_libraries/utils/iws_defines.h"
#include "../settings/settings.h"
#include "../../iws_libraries/nrf/_nrf_api/nrf_delay.h"

#define TIMEOUT_ADDITION_BETWEEN_QUERY 1500
#define EXEC_TIME 500
#define ADD_ADDITIONAL_DELAY 10

uint8_t count;
/**
 * Maximum periodic task that can be registered at the same time
 * It is application specific
 */
uint16_t ModbusDataRegArray[MODBUS_MAX_REGISTER_SIZE] = {0};

/**
 * Maximum time in ms for periodic work  to be scheduled due to internal
 * stack maximum delay comparison (~30 minutes). Computed in init function
 */
static uint32_t m_max_time_ms;

/**
 * Is library initialized
 */
static bool m_initialized = false;

static MODBUS_HANDLER gModbusInitHandler;

typedef struct
{
    union {
        app_lib_time_timestamp_coarse_t coarse;
        app_lib_time_timestamp_hp_t     hp;
    };
    bool is_hp;
} timestamp_t;

/** Structure of a task */
typedef struct
{
    MODBUS_MASTER_QUERY                 modbus_query; /* Modbus Query of this task */
    timestamp_t                         next_ts;/* When is the next execution */
    bool                                updated; /* Updated in IRQ context? */
    bool                                removed; /* Task removed, to be released */
} task_t;

/**  List of tasks */
static task_t m_tasks[QUERY_SCHEDULER_MAX_TASKS];

/** Next task to be executed */
static task_t * m_next_task_p;

/** Flag for scheduler to know that a reschedule is needed
 *  (task added or removed) */
static bool m_force_reschedule;

/** Forward declaration */
static uint32_t periodic_work(void);


/**
 * \brief   Get a coarse timestamp in future
 * \param   ts_p
 *          Pointer to the timestamp to update
 * \param   ms
 *          In how many ms is the timestamp in future
 */
static void get_timestamp(timestamp_t * ts_p, uint32_t ms)
{
    if (ms < m_max_time_ms)
    {
        // Delay is short enough to use hp timestamp
        ts_p->is_hp = true;
        ts_p->hp = lib_time->addUsToHpTimestamp(
                lib_time->getTimestampHp(),
                ms * 1000);
    }
    else
    {
        ts_p->is_hp = false;

        app_lib_time_timestamp_coarse_t coarse;

        // Initialize timestamp to now
        coarse = lib_time->getTimestampCoarse();

        // Handle the case of ms being > 2^25
        // to avoid overflow when multiplication by 128 (2^7)
        // Using uint64_t cast was an option also at the cost of
        // including additional linked library in final image to
        // handle uin64_t arithmetic
        if ((ms >> 25) != 0)
        {
            uint32_t delay_high;
            // Keep only the highest bits
            delay_high = ms & 0xfe000000;

            // Safe to first divide the delay
            coarse += ((delay_high / 1000) * 128);

            // Remove highest bits
            ms &= 0x01ffffff;
        }

        coarse += ((ms * 128) / 1000);

        // Ceil the value to upper boundary
        // (so in 1ms => ~7.8ms)
        if ((ms * 128) % 1000)
        {
            coarse +=1;
        }

        ts_p->coarse = coarse;
    }
}

/**
 * \brief   Get the delay in us relative to now for a timestamp
 * \param   ts_p
 *          Pointer to timestamp to evaluate
 * \return  Delay from now to the timestamp in us, or 0 if timestamp
 *          is in the past already
 */
static uint32_t get_delay_from_now_us(timestamp_t * ts_p)
{
    if (ts_p->is_hp)
    {
        app_lib_time_timestamp_hp_t now_hp = lib_time->getTimestampHp();
        if (lib_time->isHpTimestampBefore(ts_p->hp, now_hp))
        {
            //Timestamp is already in the past, so 0 delay
            return 0;
        }

        return lib_time->getTimeDiffUs(now_hp, ts_p->hp);
    }
    else
    {
        app_lib_time_timestamp_coarse_t now_coarse = lib_time->getTimestampCoarse();
        if (Util_isLtUint32(ts_p->coarse, now_coarse))
        {
            // Coarse timestamp is already in the past, so 0 delay
            return 0;
        }

        // Check for overflow
        uint32_t delta_coarse = ts_p->coarse - now_coarse;

        // on 32 bits, max delay in us is 2^32 * 128 / 1000 / 1000 = 549755 coarse
        if (delta_coarse > 549755)
        {
            // No need to compute, it is far enough in future and cannot be represented
            // on a 32 bits counter in us without overflow.
            // Anyway the scheduler itself will be scheduled at least every
            // m_max_time_ms so we just need to Know that it is far in future
            return (uint32_t)(-1);
        }
        else
        {
            return (((ts_p->coarse - now_coarse) * 1000) / 128) * 1000;
        }
    }
}

/**
 * \brief   Check if a timestamp is before another one
 * \param   ts1_p
 *          Pointer to first timestamp
 * \param   ts2_p
 *          Pointer to second timestamp
 * \return  True if ts1 is before ts2
 */
static bool is_timestamp_before(timestamp_t * ts1_p, timestamp_t * ts2_p)
{
    if (ts1_p->is_hp && ts2_p->is_hp)
    {
        // Both timestamps are hp
        return lib_time->isHpTimestampBefore(ts1_p->hp, ts2_p->hp);
    }
    else if (!ts1_p->is_hp && !ts2_p->is_hp)
    {
        // Both timestamps are coarse
        return Util_isLtUint32(ts1_p->coarse, ts2_p->coarse);
    }
    else
    {
        // To compare a coarse and hp timestamp, we must compute the delay
        // relative to now for both and compare it
        return get_delay_from_now_us(ts1_p) < get_delay_from_now_us(ts2_p);
    }
}

/**
 * \brief   Execute the selected task if time to do it
 */
static void perform_query(task_t * task)
{
    bool status = false;
    uint32_t next = QUERY_SCHEDULER_STOP_TASK;

    if (task == NULL)
    {
        // There is an issue, no next task
        return;
    }
    if (task->modbus_query.queryId == 0xFF)
    {
        return;
    }
    // Execute the task selected
    task->modbus_query.au16reg = ModbusDataRegArray;

    if (task->modbus_query.writeOps) {
        memcpy(task->modbus_query.au16reg, &task->modbus_query.writeData, task->modbus_query.dataLength);
    }
    DEBUG_SEND(Is_debug(), "Query");
    DEBUG_SEND(Is_debug(), task->modbus_query);
    status = postModbusMasterQuery(&task->modbus_query);
    if (status) {
        RunModbusMasterTask();
    }

    if (!task->modbus_query.oneTime) {
        next = (uint32_t) task->modbus_query.interval * 1000;
    }

    // Update its next execution time under critical section
    // to avoid overriding new value set by IRQ
    Sys_enterCriticalSection();
    if (!task->updated && !task->removed)
    {
        // Task was not modified from IRQ or task itself during execution
        // so we can safely update task
        if (next == QUERY_SCHEDULER_STOP_TASK)
        {
            // Task doesn't have to be executed again
            // so safe to release it
            memset(&task->modbus_query, 0xFF, sizeof(MODBUS_MASTER_QUERY));
        }
        else
        {
            // Compute next execution time
            get_timestamp(&task->next_ts, next);
        }
    }
    Sys_exitCriticalSection();
}

/**
 * \brief   Get the next task for execution
 * \note    Must be called under critical section
 */
static task_t * get_next_task_locked()
{
    task_t * next = NULL;
    for (uint8_t i = 0; i < QUERY_SCHEDULER_MAX_TASKS; i++)
    {
        if (m_tasks[i].modbus_query.queryId != 0xFF)
        {
            if (m_tasks[i].removed)
            {
                // Time to clear the task
                memset(&m_tasks[i].modbus_query, 0xFF, sizeof(MODBUS_MASTER_QUERY));
                continue;
            }
            if (next == NULL || is_timestamp_before(&m_tasks[i].next_ts, &next->next_ts))
            {
                // First task found or task is before the selected one
                next = &m_tasks[i];
            }
        }
    }
    return next;
}

/**
 * \brief   Schedule the next selected task
 * \param   task
 *          Selected task
 * \note    Handle the case where task is too far in future
 */
static void schedule_task(task_t * task)
{
    timestamp_t next;
    uint32_t delay_ms;
    // Initialize next scheduling timestamp to max allowed value by
    // periodic work
    get_timestamp(&next, m_max_time_ms);

    if (is_timestamp_before(&task->next_ts, &next))
    {
        // Next task is in allowed range
        next = task->next_ts;
    }

    delay_ms = get_delay_from_now_us(&next);
    delay_ms = delay_ms / 1000;

    // Schedule the task for the query.
    App_Scheduler_addTask_execTime(periodic_work, delay_ms, EXEC_TIME);
}

/**
 * \brief   Periodic work called by the stack
 */
static uint32_t periodic_work(void)
{
    // If we enter here just to reschedule, let's do not execute task
    // even if ready

    if (!m_force_reschedule)
    {
        if (m_next_task_p != NULL
            && get_delay_from_now_us(&m_next_task_p->next_ts) == 0
            && !m_next_task_p->removed)
        {
            // The last selected task is ready
            perform_query(m_next_task_p);
        }
    }
    if(!m_force_reschedule) {
        nrf_delay_ms(timeoutDelayTlv.delay);
    }

    // Enter critical section to protect m_next_task_p
    Sys_enterCriticalSection();
    // Update next task and clean canceled task
    m_next_task_p = get_next_task_locked();
    if (m_next_task_p != NULL)
    {
        // Update periodic work according to next task

        schedule_task(m_next_task_p);
        // Reset updated state of task
        m_next_task_p->updated = false;
    }
    m_force_reschedule = false;

    Sys_exitCriticalSection();

    return APP_SCHEDULER_STOP_TASK;
}

/**
 * \brief   Add task to task table
 * \param   task_p
 *          task to add
 * \return  true if task was correctly added
 * \note    Must be called from critical section
 */
static bool add_task_to_table_locked(task_t * task_p)
{
    bool res = false;
    task_t * first_free = NULL;

    // Under critical section to avoid writing the same task
    Sys_enterCriticalSection();
    // Save modbus query to the memory.

    if (!task_p->modbus_query.oneTime)
    {
        modbus_query_data_t query = {
                .queryId = task_p->modbus_query.queryId,
                .slaveId = task_p->modbus_query.u8id,
                .deviceDetails = {task_p->modbus_query.deviceDetail.deviceId,task_p->modbus_query.deviceDetail.status,task_p->modbus_query.deviceDetail.attrId},
                .functionCode = task_p->modbus_query.u8fct,
                .startAddr = task_p->modbus_query.u16RegAdd,
                .length = task_p->modbus_query.u16CoilsNo,
                .interval = task_p->modbus_query.interval,
                .oneTime = task_p->modbus_query.oneTime,
                .isEnable = true,
                //.dataType = task_p->modbus_query.dataType,
                .writeOps = task_p->modbus_query.writeOps,
                .dataLength = task_p->modbus_query.dataLength,
        };
        memcpy(&query.writeData, &task_p->modbus_query.writeData, task_p->modbus_query.dataLength);
        Add_Modbus_query(query);
    }

    for (uint8_t i = 0; i < QUERY_SCHEDULER_MAX_TASKS; i++)
    {
        // First check if task already exist
        if (m_tasks[i].modbus_query.queryId == task_p->modbus_query.queryId)
        {
            // Task found, just update the next timestamp and exit
            m_tasks[i].modbus_query = task_p->modbus_query;
            m_tasks[i].next_ts = task_p->next_ts;
            m_tasks[i].updated = true;
            m_tasks[i].removed = false;
            res = true;
            break;
        }

        // Check for first free room in case task is not found
        if (m_tasks[i].modbus_query.queryId == 0xFF && first_free == NULL)
        {
            first_free = &m_tasks[i];
        }
    }

    if (!res && first_free != NULL)
    {
        memcpy(first_free, task_p, sizeof(task_t));
        res = true;
    }

    Sys_exitCriticalSection();
    return res;
}

/**
 * \brief   Remove task to task table
 * \param   cb
 *          cb associated to the task
 * \return  Pointer to the removed task
 * \note    Must be called from critical section
 */
static task_t * remove_task_from_table_locked(MODBUS_MASTER_QUERY query)
{
    task_t * removed_task = NULL;

    Sys_enterCriticalSection();
    // Remove modbus query from memory.
    Remove_Modbus_query(query.queryId);

    for (uint8_t i = 0; i < QUERY_SCHEDULER_MAX_TASKS; i++)
    {
        if (m_tasks[i].modbus_query.queryId == query.queryId)
        {
            // Mark the task as removed
            m_tasks[i].updated = true;
            m_tasks[i].removed = true;
            removed_task = &m_tasks[i];
            break;
        }
    }

    Sys_exitCriticalSection();

    return removed_task;
}

static void modbus_init() {
    gModbusInitHandler.uiModbusType = MODBUS_MASTER_RTU;
    gModbusInitHandler.u8id = 0;
    gModbusInitHandler.au16regs = ModbusDataRegArray;
    gModbusInitHandler.u16regsize = sizeof(ModbusDataRegArray) / sizeof(ModbusDataRegArray[0]);
    gModbusInitHandler.baudRate = 9600;
    modbusRtuInitialize(&gModbusInitHandler);
}

static void query_task_init()
{
    modbus_query_data_t* modbusQueryData = Get_Modbus_settings();
    for (uint8_t i = 0; i < QUERY_SCHEDULER_MAX_TASKS; i++)
    {
        if (modbusQueryData[i].queryId != 0xFF)
        {
            MODBUS_MASTER_QUERY masterQuery = {
                    .queryId = modbusQueryData[i].queryId,
                    .u8id = modbusQueryData[i].slaveId,
                    .deviceDetail = {modbusQueryData[i].deviceDetails.deviceId,modbusQueryData[i].deviceDetails.status,modbusQueryData[i].deviceDetails.attrId},
                    //.deviceDetail.deviceId = modbusQueryData[i].deviceDetails.deviceId,
                    //.deviceDetail.attrId = modbusQueryData[i].deviceDetails.attrId,
                    .u8fct = modbusQueryData[i].functionCode,
                    .u16RegAdd = modbusQueryData[i].startAddr,
                    .u16CoilsNo = modbusQueryData[i].length,
                    .interval = modbusQueryData[i].interval,
                    .oneTime = modbusQueryData[i].oneTime,
                    //.dataType = modbusQueryData[i].dataType,
                    .writeOps = modbusQueryData[i].writeOps,
                    .dataLength = modbusQueryData[i].dataLength,

            };

            memcpy(&masterQuery.writeData, &modbusQueryData[i].writeData, modbusQueryData[i].dataLength);
            Query_Scheduler_addTask(masterQuery);
        }
    }
}

//static void adjust_time_delay(task_t * task)
//{
//    for (uint8_t i = 0; i < QUERY_SCHEDULER_MAX_TASKS; i++)
//    {
//        if (m_tasks[i].modbus_query.queryId != 0xFF
//        && m_tasks[i].modbus_query.interval == task->modbus_query.interval)
//        {
//            task->modbus_query.interval = task->modbus_query.interval + ADD_ADDITIONAL_DELAY;
//        }
//    }
//}

void Query_Scheduler_init()
{
    m_max_time_ms = lib_time->getMaxHpDelay() / 1000;
    m_next_task_p = NULL;
    m_force_reschedule = false;
    modbus_init();
    for (uint8_t i = 0; i < QUERY_SCHEDULER_MAX_TASKS; i++)
    {
        memset(&m_tasks[i].modbus_query, 0xFF, sizeof(MODBUS_MASTER_QUERY));
    }

    m_initialized = true;
    query_task_init();
}

query_scheduler_res_e Query_Scheduler_addTask(MODBUS_MASTER_QUERY query)
{
    task_t new_task = {
            .modbus_query = query,
            .updated = false,
            .removed = false,
    };
    //adjust_time_delay(&new_task);

    query_scheduler_res_e res;
    get_timestamp(&new_task.next_ts, (uint32_t) (query.interval * 1000));

    if (!m_initialized)
    {
        return QUERY_SCHEDULER_RES_UNINITIALIZED;
    }
    Sys_enterCriticalSection();
    if (!add_task_to_table_locked(&new_task))
    {
        res = QUERY_SCHEDULER_RES_NO_MORE_TASK;
    }
    else
    {
        if (m_next_task_p == NULL
            || m_next_task_p->modbus_query.queryId == query.queryId
            || is_timestamp_before(&new_task.next_ts, &m_next_task_p->next_ts))
        {
            m_force_reschedule = true;
            App_Scheduler_addTask_execTime(periodic_work, 0, EXEC_TIME);
        }
        res = QUERY_SCHEDULER_RES_OK;
    }
    Sys_exitCriticalSection();
    return res;
}

query_scheduler_res_e Query_Scheduler_cancelTask(MODBUS_MASTER_QUERY query)
{
    query_scheduler_res_e res;

    if (!m_initialized)
    {
        return QUERY_SCHEDULER_RES_UNINITIALIZED;
    }
    Sys_enterCriticalSection();

    task_t * removed_task = remove_task_from_table_locked(query);
    if (removed_task != NULL)
    {
        // Force our task to be reschedule asap to do the cleanup of the task
        // and potentially change the next task to be schedule
        m_force_reschedule = true;
        App_Scheduler_addTask_execTime(periodic_work, 0, EXEC_TIME);

        res = QUERY_SCHEDULER_RES_OK;
    }
    else
    {
        res = QUERY_SCHEDULER_RES_UNKNOWN_TASK;
    }
    Sys_exitCriticalSection();
    return res;
}

///**
// * @brief
// * This method removes the running task based on it's query number.
// * @param  query number of the task.
// * @return settings .
// */
// status_res_e removeTask(uint8_t queryNum) {
//    uint8_t indx;
//    query_scheduler_res_e res = STATUS_RES_UNSUCCESSFUL;
//    status_res_e response;
//    for(indx = 0;indx<QUERY_SCHEDULER_MAX_TASKS;indx++) {
//        if(m_tasks[indx].modbus_query.queryId == queryNum) {
//            res = Query_Scheduler_cancelTask(m_tasks[indx].modbus_query);
//            break;
//        }
//    }
//    if(res == QUERY_SCHEDULER_RES_OK)
//        response = STATUS_RES_SUCCESS;
//    else
//        response = STATUS_RES_UNSUCCESSFUL;
//    return response;
//}