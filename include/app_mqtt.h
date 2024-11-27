#ifndef __APP_MQTT_H__
#define __APP_MQTT_H__

#include "colours.h"

#include "app_mqtt_config.h"

//Behavior event type
typedef enum _VCA_EVENT_TYPE_
{
    VCA_TRAVERSE_PLANE                  = 0x1,        //Traverse plane
    VCA_ENTER_AREA                      = 0x2,        //Enter area (region rule)
    VCA_EXIT_AREA                       = 0x4,        //Leave area (region rule)
    VCA_INTRUSION                       = 0x8,        //Intrusion (region rule)
    VCA_LOITER                          = 0x10,       //Loitering (region rule)
    VCA_LEFT_TAKE                       = 0x20,       //Object left or take (region rule)
    VCA_PARKING                         = 0x40,       //Illegal parking (region rule)
    VCA_RUN                             = 0x80,       //Running (region rule)
    VCA_HIGH_DENSITY                    = 0x100,      //People density (region rule)
    VCA_VIOLENT_MOTION                  = 0x200,      //Violent motion
    VCA_REACH_HIGHT                     = 0x400,      //Reach high
    VCA_GET_UP                          = 0x800,      //Get up
    VCA_LEFT                            = 0x1000,     //Item left
    VCA_TAKE                            = 0x2000,     //Item take
    VCA_LEAVE_POSITION                  = 0x4000,     //Leave position
    VCA_TRAIL                           = 0x8000,     //Trail
    VCA_KEY_PERSON_GET_UP               = 0x10000,    //Key person get up
    VCA_STANDUP                         = 0x20000,    //Stand Up
    VCA_FALL_DOWN                       = 0x80000,    //Fall down
    VCA_AUDIO_ABNORMAL                  = 0x100000,   //Audio abnormal
    VCA_ADV_REACH_HEIGHT                = 0x200000,   //Advance reach height
    VCA_TOILET_TARRY                    = 0x400000,   //Toilet tarry
    VCA_YARD_TARRY                      = 0x800000,   //Yard tarry
    VCA_ADV_TRAVERSE_PLANE              = 0x1000000,  //Advance traverse plane
    VCA_LECTURE                         = 0x2000000,  //Lecture
    VCA_ANSWER                          = 0x4000000,  //Answer
    VCA_HUMAN_ENTER                     = 0x10000000, //Human enter ATM, supported only in ATM_PANEL mode
    VCA_OVER_TIME                       = 0x20000000, //Operation overtime, supported only in ATM_PANEL mode
    VCA_STICK_UP                        = 0x40000000, //ATM stick up (region rule)
    VCA_INSTALL_SCANNER                 = 0x80000000  //Install scanner on ATM (region rule)
}VCA_EVENT_TYPE;

//Behavior event type(extended)
typedef enum _VCA_RULE_EVENT_TYPE_EX_
{
    ENUM_VCA_EVENT_TRAVERSE_PLANE       = 1,   //Traverse plane
    ENUM_VCA_EVENT_ENTER_AREA           = 2,   //Enter area
    ENUM_VCA_EVENT_EXIT_AREA            = 3,   //Leave area
    ENUM_VCA_EVENT_INTRUSION            = 4,   //Intrusion
    ENUM_VCA_EVENT_LOITER               = 5,   //Loitering
    ENUM_VCA_EVENT_LEFT_TAKE            = 6,   //Object left or take
    ENUM_VCA_EVENT_PARKING              = 7,   //Illegal parking
    ENUM_VCA_EVENT_RUN                  = 8,   //Running
    ENUM_VCA_EVENT_HIGH_DENSITY         = 9,   //People density
    ENUM_VCA_EVENT_VIOLENT_MOTION       = 10,  //Violent motion
    ENUM_VCA_EVENT_REACH_HIGHT          = 11,  //Reach high
    ENUM_VCA_EVENT_GET_UP               = 12,  //Get up
    ENUM_VCA_EVENT_LEFT                 = 13,  //Item left
    ENUM_VCA_EVENT_TAKE                 = 14,  //Item take
    ENUM_VCA_EVENT_LEAVE_POSITION       = 15,  //Leave position
    ENUM_VCA_EVENT_TRAIL                = 16,  //Trail
    ENUM_VCA_EVENT_KEY_PERSON_GET_UP    = 17,  //Key person get up
    ENUM_VCA_EVENT_STANDUP              = 18,  //Stand Up
    ENUM_VCA_EVENT_FALL_DOWN            = 20,  //Fall down
    ENUM_VCA_EVENT_AUDIO_ABNORMAL       = 21,  //Audio abnormal
    ENUM_VCA_EVENT_ADV_REACH_HEIGHT     = 22,  //Advance reach height
    ENUM_VCA_EVENT_TOILET_TARRY         = 23,  //Toilet tarry
    ENUM_VCA_EVENT_YARD_TARRY           = 24,  //Yard tarry
    ENUM_VCA_EVENT_ADV_TRAVERSE_PLANE   = 25,  //Advance traverse plane
    ENUM_VCA_EVENT_LECTURE              = 26,  //Lecture
    ENUM_VCA_EVENT_ANSWER               = 27,  //Answer
    ENUM_VCA_EVENT_HUMAN_ENTER          = 29,  //Human enter ATM, supported only in ATM_PANEL mode--
    ENUM_VCA_EVENT_OVER_TIME            = 30,  //Operation overtime, supported only in ATM_PANEL mode
    ENUM_VCA_EVENT_STICK_UP             = 31,  //ATM stick up (region rule)
    ENUM_VCA_EVENT_INSTALL_SCANNER      = 32,  //Install scanner on ATM (region rule)
    ENUM_VCA_EVENT_PEOPLENUM_CHANGE     = 35,  //People Num Change
    ENUM_VCA_EVENT_SPACING_CHANGE       = 36,  //Spacing Change
    ENUM_VCA_EVENT_COMBINED_RULE        = 37,  //Combination Events
    ENUM_VCA_EVENT_SIT_QUIETLY          = 38,   //Sit Quietly----
    ENUM_VCA_EVENT_HIGH_DENSITY_STATUS  = 39,   //People density status
    ENUM_VCA_EVENT_RUNNING              = 40, //Run detection
    ENUM_VCA_EVENT_RETENTION            = 41, //Detention detection
    ENUM_VCA_EVENT_BLACKBOARD_WRITE     = 42,   //Black Board Writing
    ENUM_VCA_EVENT_SITUATION_ANALYSIS   = 43,   //Situational analysis
    ENUM_VCA_EVENT_PLAY_CELLPHONE       = 44,    //play cellphone
    ENUM_VCA_EVENT_DURATION             = 45     //duration
} VCA_RULE_EVENT_TYPE_EX;

int       getEngDataJsonString(char **buf);
void      init_mqtt_client(void);
void      mqtt_send_task(void *arg);
bool      mqtt_check_connected(void);
void      mqtt_restart(void);
char      *get_lwt_topic(void);
int       mqttSubscribe(enum mqtt_devices device);
void      mqttPublish(enum mqtt_devices device, const char *format, ...);
void      mqttControl(enum mqtt_devices device, const char *format, ...);


#endif
