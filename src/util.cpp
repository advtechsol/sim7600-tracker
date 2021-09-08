/******************************************************************
*********                       Includes                  *********
******************************************************************/
#include <Arduino.h>
#include <RTCZero.h>
#include <NMEAGPS.h>
#include <FreeRTOS_SAMD21.h>
#include <time.h>

#include "..\Board.h"
#include "..\Defines.h"

#include "util.h"
#include "sensor.h"
#include "modem.h"
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

FlashStorage(storage, Config);
/******************************************************************
*********                  Local Variables                *********
******************************************************************/
volatile bool logSensorDataNow = false; //a variable to keep check of the state.
volatile int edges[2];

Config settings;
/******************************************************************
*********          Local Function Definitions             *********
******************************************************************/

/******************************************************************
*********              Application Firmware               *********
******************************************************************/
/**********                 LED Firmware                **********/
bool init_leds(void)
{
    debug_println("Init Led's");
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
        os_delay_Ms(time);
        digitalWrite(GPIO_OUTPUT_STATUS_LED_0, LOW);
        os_delay_Ms(time);
    }
}
/******************************************************************/
/**********                   RTC Firmware               **********/
bool init_rtc(void)
{
    debug_println("Init RTC");
    rtc.begin();                           // Start the RTC now that BEACON_INTERVAL has been updated
    rtc.setAlarmSeconds(rtc.getSeconds()); // Initialise RTC Alarm Seconds

    alarmMatch();                      // Set next alarm time using updated BEACON_INTERVAL
    rtc.enableAlarm(rtc.MATCH_HHMMSS); // Alarm Match on hours, minutes and seconds
    rtc.attachInterrupt(alarmMatch);   // Attach alarm interrupt

    return true;
}

void set_rtc()
{
    debug_println("set rtc time, from gps time (UTC)...");
    rtc.setTime(fix.dateTime.hours, fix.dateTime.minutes, fix.dateTime.seconds);
    rtc.setDate(fix.dateTime.date, fix.dateTime.month, fix.dateTime.year);
    debug_println("RTC ALARM");
    debug_print("current minutes: ");
    debug_println(rtc.getMinutes());
    debug_print("alarm will activate in ");
    debug_println(rtc.getMinutes() + APP_DEVICE_NO_MOTION_INTERVAL_S / 60);
    debug_print(rtc.getYear());
    debug_print(rtc.getMonth());
    debug_println(rtc.getDay());
}

void set_rtc(const char* dateToday, const char* timeNow)
{
    //2021-06-21 @param dateToday
    //03:50:32 @param timeNow   
    struct tm timeGPS;

    //2021-06-21
    debug_print(dateToday);
    sscanf(dateToday, "%04d-%02d-%02d", &timeGPS.tm_year, &timeGPS.tm_mon, &timeGPS.tm_mday);
    timeGPS.tm_year %= 1000;
    //03:50:32            
    debug_print(timeNow);
    sscanf(timeNow, "%02d:%02d:%02d", &timeGPS.tm_hour, &timeGPS.tm_min, &timeGPS.tm_sec);

    debug_println("set rtc time, from gps time (UTC)...");
    rtc.setTime(timeGPS.tm_hour, timeGPS.tm_min, timeGPS.tm_sec);
    rtc.setDate(timeGPS.tm_mday, timeGPS.tm_mon, timeGPS.tm_year);
    debug_println("RTC ALARM");
    debug_print("current minutes: ");
    debug_println(rtc.getMinutes());
    debug_print("alarm will activate in ");
    debug_println(rtc.getMinutes() + APP_DEVICE_NO_MOTION_INTERVAL_S / 60);
    debug_print(rtc.getYear());
    debug_print(rtc.getMonth());
    debug_println(rtc.getDay());
}

void set_alarm()
{
    debug_println("set rtc alarm");
    os_delay_Ms(250);
    rtc.begin();                           // Start the RTC now that BEACON_INTERVAL has been updated
    rtc.setAlarmSeconds(rtc.getSeconds()); // Initialise RTC Alarm Seconds
    alarmMatch();                          // Set next alarm time using updated BEACON_INTERVAL
    debug_print("iterationCounter");
    // debug_println(get_iteration_counter());
    int rtc_mins = rtc.getMinutes(); // Read the RTC minutes
    debug_print("rtc_mins: ");
    debug_println(rtc_mins);
    int rtc_hours = rtc.getHours(); // Read the RTC hours
    debug_print("rtc_hours: ");
    debug_println(rtc_hours);
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
    modem_stop();
    os_delay_Ms(1000); // Wait for serial ports to clear

    // Turn LED off
    blinky(10, BLINK_TIME_INTERVAL);

    // Turn off LORA
    // try keeping the LORA on... digitalWrite(GPIO_OUTPUT_GPS_PWR_EN, LOW);
    if (check_movement_timeout())
    {
        debug_println("BEACON_INTERVAL too short, not economic to switch off GPS");
    }
    else
    {
        modem_turn_pwr(LOW); // Disable the GPS
    }

    // Close and detach the serial console (as per CaveMoa's SimpleSleepUSB)
    debug_println("Going to sleep until next alarm time...");
    os_delay_Ms(1000);        // Wait for serial port to clear
    close_debug_serial();       // Close the serial console
    USBDevice.detach(); // Safely detach the USB prior to sleeping
    os_delay_Ms(250);
    // Sleep until next alarm match
    rtc.standbyMode();
}

void alarmMatch()
{
    uint32_t rtc_secs = rtc.getSeconds(); //Read RTC seconds
    uint16_t rtc_mins = rtc.getMinutes(); // Read the RTC minutes
    uint8_t rtc_hours = rtc.getHours();  // Read the RTC hours

    if(settings.recovery)
    {
        debug_println("recovery ALARM occured...");
        rtc_secs = rtc_secs + settings.recoveryPeriod;
    }
    else if (check_movement_timeout())
    {
        debug_println("While in motion ALARM occured...");
        rtc_secs = rtc_secs + settings.inMotionPeriod; // Add the BEACON_INTERVAL to the RTC minutes}
    }
    else
    {
        debug_println("no motion ALARM occured...");
        rtc_secs = rtc_secs + settings.noMotionPeriod; // Add the MOVEMENT_INTERVAL to the RTC minutes}
    }

    while (rtc_secs >= 60)
    {                   // If there has been an hour roll over
        rtc_secs -= 60; // Subtract 60 seconds
        rtc_mins += 1; // Add a minute
    }

    while (rtc_mins >= 60)
    {                   // If there has been an hour roll over
        rtc_mins -= 60; // Subtract 60 minutes
        rtc_hours += 1; // Add an hour
    }

    rtc_hours = rtc_hours % 24;    // Check for a day roll over
    rtc.setAlarmSeconds(rtc_secs); // Set next alarm time (seconds)
    rtc.setAlarmMinutes(rtc_mins); // Set next alarm time (minutes)
    rtc.setAlarmHours(rtc_hours);  // Set next alarm time (hours)

    logSensorDataNow = true;
} // end alarmMatch

void wake_device()
{
    digitalWrite(GPIO_OUTPUT_STATUS_LED_0, LOW);

    USBDevice.attach(); // Re-attach the USB, audible sound on windows machine
    os_delay_Ms(250);         // Delay added to make serial more reliable
    init_debug_serial();
    os_delay_Ms(3000);
    debug_println("device awake");

    if(!check_movement_timeout())
    {
        init_modem();
    }
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
/******************************************************************/
/**********                   Flash Storage              **********/
void check_config()
{
    storage.read(&settings);
    if (settings.valid)
    {
        debug_println("... settings found");
    }
    else
    {
        debug_println("... settings not found");
        const char default_domain[] = "http://iotnetwork.com.au:5055/";
        memcpy(settings.server, default_domain, strlen(default_domain));
        settings.noMotionPeriod = APP_DEVICE_NO_MOTION_INTERVAL_S;
        settings.inMotionPeriod = APP_DEVICE_IN_MOTION_INTERVAL_S;
        settings.noMotionUploadPeriod = APP_DEVICE_NO_MOTION_UPLOAD_INTERVAL_S;
        settings.inMotionUploadPeriod = APP_DEVICE_IN_MOTION_UPLOAD_INTERVAL_S;
        settings.recoveryPeriod = APP_DEVICE_RECOVERY_INTERVAL_S;
        settings.recovery = false;
        settings.valid = true;
        store_settings();
        debug_println("... settings saved");
    }
    print_settings();
}

void get_settings(Config *ptrSettings)
{
    ptrSettings = &settings;
}

void store_settings(void)
{
    storage.write(settings);
}

void print_settings(void)
{
    debug_print("... server: ");
    debug_println(settings.server);
    debug_print("... no motion: ");
    debug_println(settings.noMotionPeriod);
    debug_print("... in motion: ");
    debug_println(settings.inMotionPeriod);
    debug_print("... no motion upload: ");
    debug_println(settings.noMotionUploadPeriod);
    debug_print("... in motion upload: ");
    debug_println(settings.inMotionUploadPeriod);
    debug_print("... in recovery period: ");
    debug_println(settings.recoveryPeriod);
    debug_print("... recovery: ");
    debug_println(settings.recovery);
}
/******************************************************************/
/**********                   Debu Serial                **********/
void init_debug_serial(void)
{
#if APP_MODE_DEBUG
    Serial.begin(230400);
    // while(!Serial); //Uncomment for debugging purposes

    os_delay_Ms(2000);
#endif
}

void debug_print(String buf)
{
#if APP_MODE_DEBUG
    Serial.print(buf);
#endif
}

void debug_print(char c)
{
#if APP_MODE_DEBUG
    Serial.print(c);
#endif
}

void debug_print(int i)
{
#if APP_MODE_DEBUG
    Serial.print(i);
#endif
}

void debug_print(float f)
{
#if APP_MODE_DEBUG
    Serial.print(f);
#endif
}

void debug_print(float f, uint8_t decimals)
{
#if APP_MODE_DEBUG
    Serial.print(f, decimals);
#endif
}

void debug_println(String buf)
{
#if APP_MODE_DEBUG
    Serial.println(buf);
#endif
}

void debug_println(char c)
{
#if APP_MODE_DEBUG
    Serial.println(c);
#endif
}

void debug_println(int i)
{
#if APP_MODE_DEBUG
    Serial.println(i);
#endif
}

void debug_println(float f)
{
#if APP_MODE_DEBUG
    Serial.println(f);
#endif
}

void debug_println(float f, uint8_t decimals)
{
#if APP_MODE_DEBUG
    Serial.println(f, decimals);
#endif
}

void close_debug_serial(void)
{
#if APP_MODE_DEBUG
    Serial.end();
#endif
}
/******************************************************************/
/**********                   Delay Helpers              **********/
void os_delay_Us(uint16_t us)
{
    delayMicroseconds(us);
}

void os_delay_Ms(uint16_t ms)
{
    delay(ms);
}

void os_delay_S(uint16_t s)
{
    while (s > 0)
    {
        os_delay_Ms(1000);
        s--;
    }
}
/******************************************************************/
/**********               String Operations              **********/
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
