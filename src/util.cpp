/******************************************************************
*********                       Includes                  *********
******************************************************************/
#include <Arduino.h>
#include <RTCZero.h>
#include <NMEAGPS.h>

#include "..\Board.h"
#include "..\Defines.h"

#include "util.h"
#include "sensor.h"
/******************************************************************
*********                  Local Defines                  *********
******************************************************************/

/******************************************************************
*********                Local Enumerations               *********
******************************************************************/

/******************************************************************
*********                  Local Objects                  *********
******************************************************************/
RTCZero rtc;// Create an rtc object
extern gps_fix fix;
/******************************************************************
*********                  Local Variables                *********
******************************************************************/
volatile bool logSensorDataNow = false;             //a variable to keep check of the state.
/******************************************************************
*********          Local Function Definitions             *********
******************************************************************/

/******************************************************************
*********              Application Firmware               *********
******************************************************************/
bool init_leds(void)
{
    Serial.println("Init Led's");
    pinMode(GPIO_OUTPUT_STATUS_LED_0, OUTPUT);
    pinMode(GPIO_OUTPUT_STATUS_LED_1, OUTPUT);
    pinMode(GPIO_OUTPUT_STATUS_LED_2, OUTPUT);
    return true;
}

void blinky(int repeats, int time)
{
    for (int i = 0; i < repeats; i++)
    {
        digitalWrite(GPIO_OUTPUT_STATUS_LED_0, HIGH);
        delay(time);
        digitalWrite(GPIO_OUTPUT_STATUS_LED_0, LOW);
        delay(time);
    }
}

bool init_rtc(void)
{
    Serial.println("Init RTC");
    rtc.begin();                           // Start the RTC now that BEACON_INTERVAL has been updated
    rtc.setAlarmSeconds(rtc.getSeconds()); // Initialise RTC Alarm Seconds

    alarmMatch();                      // Set next alarm time using updated BEACON_INTERVAL
    rtc.enableAlarm(rtc.MATCH_HHMMSS); // Alarm Match on hours, minutes and seconds
    rtc.attachInterrupt(alarmMatch);   // Attach alarm interrupt

    return true;
}

void set_rtc()
{
    Serial.println("set rtc time, from gps time (UTC)...");
    rtc.setTime(fix.dateTime.hours, fix.dateTime.minutes, fix.dateTime.seconds);
    rtc.setDate(fix.dateTime.year, fix.dateTime.month, fix.dateTime.date);
    Serial.println("RTC ALARM");
    Serial.print("current minutes: ");
    Serial.println(rtc.getMinutes());
    Serial.print("alarm will activate in ");
    Serial.println(rtc.getMinutes() + APP_DEVICE_NO_MOTION_INTERVAL_S/60UL);
    Serial.print(rtc.getYear());
    Serial.print(rtc.getMonth());
    Serial.println(rtc.getDay());
}

void set_alarm()
{
    Serial.println("set rtc alarm");
    delay(250);
    rtc.begin();                           // Start the RTC now that BEACON_INTERVAL has been updated
    rtc.setAlarmSeconds(rtc.getSeconds()); // Initialise RTC Alarm Seconds
    alarmMatch();                          // Set next alarm time using updated BEACON_INTERVAL
    Serial.print("iterationCounter");
    // Serial.println(get_iteration_counter());
    int rtc_mins = rtc.getMinutes(); // Read the RTC minutes
    Serial.print("rtc_mins: ");
    Serial.println(rtc_mins);
    int rtc_hours = rtc.getHours(); // Read the RTC hours
    Serial.print("rtc_hours: ");
    Serial.println(rtc_hours);
    rtc_mins = rtc_mins + APP_DEVICE_NO_MOTION_INTERVAL_S/60UL; // Add the BEACON_INTERVAL to the RTC minutes
    while (rtc_mins >= 60)
    {                              // If there has been an hour roll over
        rtc_mins = rtc_mins - 60;  // Subtract 60 minutes
        rtc_hours = rtc_hours + 1; // Add an hour
    }
    rtc_hours = rtc_hours % 24;        // Check for a day roll over
    rtc.setAlarmMinutes(rtc_mins);     // Set next alarm time (minutes)
    rtc.setAlarmHours(rtc_hours);      // Set next alarm time (hours)
    rtc.enableAlarm(rtc.MATCH_HHMMSS); // Alarm Match on hours, minutes and seconds
    rtc.attachInterrupt(alarmMatch);   // Attach alarm interrupt
}

void rtc_sleep()
{
    blinky(5, 250);

    // Get ready for sleep
    gps_stop();
    delay(1000); // Wait for serial ports to clear

    // Turn LED off
    blinky(10, BLINK_TIME_INTERVAL);

    // Turn off LORA
    // try keeping the LORA on... digitalWrite(GPIO_OUTPUT_GPS_PWR_EN, LOW);

    if (check_movement_timeout())
    {
        Serial.println("BEACON_INTERVAL too short, not economic to switch off GPS");
    }
    else
    {
        gps_turn_pwr(LOW); // Disable the GPS
    }

    // Close and detach the serial console (as per CaveMoa's SimpleSleepUSB)
    Serial.println("Going to sleep until next alarm time...");
    delay(1000);        // Wait for serial port to clear
    Serial.end();       // Close the serial console
    USBDevice.detach(); // Safely detach the USB prior to sleeping
    delay(250);
    // Sleep until next alarm match
    rtc.standbyMode();
}

/************************** RTC alarm interrupt*****************************************/
void alarmMatch()
{
    uint8_t rtc_mins = rtc.getMinutes(); // Read the RTC minutes
    uint8_t rtc_hours = rtc.getHours();  // Read the RTC hours

    if (check_movement_timeout())
    {
        Serial.println("While in motion ALARM occured...");
        rtc_mins = rtc_mins + APP_DEVICE_IN_MOTION_INTERVAL_S/60UL; // Add the BEACON_INTERVAL to the RTC minutes}
    }
    else
    {
        Serial.println("no motion ALARM occured...");
        rtc_mins = rtc_mins + APP_DEVICE_NO_MOTION_INTERVAL_S/60UL; // Add the MOVEMENT_INTERVAL to the RTC minutes}
    }

    while (rtc_mins >= 60)
    {                    // If there has been an hour roll over
        rtc_mins -= 60;  // Subtract 60 minutes
        rtc_hours += 1; // Add an hour
    }
    rtc_hours = rtc_hours % 24;    // Check for a day roll over
    rtc.setAlarmMinutes(rtc_mins); // Set next alarm time (minutes)
    rtc.setAlarmHours(rtc_hours);  // Set next alarm time (hours)

    logSensorDataNow = true;
} // end alarmMatch

void wake_device()
{
    digitalWrite(GPIO_OUTPUT_STATUS_LED_0, LOW);
    //digitalWrite(11, LOW); // HIGH IS ON

    USBDevice.attach(); // Re-attach the USB, audible sound on windows machine
    delay(250);         // Delay added to make serial more reliable
    Serial.begin(115200);
    delay(3000);
    Serial.println("device awake");
    
    init_gps();
}

bool get_status_log_sensor_data(void)
{
    return logSensorDataNow;
}

void get_epoch_time_bytes(uint8_t *dataBuf)
{
    uint32_t epochTimeNow = rtc.getEpoch();
    *dataBuf++ = (uint8_t)(epochTimeNow & 0xff);
    *dataBuf++ = (uint8_t)((epochTimeNow >> 8) & 0xff);
    *dataBuf++ = (uint8_t)((epochTimeNow >> 16) & 0xff);
    *dataBuf++ = (uint8_t)((epochTimeNow >> 24) & 0xff);
}
/******************************************************************
*********                       EOF                       *********
*******************************************************************
********* Dev. by Sanchitha Dias (sanchithadias@gmail.com)*********
******************************************************************/
