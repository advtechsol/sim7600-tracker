// - - - - - - - - - - - - - - - - - - - - - - -
// Commands to compile/upload and monitor output
// arduino-cli compile --fqbn adafruit:samd:adafruit_feather_m0 sim7600_tracker
// arduino-cli upload --port /dev/ttyACM0 --fqbn adafruit:samd:adafruit_feather_m0 sim7600_tracker
// picocom /dev/ttyACM0
// - - - - - - - - - - - - - - - - - - - - - - -
/******************************************************************
*********                       Includes                  *********
******************************************************************/
#include <FreeRTOS_SAMD21.h>
#include <avr/dtostrf.h>

#include "Board.h"
#include "Defines.h"

#include "src/logger.h"
#include "src/modem.h"
#include "src/sensor.h"
#include "src/util.h"
/******************************************************************
*********                  Local Defines                  *********
******************************************************************/

/******************************************************************
*********                   Type Defines                  *********
******************************************************************/

/******************************************************************
*********                Local Enumerations               *********
******************************************************************/
typedef enum
{
    state_init = 1,
    state_check_sms,
    state_read_sensors,
    state_log_data,
    state_data_offload,
    state_send_sms,
    state_sleep,
    state_wake
} DeviceState_t;

DeviceState_t deviceState = state_init;
/******************************************************************
*********                  Local Objects                  *********
******************************************************************/

/******************************************************************
*********                  Local Variables                *********
******************************************************************/
extern volatile bool flagSendSMS;
extern volatile bool flagUpload;

extern Config settings;
/******************************************************************
*********          Local Function Definitions             *********
******************************************************************/

/******************************************************************
*********              Application Firmware               *********
******************************************************************/
void setup()
{
    init_debug_serial();

    check_config();

    os_delay_Ms(1000); // keep this to avoid USB crash
    init_leds();

    init_modem();    

    init_logger();

    init_rtc();

    init_tilt_sensor();

    debug_println("... start!");
}

void loop()
{
    digitalWrite(GPIO_OUTPUT_STATUS_LED_2, HIGH);

    if (get_batt_mvolts() < BATTERY_LOW_VOLTAGE_THRESHOLD_MV)
    {
        digitalWrite(GPIO_OUTPUT_STATUS_LED_2, LOW);
        debug_println("Battery Voltage is below the Threshold.");
        deviceState = state_sleep;
    }

    switch (deviceState)
    {
    case state_init:
        debug_println("State: state_init");
        if(sms_config())
        {
            while(state_modem_ok != send_command("GSN", 3));
            send_command("CMEE=2", 10);
            deviceState = state_check_sms;
        }
        else
        {            
            send_command("CRESET", 15);
            os_delay_S(15);
            send_command("?", 10);
        }
        break;
    case state_check_sms:
        debug_println("State: state_check_sms");
        send_command("CMGRD=0", 8);
        deviceState = state_read_sensors;
        break;
    case state_read_sensors:
        debug_println("State: state_read_sensors");
        get_location();
        deviceState = state_log_data;
        break;
    case state_log_data:
        debug_println("State: state_log_data");
        deviceState = state_data_offload;
        break;
    case state_data_offload:
        debug_println("State: state_data_offload");
        upload_location();
        if(flagSendSMS)
        {
            deviceState = state_send_sms;
        }
        else
        {
            deviceState = state_sleep;
        }
        break;
    case state_send_sms:
        debug_println("State: state_send_sms");
        send_sms();
        flagSendSMS = false;
        deviceState = state_sleep;
        break;
    case state_sleep:
        debug_println("State: state_sleep");
        alarmMatch();

        if(check_movement_timeout() || get_speed() >= 1)
        {
            deviceState = state_check_sms;
            break;
        }
        /* Send device to sleep */
        digitalWrite(GPIO_OUTPUT_STATUS_LED_2, LOW);
        rtc_sleep();
        deviceState = state_wake;
        break;
    case state_wake:
        debug_println("State: state_wake");
        /* Wake up the device */
        wake_device();
        // deviceState = state_check_sms;
        deviceState = state_init;
        break;
    default:
        debug_println("State: default");
        deviceState = state_init;
        break;
    }
    digitalWrite(GPIO_OUTPUT_STATUS_LED_2, LOW);
    os_delay_Ms(10);
}
/******************************************************************
*********                       EOF                       *********
******************************************************************/