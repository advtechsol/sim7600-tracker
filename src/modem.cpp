/******************************************************************
*********                       Includes                  *********
******************************************************************/
#include <Arduino.h>
#include <avr/dtostrf.h>
#include <FlashStorage.h>

#include "..\Board.h"
#include "..\Defines.h"

#include "modem.h"
#include "logger.h"
#include "sensor.h"
#include "util.h"
/******************************************************************
*********                  Local Defines                  *********
******************************************************************/

/******************************************************************
*********                Local Enumerations               *********
******************************************************************/

/******************************************************************
*********                  Local Objects                  *********
******************************************************************/

/******************************************************************
*********                  Local Variables                *********
******************************************************************/
extern Config settings;

volatile bool flagGNS = false;
volatile bool flagHTTP = false;
volatile bool flagSendSMS = false;

static const int modem_size = 180;      // Size of Modem serila buffer
static char modem_buffer[modem_size]; // Modem serial buffer
char commandBuffer[modem_size]; // command to POST coordinates to server

const int memory_buffer = 10;
char memory[memory_buffer][modem_size];
volatile uint8_t memoryCounter = 0;

char latitude[13];
char longitude[13];
char date[12];
char timeBuf[9];
char lat_indicator[2];
char lng_indicator[2];
char altitude[7];
char speed[7];
char course[7];
char hdop[7];
char GSN[17];
char phone[20];

volatile float lat;
volatile float lng;
volatile float spd; // request

volatile bool flagUpload = false;

//List of commands saved in flash
//SMS configuration headers
const char sSER[] PROGMEM = "serv";
const char sSTA[] PROGMEM = "stat";
const char sLOG[] PROGMEM = "logg";
const char sUPL[] PROGMEM = "uplo";
const char sREC[] PROGMEM = "reco";
const char sSMS[] PROGMEM = "sms";

//Modem responses
const char mOK[]    PROGMEM = "OK";
const char mERROR[] PROGMEM = "ERRO";
const char mRDY[]   PROGMEM = "PB DONE";
const char mHTTP[]  PROGMEM = "+HTTP";
const char mGSN[]   PROGMEM = "8639"; // this is SIMCOMS id found on serial number
const char mCGN[]   PROGMEM = "+CGN";
const char mSMS[]   PROGMEM = "#*,";

uint32_t uploadNowTimeS = 0;
/******************************************************************
*********          Local Function Definitions             *********
******************************************************************/
bool init_modem_gpio(void);
bool modem_reset_state(bool reset_state);
void modem_power(void);
bool modem_reset(void);
ModemResponseState_t wait_ok(uint16_t timeout);
ModemResponseState_t get_modem_response(void);
void create_command(const char *server);
/******************************************************************
*********              Application Firmware               *********
******************************************************************/
bool init_modem(void)
{
    debug_println("Init Modem");
    init_modem_gpio();
    modem_reset_state(HIGH);
    modem_turn_pwr(HIGH);

    // Modem init
    Serial1.begin(115200);
    while(!Serial1)
    {
        debug_println("Modem serial failed!");
        os_delay_Ms(500);
    }

    while(state_modem_error != send_command("?", 15))
    {
        debug_println("Waiting for modem to respond");
        os_delay_Ms(500);
    }

    debug_println("Modem initialized");

    uploadNowTimeS = millis() / 1000;

    return true;
}

bool init_modem_gpio(void)
{
    pinMode(GPIO_OUTPUT_MODEM_PWR_EN, OUTPUT);
    digitalWrite(GPIO_OUTPUT_MODEM_PWR_EN, HIGH);
    os_delay_Ms(500);

    return true;
}

bool modem_turn_pwr(bool on_off_state)
{
    if(on_off_state)
    {
        digitalWrite(GPIO_OUTPUT_MODEM_PWR_EN, LOW);
        os_delay_Ms(1000);
        digitalWrite(GPIO_OUTPUT_MODEM_PWR_EN, HIGH);
        os_delay_Ms(100);
    }
    else
    {
        digitalWrite(GPIO_OUTPUT_MODEM_PWR_EN, LOW);
        os_delay_Ms(2500);
        digitalWrite(GPIO_OUTPUT_MODEM_PWR_EN, HIGH);
        os_delay_Ms(100);
    }

    return true;
}

bool modem_reset_state(bool reset_state)
{
    // digitalWrite(GPIO_OUTPUT_MODEM_7600_RST, reset_state);

    return true;
}

bool modem_reset(void)
{
    modem_reset_state(LOW);
    os_delay_Ms(10);
    modem_reset_state(HIGH);
    os_delay_Ms(10);

    return true;
}

void modem_power(void)
{
    debug_println("... restart modem");
    modem_turn_pwr(LOW);
    os_delay_S(3);
    modem_turn_pwr(HIGH);
    os_delay_S(10);
}

// Read modem IMEI
void process_gsn(char *modem_buffer)
{
    memcpy(GSN, modem_buffer, 15);
    debug_print("... GSN: ");
    debug_println(GSN);
}

void process_gcn(char *modem_buffer)
{
    flagGNS = false;
    if (modem_buffer[10] == ':')
    {
        if (modem_buffer[12] == '2' || modem_buffer[12] == '3')
        { // if GNS is fixed
            flagGNS = true;
            split_chr(latitude, modem_buffer, ',', 4);      //debug_print("... latitude: ");      debug_println(latitude);
            split_chr(longitude, modem_buffer, ',', 6);     //debug_print("... longitude: ");     debug_println(longitude);
            split_chr(lat_indicator, modem_buffer, ',', 5); //debug_print("... lat_indicator: "); debug_println(lat_indicator);
            split_chr(lng_indicator, modem_buffer, ',', 7); //debug_print("... lng_indicador: "); debug_println(lng_indicator);
            lat = to_geo(latitude, lat_indicator);
            debug_print("... lat: ");
            debug_println(lat, 6);
            lng = to_geo(longitude, lng_indicator);
            debug_print("... lng: ");
            debug_println(lng, 6);
            dtostrf(lat, 7, 6, latitude);
            dtostrf(lng, 7, 6, longitude);
            split_chr(date, modem_buffer, ',', 8);      //debug_print("... date: ");          debug_println(date);
            split_chr(timeBuf, modem_buffer, ',', 9);   //debug_print("... time: ");          debug_println(timeBuf);
            to_date(date);                              //debug_print("... date: ");          debug_println(date);
            to_time(timeBuf);                           //debug_print("... time: ");          debug_println(timeBuf);
            split_chr(altitude, modem_buffer, ',', 10); //debug_print("... altitude: ");      debug_println(altitude);
            split_chr(speed, modem_buffer, ',', 11);    //debug_print("... speed: ");         debug_println(speed);
            split_chr(course, modem_buffer, ',', 12);   //debug_print("... speed: ");         debug_println(speed);
            split_chr(hdop, modem_buffer, ',', 14);     //debug_print("... hdop: ");          debug_println(hdop);
            spd = atof(speed);
            debug_print("... spd: ");
            debug_println(spd);
        }
    }
}

float get_speed(void)
{
    return spd;
}

void send_sms(void)
{
    // SMS Format:
    // Track:
    // date time
    // https://maps.google.com/?q=<lat>,<lng>
    // speed
    char ctrl_z = 26;
    char sms_command[40];
    char sms_message[140];
    sprintf(sms_command, "AT+CMGS=\"%s\"\r", phone);
    sprintf(sms_message, "Track:\n%s %s\nhttps://maps.google.com/?q=%s,%s\nspd: %s\n", date, timeBuf, latitude, longitude, speed);
    debug_print("... send sms:");
    debug_println(sms_command);
    debug_print("... message:");
    debug_println(sms_message);
    Serial1.print(sms_command);
    os_delay_S(1);
    Serial1.print(sms_message);
    Serial1.print(ctrl_z);
    wait_ok(10);
}

bool process_sms(char *modem_buffer)
{
    // incomming SMS for configuration purposes, examples:
    // #*,server,http://iotnetwork.com.au:5055/,
    // #*,stationary,300,
    // #*,logging,20,
    // #*,upload,300,
    // #*,recovery,1,
    // #*,sms,+51984894723,
    char sms_command[40];
    char sms_value[100];
    bool settingFlag = false;
    split_chr(sms_command, modem_buffer, ',', 1);
    split_chr(sms_value, modem_buffer, ',', 2);
    debug_println("... SMS command");
    debug_print("... command: ");
    debug_println(sms_command);
    debug_print("... value: ");
    debug_println(sms_value);
    if (memcmp(sSER, sms_command, 4) == 0)
    {
        memcpy(settings.server, sms_value, strlen(sms_value));
        settingFlag = true;
    }
    if (memcmp(sSTA, sms_command, 4) == 0)
    {
        settings.noMotionPeriod = atoi(sms_value);
        settingFlag = true;
    }
    if (memcmp(sLOG, sms_command, 4) == 0)
    {
        settings.inMotionPeriod = atoi(sms_value);
        settingFlag = true;
    }
    if (memcmp(sUPL, sms_command, 4) == 0)
    {
        settings.noMotionUploadPeriod = atoi(sms_value);
        settingFlag = true;
    }
    if (memcmp(sREC, sms_command, 4) == 0)
    {
        settings.recovery = atoi(sms_value);
        settingFlag = true;
    }
    if (memcmp(sSMS, sms_command, 3) == 0)
    {
        memcpy(phone, sms_value, strlen(sms_value));
        flagSendSMS = true;
    }

    return settingFlag;
}

ModemResponseState_t send_command(const char *command, uint16_t timeout)
{
    Serial1.print("AT+" + String(command) + "\r");
    return wait_ok(timeout);
}

ModemResponseState_t wait_ok(uint16_t timeout)
{
    timeout = timeout * 10;
    uint16_t t = 0;
    ModemResponseState_t modemResponse;
    while (timeout > t)
    {
        t++;
        modemResponse = get_modem_response();
        if (modemResponse > state_no_response)
        {
            os_delay_Ms(100);
            return modemResponse;
        }
        os_delay_Ms(100);
    }
    modem_power();
    return modemResponse;
}

ModemResponseState_t get_modem_response(void)
{
    char modem_char;
    uint8_t modem_i = 0;
    ModemResponseState_t modemResponse = state_no_response;

    while (Serial1.available())
    {
        modem_char = Serial1.read();
        debug_print(modem_char);
        modem_buffer[modem_i] = modem_char;
        modem_i++;
        if (modem_i >= modem_size)
            modem_i = 0;
        if ((modem_i >= 2) && ((modem_char == '\r') || (modem_char == '\n')))
        {
            modem_buffer[modem_i] = '\0';

            if (strstr(modem_buffer, mOK))
            {
                modemResponse = state_modem_ok;
                goto  MODEM_RESPONSE;
            }
            if (strstr(modem_buffer, mERROR))
            {
                modemResponse = state_modem_error;
                goto  MODEM_RESPONSE;
            }
            if (strstr(modem_buffer, mRDY))
            {
                modemResponse = state_modem_ready;
                goto  MODEM_RESPONSE;
            }
            if (strstr(modem_buffer, mHTTP))
            {
                modemResponse = state_modem_http;
                // goto  MODEM_RESPONSE;
            }
            if (strstr(modem_buffer, mGSN))
            {
                process_gsn(modem_buffer);
                modemResponse = state_modem_gsn;
                // goto  MODEM_RESPONSE;
            }
            if (strstr(modem_buffer, mCGN))
            {
                process_gcn(modem_buffer);
                modemResponse = state_modem_cgn;
                // goto  MODEM_RESPONSE;
            }
            if (strstr(modem_buffer, mSMS))
            {
                if (process_sms(modem_buffer))
                {
                    store_settings();
                    print_settings();
                    debug_println("... settings saved");
                    alarmMatch();
                }                
                modemResponse = state_modem_sms;
                // goto  MODEM_RESPONSE;
            }
            memset(modem_buffer, 0, modem_i);
            modem_i = 0;
        }
    }
    return state_no_response;

MODEM_RESPONSE:
    memset(modem_buffer, 0, modem_i);
    debug_println("Modem State: " + String(modemResponse));
    return modemResponse;
}

void create_command(const char *server)
{
    // http://iotnetwork.com.au:5055/?id=863922031635619&lat=-13.20416&lon=-72.20898&timestamp=1624031099&hdop=12&altitude=3400&speed=10
    sprintf(
        commandBuffer,
        "HTTPPARA=\"URL\",\"%s?id=%s&lat=%s&lon=%s&timestamp=%s%%20%s&hdop=%s&altitude=%s&speed=%s&heading=%s\"",
        server, GSN, latitude, longitude, date, timeBuf, hdop, altitude, speed, course);
}

bool sms_config(void)
{
    bool result = (state_modem_ok == send_command(PSTR("CSMS=0"), 5)) &&
                  (state_modem_ok == send_command(PSTR("CPMS=\"ME\",\"ME\",\"ME\""), 5)) &&
                  (state_modem_ok == send_command(PSTR("CMGF=1"), 5)) /*&&
                  (state_modem_ok == send_command(PSTR("CNMI=2,2"), 5))*/;
    return result;
}

void save_on_memory(volatile uint8_t *memoryCounterPtr, char *commandBufferPtr)
{
    int len = strlen(commandBufferPtr);
    for (int z = 0; z < len; z++)
    {
        memory[*memoryCounterPtr][z] = commandBufferPtr[z];
    }
    memory[*memoryCounterPtr][len] = 0;
    (*memoryCounterPtr)++;
    if (*memoryCounterPtr >= (memory_buffer - 8))
    {
        flagUpload = true;
    }
}

void get_location(void)
{
    uint32_t timeNow = millis();
    uint32_t timeOut = millis();

    while (!flagGNS || (millis() - timeNow < (5 * 60 * 1000)))
    {
        timeOut = millis();
        while ((state_modem_ok != send_command(PSTR("CGNSSINFO"), 10)) && (millis() - timeOut < APP_MODEM_TIMEOUT_MS))
            ;
        if (flagGNS)
        {
            set_rtc(date, timeBuf);
            debug_print("Log sensor data! ");
            memset(commandBuffer, 0, sizeof(commandBuffer));
            create_command(settings.server);
            save_on_memory(&memoryCounter, commandBuffer);
            // flagGNS = false;
            break;
        }
        os_delay_Ms(1000);
    }
    flagGNS = false;
}

void turn_rf_on(void)
{
    send_command(PSTR("CFUN=1"), 5);
    os_delay_S(5);
}

void turn_rf_off(void)
{
    send_command(PSTR("CFUN=0"), 5);
}

bool init_http(void)
{
    uint32_t timeNow = millis();
    volatile ModemResponseState_t modemStatus = state_no_response;
    // stop_http();
    send_command(PSTR("HTTPTERM"), 3);
    os_delay_Ms(100);
    while((state_modem_ok != modemStatus) && (millis() - timeNow < APP_MODEM_TIMEOUT_MS))
    {
        modemStatus = send_command(PSTR("HTTPINIT"), 3);
        os_delay_Ms(100);
    }

    return !(bool)modemStatus;
}

bool post_http(void)
{
    uint32_t timeNow = millis();
    volatile ModemResponseState_t modemStatus = state_no_response;

    send_command(commandBuffer, 5);
    while ((state_modem_ok != modemStatus) && (millis() - timeNow < APP_MODEM_TIMEOUT_MS))
    {
        modemStatus = send_command(PSTR("HTTPACTION=1"), 15);
        os_delay_Ms(100);
    }

    return !(bool)modemStatus;
}

bool stop_http(void)
{
    uint32_t timeNow = millis();
    volatile ModemResponseState_t modemStatus = state_no_response;
    
    while ((state_modem_ok != modemStatus) && (millis() - timeNow < APP_MODEM_TIMEOUT_MS))
    {
        modemStatus = send_command(PSTR("HTTPTERM"), 3);
        os_delay_Ms(100);
    }

    return !(bool)modemStatus;
}

bool upload_location(void)
{
    if(check_movement_timeout())
    {
        if(millis() / 1000 - uploadNowTimeS > settings.inMotionUploadPeriod)
        {
            uploadNowTimeS = millis() / 1000;
            flagUpload = true;
        }
    }
    else
    {
        if(millis() / 1000 - uploadNowTimeS > settings.noMotionUploadPeriod)
        {
            uploadNowTimeS = millis() / 1000;
            flagUpload = true;
        }
    }
    if (!flagUpload)
    {
        return false;
    }

    if (spd < 1)
    {
        turn_rf_on();
    }
    if(!init_http())
    {
        return false;
    }

    debug_println("http initialized!");

    for (int i = 0; i < memoryCounter; i++)
    {
        memset(commandBuffer, 0, sizeof(commandBuffer));
        memcpy(commandBuffer, memory[i], strlen(memory[i]));
        // debug_println(memory[i]);
        if(!post_http())
        {
            return false;
        }
    }
    memoryCounter = 0;
    if(!stop_http())
    {
        return false;
    }
    if (spd < 1)
        turn_rf_off();
    flagUpload = false;

    return true;
}

bool modem_stop(void)
{
    debug_println("Deinitializing modem!");
    Serial1.end();
    return true;
}
/******************************************************************
*********                       EOF                       *********
*******************************************************************
********* Dev. by Sanchitha Dias (sanchithadias@gmail.com)*********
******************************************************************/