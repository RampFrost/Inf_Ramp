//
// Created by Maverick on 14/12/21.
//

#ifndef QUERY_SCHEDULER_H
#define QUERY_SCHEDULER_H

#include <stdint.h>
#include <stdbool.h>
#include "api.h"
#include "../driver/modbus_lib.h"
#include "../../iws_libraries/utils/iws.h"

/**
 * \brief   Value to return from task to remove it
 */
#define QUERY_SCHEDULER_STOP_TASK     ((uint32_t)(-1))

/**
 * \brief   Value to return from task or as initial time to be executed ASAP
 */
#define APP_SCHEDULER_SCHEDULE_ASAP (0)

#define QUERY_SCHEDULER_MAX_TASKS (60)
#define MODBUS_MAX_REGISTER_SIZE (56)

/**
 * \brief   List of return code
 */
typedef enum
{
    /** Operation is successful */
    QUERY_SCHEDULER_RES_OK = 0,
    /** No more tasks available */
    QUERY_SCHEDULER_RES_NO_MORE_TASK = 1,
    /** Trying to cancel a task that doesn't exist */
    QUERY_SCHEDULER_RES_UNKNOWN_TASK = 2,
    /** Using the library without previous initialization */
    QUERY_SCHEDULER_RES_UNINITIALIZED = 3
} query_scheduler_res_e;

/**
 * \brief   Initialize scheduler
 *
 * Example on use:
 * @code
 * void App_init(const app_global_functions_t * functions)
 * {
 *     App_Scheduler_init();
 *     ...
 *     // Start the stack
 *     lib_state->startStack();
 * }
 * @endcode
 *
 * \note    If App scheduler is used in application, the periodicWork offered
 *          by system library MUST NOT be used outside of this module
 */
void Query_Scheduler_init(void);

/**
 * \brief   Add a task
 *
 * Example on use:
 *
 * @code
 *
 * static uint32_t periodic_task_50ms()
 * {
 *     ...
 *     return 50;
 * }
 *
 * static uint32_t periodic_task_500ms()
 * {
 *     ...
 *     return 500;
 * }
 *
 * void App_init(const app_global_functions_t * functions)
 * {
 *     App_Scheduler_init();
 *     // Launch two periodic task with different period
 *     App_Scheduler_addTask(periodic_task_50ms, APP_SCHEDULER_SCHEDULE_ASAP);
 *     App_Scheduler_addTask(periodic_task_500ms, APP_SCHEDULER_SCHEDULE_ASAP);
 *     ...
 *     // Start the stack
 *     lib_state->startStack();
 * }
 * @endcode
 *
 *
 * \param   cb
 *          Callback to be called from main periodic task.
 *          Same cb can only be added once. Calling this function with an already
 *          registered cb will update the next scheduled time.
 * \param   delay_ms
 *          delay in ms to be scheduled (0 to be scheduled asap)
 * \param   exec_time_us
 *          Maximum execution time required for the task to be executed
 * \return  True if able to add, false otherwise
 */
query_scheduler_res_e Query_Scheduler_addTask(MODBUS_MASTER_QUERY query);

/**
 * \brief   Cancel a task
 * \param   cb
 *          Callback already registered from App_Scheduler_addTask.
 * \return  True if able to cancel, false otherwise (not existing)
 */
query_scheduler_res_e Query_Scheduler_cancelTask(MODBUS_MASTER_QUERY query);
// /**
//  * Function to remove a query from m_tasks[] array.
//  */
// status_res_e removeTask(uint8_t queryNum);

#endif //QUERY_SCHEDULER_H