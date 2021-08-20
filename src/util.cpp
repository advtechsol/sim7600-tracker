/******************************************************************
*********                       Includes                  *********
******************************************************************/
#include <Arduino.h>
#include <RTCZero.h>
#include <NMEAGPS.h>
#include <FreeRTOS_SAMD21.h>

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
RTCZero rtc; // Create an rtc object
extern gps_fix fix;
/******************************************************************
*********                  Local Variables                *********
******************************************************************/
volatile bool logSensorDataNow = false; //a variable to keep check of the state.
volatile int edges[2];
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
    Serial.println(rtc.getMinutes() + APP_DEVICE_NO_MOTION_INTERVAL_S / 60UL);
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
    rtc_mins = rtc_mins + APP_DEVICE_NO_MOTION_INTERVAL_S / 60UL; // Add the BEACON_INTERVAL to the RTC minutes
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
        rtc_mins = rtc_mins + APP_DEVICE_IN_MOTION_INTERVAL_S / 60UL; // Add the BEACON_INTERVAL to the RTC minutes}
    }
    else
    {
        Serial.println("no motion ALARM occured...");
        rtc_mins = rtc_mins + APP_DEVICE_NO_MOTION_INTERVAL_S / 60UL; // Add the MOVEMENT_INTERVAL to the RTC minutes}
    }

    while (rtc_mins >= 60)
    {                   // If there has been an hour roll over
        rtc_mins -= 60; // Subtract 60 minutes
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

// - - - - - - - - Delay Helpers - - - - - - - -
void os_delay_Us(int us)
{
    vTaskDelay(us / portTICK_PERIOD_US);
}

void os_delay_Ms(int ms)
{
    vTaskDelay((ms * 1000) / portTICK_PERIOD_US);
}

void os_delay_S(int s)
{
    while (s > 0)
    {
        os_delay_Ms(1000);
        s--;
    }
}

// - - - - - - - - - - - - - - - - - - - - - - -
int find_chr(const char *text, const int start, const char chr)
{
    char *pch;
    pch = (char *)memchr(&text[start], chr, strlen(text));
    if (pch != NULL)
    {
        return min(pch - text, strlen(text) - 1);
    }
}

void find_edges(const char *text, int order, const char chr)
{
    int start = 0;
    int end = 0;
    int i = 0;
    while ((find_chr(text, start, chr) != -1) && (i <= order))
    {
        start = end;
        end = find_chr(text, start + 1, chr);
        i++;
    }
    edges[0] = start;
    edges[1] = end;
}

void split_chr(char *destination, const char *source, const char chr, const int part)
{
    find_edges(source, part, chr);
    int len = edges[1] - edges[0] - 1;
    int i = 0;
    while (i < len)
    {
        destination[i] = source[i + edges[0] + 1];
        i++;
    }
    destination[i] = 0;
}

float to_geo(char *coordinate, char *indicator)
{
    char d_str[5];
    char m_str[12];
    int len = strlen(coordinate);
    if (len == 11)
    {
        memcpy(d_str, coordinate, 2);
        memmove(coordinate, coordinate + 2, 9);
    }
    if (len == 12)
    {
        memcpy(d_str, coordinate, 3);
        memmove(coordinate, coordinate + 3, 9);
    }
    memcpy(m_str, coordinate, 9);
    float deg = atof(d_str);
    float min = atof(m_str);
    deg = deg + min / 60.0;

    if (indicator[0] == 'S' || indicator[0] == 'W')
        deg = 0 - deg;
    return deg;
}

void to_date(char *date)
{
    // input: 190621
    // output: 2021-06-21
    char temp[12];
    temp[0] = '2';
    temp[1] = '0';
    temp[2] = date[4];
    temp[3] = date[5];
    temp[4] = '-';
    temp[5] = date[2];
    temp[6] = date[3];
    temp[7] = '-';
    temp[8] = date[0];
    temp[9] = date[1];
    temp[10] = 0;
    memcpy(date, temp, 11);
}

void to_time(char *time)
{
    // input: 035032.0
    // output: 03:50:32
    char temp[10];
    temp[0] = time[0];
    temp[1] = time[1];
    temp[2] = ':';
    temp[3] = time[2];
    temp[4] = time[3];
    temp[5] = ':';
    temp[6] = time[4];
    temp[7] = time[5];
    temp[10] = 0;
    memcpy(time, temp, 10);
}
/******************************************************************
*********                       EOF                       *********
*******************************************************************
********* Dev. by Sanchitha Dias (sanchithadias@gmail.com)*********
******************************************************************/
