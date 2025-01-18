


#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <freertos/FreeRTOS.h>
#include <freertos/FreeRTOSConfig.h>
#include <freertos/portmacro.h>
#include <freertos/task.h>
#include <esp_heap_task_info.h>
#include <freertos/event_groups.h>

#include "app_main.h"
#include "app_utils.h"

// We'll do something with these, I am sure
TaskHandle_t xBTHandle      = NULL;
TaskHandle_t xCCTVHandle    = NULL;
TaskHandle_t xGpioHandle    = NULL;
TaskHandle_t xLightsHandle  = NULL;
TaskHandle_t xMQTTHandle    = NULL;
TaskHandle_t xSchedHandle   = NULL;
TaskHandle_t xSNTPHandle    = NULL;
TaskHandle_t xStatsHandle   = NULL;
TaskHandle_t xWebHandle     = NULL;
TaskHandle_t xWSSHandle     = NULL;

static TaskHandle_t *taskHandles[] = {&xBTHandle, &xCCTVHandle, &xGpioHandle, &xLightsHandle, &xMQTTHandle, &xSchedHandle, &xSNTPHandle, &xStatsHandle, &xWebHandle, &xWSSHandle};
#define MAX_TASKS ((sizeof(taskHandles) / sizeof(TaskHandle_t)) - 1)

#if (CONFIG_FREERTOS_VTASKLIST_INCLUDE_COREID && CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS)

#define MAX_TASK_NUM        25                  // Max number of per tasks info that it can store
#define MAX_BLOCK_NUM       25                  // Max number of per block info that it can store
#define HEAP_TRACE_RECORDS  100
#define JSON_BUFSIZE        4096                // JSON buffer size

uint16_t buflen             = 0;                // Length of JSON string in buffer
char *jsonTasksBuffer       = NULL;             // Buffer for storing JSON data to send to web client
bool statsViaSerial         = false;            // Show runtime info via the serial port

// ****************** BUG FIX *******************
// https://community.nxp.com/t5/MCUXpresso-IDE/FreeRTOS-maximum-used-priority-is-unreasonably-big-not/m-p/905591
void vPortStartFirstTask (void)
{
#if configUSE_TOP_USED_PRIORITY || configLTO_HELPER
  /* only needed for openOCD or Segger FreeRTOS thread awareness. It needs the symbol uxTopUsedPriority present after linking */
  {
    extern volatile const int uxTopUsedPriority;
    __attribute__ ((__unused__)) volatile uint8_t dummy_value_for_openocd;
    dummy_value_for_openocd = uxTopUsedPriority;
  }
#endif
}

// ******************************************************************
// ******************************************************************
#if defined (CONFIG_COMPILER_OPTIMIZATION_PERF)
IRAM_ATTR static int sTaskSortByPID (const void *a, const void *b)
#else
static int sTaskSortByPID (const void *a, const void *b)
#endif
{
  return (int)((const TaskStatus_t *)a)->xTaskNumber - (int)((const TaskStatus_t *)b)->xTaskNumber;

  const TaskStatus_t *pA = (const TaskStatus_t *)a;
  const TaskStatus_t *pB = (const TaskStatus_t *)b;
  if ( pA->xCoreID != pB->xCoreID )
  {
    return (int)pA->xCoreID - (int)pB->xCoreID;
  }
  else if ( pA->uxBasePriority != pB->uxBasePriority )
  {
    return (int)pB->uxBasePriority - (int)pA->uxBasePriority;
  }
  else
  {
    return (int)pA->xTaskNumber - (int)pB->xTaskNumber;
  }
}
// ******************************************************************
// ******************************************************************
#if defined (CONFIG_COMPILER_OPTIMIZATION_PERF)
IRAM_ATTR void execRuntimeStats (void)
#else
void execRuntimeStats (void)
#endif
{
  volatile UBaseType_t x;
  uint64_t ulTotalRunTime = 0, ulStatsAsPercentage = 0;

  // FixMe: Hate this, but it works for now.
  buflen = 0;

  /* Take a snapshot of the number of tasks in case it changes while this function is executing. */
  volatile UBaseType_t nTasks = uxTaskGetNumberOfTasks ();

  /* Allocate a TaskStatus_t structure for each task.  An array could be allocated statically at compile time. */
  TaskStatus_t *pTaskArray = (TaskStatus_t *)pvPortMalloc(nTasks * sizeof (TaskStatus_t));
  if ( pTaskArray == NULL )
  {
    F_LOGE(true, true, LC_YELLOW, "pvPortMalloc failed allocating %d bytes for 'config_list'", (nTasks * sizeof (TaskStatus_t)));
    return;
  }

  buflen += snprintf (jsonTasksBuffer, (JSON_BUFSIZE - buflen), "{\"proc\": [");

  if ( pTaskArray != NULL )
  {
    /* Generate raw status information about each task. */
    nTasks = uxTaskGetSystemState (pTaskArray, nTasks, (uint64_t *)&ulTotalRunTime);

    // sort by task ID
    //qsort (pTaskArray, nTasks, sizeof (TaskStatus_t), sTaskSortByPID);

    /* For percentage calculations. */
    ulTotalRunTime /= 100UL;

    // total runtime (tasks, OS, ISRs) since we checked last
    uint64_t totalRuntime = 0;
    static uint64_t sLastTotalRuntime;
    {
      const uint64_t runtime = totalRuntime;
      totalRuntime = totalRuntime - sLastTotalRuntime;
      sLastTotalRuntime = runtime;
    }

    /* Avoid divide by zero errors. */
    if ( ulTotalRunTime > 0 )
    {
#if defined (CONFIG_HEAP_TASK_TRACKING)
      // static storage is required for task memory info;
      static heap_task_totals_t         sTotals[MAX_TASK_NUM];
      static heap_task_block_t          sBlocks[MAX_BLOCK_NUM];
      static size_t numTotals = 0;
      heap_task_info_params_t heap_info = {};
      heap_info.caps[0]         = MALLOC_CAP_8BIT;        // Gets heap with CAP_8BIT capabilities
      heap_info.mask[0]         = MALLOC_CAP_8BIT;
      heap_info.caps[1]         = MALLOC_CAP_32BIT;       // Gets heap info with CAP_32BIT capabilities
      heap_info.mask[1]         = MALLOC_CAP_32BIT;
      heap_info.tasks           = NULL;                   // Passing NULL captures heap info for all tasks
      heap_info.num_tasks       = 0;
      heap_info.totals          = sTotals;                // Gets task wise allocation details
      heap_info.num_totals      = &numTotals;
      heap_info.max_totals      = MAX_TASK_NUM;           // Maximum length of "sTotals"
      heap_info.blocks          = sBlocks;                // Gets block wise allocation details. For each block, gets owner task, address and size
      heap_info.max_blocks      = MAX_BLOCK_NUM;          // Maximum length of "sBlocks"
      heap_caps_get_per_task_info (&heap_info);
#endif // CONFIG_HEAP_TASK_TRACKING
      if ( statsViaSerial == true )
      {
        F_LOGI(true, true, LC_WHITE, "%4s %4s %4s %17s %10s %10s %10s %14s %7s %6s %6s", "PID", "BP", "CP", "Task Name", "HWM", "8 Bit", "32 Bit", "Run Time", "State", "Core", "CPU");
        F_LOGI(true, true, LC_WHITE, "--------------------------------------------------------------------------------------------------------");
      }

      /* For each populated position in the pTaskArray array,
       * format the raw data as human readable ASCII data. */
      for ( x = 0; x < nTasks; x++ )
      {
        const TaskStatus_t *pTask = &pTaskArray[x];
        char state = '?';

        uint64_t cap8 = 0;
        uint64_t cap32 = 0;
#if defined (CONFIG_HEAP_TASK_TRACKING)
        for ( int i = 0; i < *heap_info.num_totals; i++ )
        {
          if ( pTask->xHandle == heap_info.totals[i].task )
          {
            cap8  = heap_info.totals[i].size[0];      // Heap size with CAP_8BIT capabilities
            cap32 = heap_info.totals[i].size[1];      // Heap size with CAP32_BIT capabilities
            break;
          }
        }
#endif // CONFIG_HEAP_TASK_TRACKING
        /* What percentage of the total run time has the task used?
         * This will always be rounded down to the nearest integer.
         * ulTotalRunTimeDiv100 has already been divided by 100. */
        ulStatsAsPercentage = pTask->ulRunTimeCounter / ulTotalRunTime;
        if (ulStatsAsPercentage > 100)
        {
          F_LOGE (true, true, LC_YELLOW, "pid: %d, ulStatsAsPercentage = %d, ulTotalRunTime = %d, pTask->ulRunTimeCounter = %d", pTask->xTaskNumber, ulStatsAsPercentage, ulTotalRunTime, pTask->ulRunTimeCounter)
        }

        switch ( pTask->eCurrentState )
        {
          case eRunning:   state = 'X'; break;
          case eReady:     state = 'R'; break;
          case eBlocked:   state = 'B'; break;
          case eSuspended: state = 'S'; break;
          case eDeleted:   state = 'D'; break;
          case eInvalid:   state = 'I'; break;
        }

        const char core = pTask->xCoreID == tskNO_AFFINITY?'*':('0' + pTask->xCoreID);

        buflen += snprintf (&jsonTasksBuffer[buflen], (JSON_BUFSIZE - buflen), "{\"pid\": %u,\"bp\": %d,\"cp\": %d,\"tn\": \"%s\",\"hwm\": %lu,\"c8\": %llu,\"c32\": %llu,\"rt\": %llu,\"st\": \"%c\",\"core\": \"%c\",\"use\": %lld},",
          pTask->xTaskNumber, pTask->uxBasePriority, pTask->uxCurrentPriority, pTask->pcTaskName, pTask->usStackHighWaterMark, cap8, cap32, pTask->ulRunTimeCounter, state, core, ulStatsAsPercentage);
        if ( ulStatsAsPercentage > 0UL )
        {
          if ( statsViaSerial == true )
          {
            F_LOGI(true, true, LC_WHITE, "%4d %4d %4d %17s %10d %10u %10u %14u %7c %6c %6ld %%", pTask->xTaskNumber, pTask->uxBasePriority, pTask->uxCurrentPriority, pTask->pcTaskName, pTask->usStackHighWaterMark, cap8, cap32, pTask->ulRunTimeCounter, state, core, ulStatsAsPercentage);
          }
        }
        else
        {
          if ( statsViaSerial == true )
          {
            F_LOGI(true, true, LC_WHITE, "%4d %4d %4d %17s %10d %10u %10u %14u %7c %6c     <1 %%", pTask->xTaskNumber, pTask->uxBasePriority, pTask->uxCurrentPriority, pTask->pcTaskName, pTask->usStackHighWaterMark, cap8, cap32, pTask->ulRunTimeCounter, state, core);
          }
        }
      }
    }
  }
    // Remove trailing ','
  buflen--;
  // Close the JSON data
  buflen += snprintf (&jsonTasksBuffer[buflen], (JSON_BUFSIZE - buflen), "]}");
  // Terminate the string
  jsonTasksBuffer[buflen] = 0x0;

  vPortFree (pTaskArray);
}

IRAM_ATTR int getSystemTasksJsonString (char **buf)
{
  *(buf) = jsonTasksBuffer;
  return buflen;
}

/* ************************************************************************************* */
/* * CPU Usage/runtime stats (console and web display)                                   */
/* ************************************************************************************* */
void stats_task (void *pvParameters)
{
  jsonTasksBuffer = (char *)pvPortMalloc(JSON_BUFSIZE);
  if ( jsonTasksBuffer == NULL )
  {
    F_LOGE(true, true, LC_YELLOW, "pvPortMalloc failed allocating %d bytes for 'jsonTasksBuffer'", JSON_BUFSIZE);
  }
  else
  {
    for ( ;; )
    {
      delay_ms (5000);
      execRuntimeStats();
    }
  }
}
#endif // (CONFIG_FREERTOS_VTASKLIST_INCLUDE_COREID && CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS)
