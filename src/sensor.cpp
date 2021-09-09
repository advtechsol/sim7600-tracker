/******************************************************************
*********                       Includes                  *********
******************************************************************/
#include <Arduino.h>
#include <NMEAGPS.h>
#include <rBase64.h>

#include "..\Board.h"
#include "..\Defines.h"

#include "sensor.h"
#include "util.h"
#include "logger.h"
/******************************************************************
*********                  Local Defines                  *********
******************************************************************/

/******************************************************************
*********                Local Enumerations               *********
******************************************************************/

/******************************************************************
*********                  Local Objects                  *********
******************************************************************/
NMEAGPS gps;
gps_fix fix;                        // the current GPS fix
HardwareSerial &gps_port = Serial1; // an alias
/******************************************************************
*********                  Local Variables                *********
******************************************************************/
// Variables used by Loop
int year;
byte month, day, hour, minute, second, hundredths;
unsigned long dateFix, locationFix;
static float latitude, longitude;
static long altitude;
static float speed;
short satellites;
static long course;

uint8_t gpsSeconds; // for counting elapsed time instead of using delay
uint8_t fixSeconds; // for counting elapsed time instead of using delay

unsigned long tnow;

bool fixFound = false;

volatile bool use_movement_interval = 0;
volatile uint32_t last_movement_time = 0;

static uint8_t payload[32];
static uint8_t payloadBase64[32] = {0};

extern Config settings;
/******************************************************************
*********          Local Function Definitions             *********
******************************************************************/
bool init_gps_gpio(void);
/******************************************************************
*********              Application Firmware               *********
******************************************************************/
bool init_gps(void)
{
    debug_println("Init GPS");
    init_gps_gpio();
    gps_turn_pwr(HIGH);

    return true;
}

bool init_gps_gpio(void)
{
    // pinMode(GPIO_OUTPUT_GPS_PWR_EN, OUTPUT);

    return true;
}

bool gps_turn_pwr(bool on_off_state)
{
    // digitalWrite(GPIO_OUTPUT_GPS_PWR_EN, on_off_state);

    return true;
}

// UF - modified refereneces to radiopacket to be to the radiopacket array.
bool readGPS(void)
{
    bool status = false;

    gps_turn_pwr(HIGH);

    // UF - why is the readGPS() routine clearing the packet string when it doesn't use it?
    // This should be somewhere else like in build_radiopacket().
    // sprintf(radiopacket, "");

    gps_port.begin(9600);
    for (tnow = millis(); millis() - tnow < 10UL * 60UL * 1000UL;) //keep checking GPS for 'n' minutes if no fix is found
    {
        if (gps.available(gps_port))
        {
            fix = gps.read(); // save the latest

            // When we have a location, calculate how far away we are from the base location.
            if (fix.valid.location)
            {
                blinky(2, BLINK_TIME_INTERVAL);
                digitalWrite(GPIO_OUTPUT_STATUS_LED_0, HIGH);
                latitude = fix.latitude();
                longitude = fix.longitude();
                altitude = fix.altitude_cm();
                speed = fix.speed_kph() * 1000 / 3600;
                satellites = fix.satellites;

                year = fix.dateTime.year;
                month = fix.dateTime.month;
                day = fix.dateTime.date;
                hour = fix.dateTime.hours;
                minute = fix.dateTime.minutes;
                second = fix.dateTime.seconds;
                hundredths = fix.dateTime_cs;
                course = fix.heading_cd();
                //            hdop = fix.hdop();

                if (fix.valid.date && fix.valid.time && fix.valid.location && fix.valid.altitude && fix.valid.speed && fix.valid.heading)
                {
                    fixFound = 1;
                }

                fixSeconds++;
                if (fixSeconds >= 3)
                {
                    fixSeconds = 0;
                    status = true;
                    // displayInfo();
                    break;
                }
            }
            else
                // Waiting...
                debug_println("waiting for gps...");

            // Instead of delay, count the number of GPS fixes
            gpsSeconds++;
            if (gpsSeconds >= 3)
            {
                gpsSeconds = 0; // reset counter after 3
            }
        }
    }
    gps_stop();
    gps_turn_pwr(LOW);

    return status;
} // End CheckGPS

bool gps_stop(void)
{
    gps_port.end();
    return true;
}

float get_latitude(void)
{
    return fix.latitude();
}

float get_longitude(void)
{
    return fix.longitude();
}

float get_altitude(void)
{
    return fix.altitude();
}

bool init_tilt_sensor(void)
{
    pinMode(GPIO_INTERRUPT_TILT_SENSOR, INPUT_PULLUP);
    attachInterruptWakeup(GPIO_INTERRUPT_TILT_SENSOR, tilt_sensor_isr, CHANGE);

    return true;
}

void tilt_sensor_isr()
{
    last_movement_time = millis(); //Noting the time when a movement occured.

    //if NOT already in movement_interval enabled mode...
    if (!use_movement_interval)
    {
        debug_println("First time interrupt");
        //update alarm setting..
        use_movement_interval = true;
    }
}

bool check_movement_timeout(void)
{
    if(millis() - last_movement_time > (settings.inMotionPeriod * 2 * 1000))
    {
        use_movement_interval = false;   
    }
    return use_movement_interval;
}

bool log_sensor_data(void)
{
    int32_t payloadLatitude = 0, payloadLongitude = 0, payloadAltitude = 0;
    uint32_t rtcEpochTimeNow = 0;
    uint8_t *payloadPtr = payload;
    // Check if there is not a current TX/RX job running
    memset(payload, 0, sizeof(payload));
    memset(payloadBase64, 0, sizeof(payloadBase64));

    // Prepare upstream data transmission at the next possible time.
    if(readGPS())
    {
        set_rtc();
    }

    debug_println("GPS Location: Lat: " + String(get_latitude()) + ", Lon: " + String(get_longitude()) + ", Alt: " + String(get_latitude()));

    get_epoch_time_bytes(payloadPtr);

    payloadLatitude = get_latitude() * 1000000;

    *payloadPtr++ = (uint8_t)(payloadLatitude & 0xff);
    *payloadPtr++ = (uint8_t)((payloadLatitude >> 8) & 0xff);
    *payloadPtr++ = (uint8_t)((payloadLatitude >> 16) & 0xff);
    *payloadPtr++ = (uint8_t)((payloadLatitude >> 24) & 0xff);

    payloadLongitude = get_longitude() * 1000000;

    *payloadPtr++ = (uint8_t)(payloadLongitude & 0xff);
    *payloadPtr++ = (uint8_t)((payloadLongitude >> 8) & 0xff);
    *payloadPtr++ = (uint8_t)((payloadLongitude >> 16) & 0xff);
    *payloadPtr++ = (uint8_t)((payloadLongitude >> 24) & 0xff);

    payloadAltitude = get_latitude() * 100;

    *payloadPtr++ = (uint8_t)(payloadAltitude & 0xff);
    *payloadPtr++ = (uint8_t)((payloadAltitude >> 8) & 0xff);

    read_batV_buf(payloadPtr);

    //Encode the data to base 64
    rbase64_encode((char *)payloadBase64, (char *)payload, payloadPtr - payload - 1);
    debug_println((char *)payloadBase64);
    writetosd((char *)payloadBase64);

    debug_println(F("Packet queued in the SD Card"));
    // Next TX is scheduled after TX_COMPLETE event.
}

float read_batV_buf(uint8_t *dataBuf)
{
    uint16_t measuredvbat = 0;

    for(uint8_t i = 0; i < 10; i++)
    {
        measuredvbat += analogRead(ADC_BATTERY_VOLTAGE);
        os_delay_Ms(10);
    }
    measuredvbat /= 10;
    measuredvbat = ((float)measuredvbat * 4.3 * 3.3) / 1.023;
    debug_print("VBat: ");
    debug_println(measuredvbat);
    *dataBuf++ = (unsigned char)(measuredvbat & 0xff); //we're unsigned
    *dataBuf++ = (unsigned char)((measuredvbat >> 8) & 0xff); //we're unsigned

    return measuredvbat/100;
}

/**
 * @brief Measures battery voltage in milli volts
 * */
uint16_t get_batt_mvolts(void)
{
    uint16_t measuredvbat = 0;

    for(uint8_t i = 0; i < 10; i++)
    {
        measuredvbat += analogRead(ADC_BATTERY_VOLTAGE);
        os_delay_Ms(10);
    }
    measuredvbat /= 10;
    measuredvbat = ((float)measuredvbat * 4.3 * 3.3) / 1.023;
    debug_print("Battery Volatge (mV): ");
    debug_println(measuredvbat);
    return measuredvbat;
}
/******************************************************************
*********                       EOF                       *********
*******************************************************************
********* Dev. by Sanchitha Dias (sanchithadias@gmail.com)*********
******************************************************************/
