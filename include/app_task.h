
#ifndef   __TASK_H__
#define   __TASK_H__

// Stack size
// ----------------------------------------
#define     STACKSIZE_DEFAULT         2048

#define     STACKSIZE_BT              4096
#define     STACKSIZE_CCTV            4096
#define     STACKSIZE_GPIO            4096
#define     STACKSIZE_HTTPD           8192
#define     STACKSIZE_LIGHTS          3072
#define     STACKSIZE_MQTT_BROKER     3072
#define     STACKSIZE_SCHEDULER       4096
#define     STACKSIZE_SNTP            4096
#define     STACKSIZE_STATS           3072
#define     STACKSIZE_WEB             4096
#define     STACKSIZE_WSS             4096

// Task names
// ----------------------------------------
#define     TASK_NAME_BT              "app_bluetooth"
#define     TASK_NAME_CCTV            "cctv"
#define     TASK_NAME_GPIO            "gpio_handler"
#define     TASK_NAME_LIGHTS          "led_display"
#define     TASK_NAME_MQTT_BROKER     "mqtt_broker"
#define     TASK_NAME_SCHEDULER       "scheduler"
#define     TASK_NAME_SNTP            "app_sntp"
#define     TASK_NAME_STATS           "task_list"
#define     TASK_NAME_WEB             "web_server"
#define     TASK_NAME_WSS             "ws_transport"

// Task priorities
// ----------------------------------------
#define     TASK_PRIORITY_BT          (tskIDLE_PRIORITY + 1)
#define     TASK_PRIORITY_CCTV        (tskIDLE_PRIORITY + 1)
#define     TASK_PRIORITY_GPIO        (tskIDLE_PRIORITY + 1)
#define     TASK_PRIORITY_LIGHTS      (configMAX_PRIORITIES - 2)
#define     TASK_PRIORITY_MQTT        (tskIDLE_PRIORITY + 1)
#define     TASK_PRIORITY_SCHEDULER   (tskIDLE_PRIORITY + 1)
#define     TASK_PRIORITY_SNTP        (tskIDLE_PRIORITY + 1)
#define     TASK_PRIORITY_STATS       (tskIDLE_PRIORITY + 1)
#define     TASK_PRIORITY_WEB         (configMAX_PRIORITIES - 4)
#define     TASK_PRIORITY_WSS         (tskIDLE_PRIORITY + 1)

extern TaskHandle_t xBTHandle;
extern TaskHandle_t xCCTVHandle;
extern TaskHandle_t xGpioHandle;
extern TaskHandle_t xLightsHandle;
extern TaskHandle_t xMQTTHandle;
extern TaskHandle_t xSchedHandle;
extern TaskHandle_t xSNTPHandle;
extern TaskHandle_t xStatsHandle;
extern TaskHandle_t xWebHandle;
extern TaskHandle_t xWSSHandle;

void   stats_task (void *pvParameters);
int    getSystemTasksJsonString (char **buf);

#endif //  __TASK_H__
