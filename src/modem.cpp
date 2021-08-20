/******************************************************************
*********                       Includes                  *********
******************************************************************/
#include <Arduino.h>
#include <avr/dtostrf.h>

#include "..\Board.h"
#include "..\Defines.h"

#include "modem.h"
#include "logger.h"
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

/******************************************************************
*********                  Local Variables                *********
******************************************************************/
volatile bool flagOK = false;
volatile bool flagERROR = false;
volatile bool flagGNS = false;
volatile bool flagHTTP = false;
volatile bool flagSendSMS = false;

static const int modem_size = 180;      // Size of Modem serila buffer
char command_buffer[modem_size]; // command to POST coordinates to server

const int memory_buffer = 10;
char memory[memory_buffer][modem_size];
volatile int memory_counter = 0;

char latitude[13];
char longitude[13];
char date[12];
char time[9];
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
/******************************************************************
*********          Local Function Definitions             *********
******************************************************************/
bool init_modem_gpio(void);
bool modem_reset_state(bool reset_state);
bool modem_turn_pwr(bool on_off_state);
bool modem_reset(void);
bool wait_ok(int timeout);
void create_command(const char *server);
/******************************************************************
*********              Application Firmware               *********
******************************************************************/
bool init_modem(void)
{
    Serial.println("Init Modem");
    init_modem_gpio();
    modem_reset_state(HIGH);
    modem_turn_pwr(HIGH);
    delay(2000);
    modem_reset();
    delay(1000);

    // Modem init

    return true;
}

bool init_modem_gpio(void)
{
    pinMode(GPIO_OUTPUT_MODEM_PWR_EN, OUTPUT);

    return true;
}

bool modem_turn_pwr(bool on_off_state)
{
    digitalWrite(GPIO_OUTPUT_MODEM_PWR_EN, on_off_state);

    return true;
}

bool modem_reset_state(bool reset_state)
{
    // digitalWrite(GPIO_OUTPUT_LORA_RFM95_RST, reset_state);

    return true;
}

bool modem_reset(void)
{
    modem_reset_state(LOW);
    delay(10);
    modem_reset_state(HIGH);
    delay(10);

    return true;
}

void modem_power(void)
{
    Serial.println("... restart modem");
    digitalWrite(GPIO_OUTPUT_MODEM_PWR_EN, LOW);
    os_delay_S(3);
    digitalWrite(GPIO_OUTPUT_MODEM_PWR_EN, HIGH);
    os_delay_S(10);
}

void turn_rf_on(void)
{
    send_command("CFUN=1", 5);
    os_delay_S(5);
}

void turn_rf_off(void)
{
    send_command("CFUN=0", 5);
}

// Read modem IMEI
void process_gsn(char *modem_buffer)
{
    memcpy(GSN, modem_buffer, 15);
    Serial.print("... GSN: ");
    Serial.println(GSN);
}

void process_gcn(char *modem_buffer)
{
    flagGNS = false;
    if (modem_buffer[10] == ':')
    {
        if (modem_buffer[12] == '2' || modem_buffer[12] == '3')
        { // if GNS is fixed
            flagGNS = true;
            split_chr(latitude, modem_buffer, ',', 4);      //Serial.print("... latitude: ");      Serial.println(latitude);
            split_chr(longitude, modem_buffer, ',', 6);     //Serial.print("... longitude: ");     Serial.println(longitude);
            split_chr(lat_indicator, modem_buffer, ',', 5); //Serial.print("... lat_indicator: "); Serial.println(lat_indicator);
            split_chr(lng_indicator, modem_buffer, ',', 7); //Serial.print("... lng_indicador: "); Serial.println(lng_indicator);
            lat = to_geo(latitude, lat_indicator);
            Serial.print("... lat: ");
            Serial.println(lat, 6);
            lng = to_geo(longitude, lng_indicator);
            Serial.print("... lng: ");
            Serial.println(lng, 6);
            dtostrf(lat, 7, 6, latitude);
            dtostrf(lng, 7, 6, longitude);
            split_chr(date, modem_buffer, ',', 8);      //Serial.print("... date: ");          Serial.println(date);
            split_chr(time, modem_buffer, ',', 9);      //Serial.print("... time: ");          Serial.println(time);
            to_date(date);                              //Serial.print("... date: ");          Serial.println(date);
            to_time(time);                              //Serial.print("... time: ");          Serial.println(time);
            split_chr(altitude, modem_buffer, ',', 10); //Serial.print("... altitude: ");      Serial.println(altitude);
            split_chr(speed, modem_buffer, ',', 11);    //Serial.print("... speed: ");         Serial.println(speed);
            split_chr(course, modem_buffer, ',', 12);   //Serial.print("... speed: ");         Serial.println(speed);
            split_chr(hdop, modem_buffer, ',', 14);     //Serial.print("... hdop: ");          Serial.println(hdop);
            spd = atof(speed);
            Serial.print("... spd: ");
            Serial.println(spd);
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
    sprintf(sms_message, "Track:\n%s %s\nhttps://maps.google.com/?q=%s,%s\nspd: %s\n", date, time, latitude, longitude, speed);
    Serial.print("... send sms:");
    Serial.println(sms_command);
    Serial.print("... message:");
    Serial.println(sms_message);
    Serial1.print(sms_command);
    os_delay_S(1);
    Serial1.print(sms_message);
    Serial1.print(ctrl_z);
}

bool process_sms(Config *settings, char *modem_buffer)
{
    // incomming SMS for configuration purposes, examples:
    // #*,server,http://iotnetwork.com.au:5055/,
    // #*,stationary,300,
    // #*,logging,20,
    // #*,upload,300,
    // #*,recovery,1,
    // #*,sms,+51984894723,
    const char sSER[] = "serv";
    const char sSTA[] = "stat";
    const char sLOG[] = "logg";
    const char sUPL[] = "uplo";
    const char sREC[] = "reco";
    const char sSMS[] = "sms";
    char sms_command[40];
    char sms_value[100];
    bool settingFlag = false;
    split_chr(sms_command, modem_buffer, ',', 1);
    split_chr(sms_value, modem_buffer, ',', 2);
    Serial.println("... SMS command");
    Serial.print("... command: ");
    Serial.println(sms_command);
    Serial.print("... value: ");
    Serial.println(sms_value);
    if (memcmp(sSER, sms_command, 4) == 0)
    {
        memcpy(settings->server, sms_value, strlen(sms_value));
        settingFlag = true;
    };
    if (memcmp(sSTA, sms_command, 4) == 0)
    {
        settings->stationary_period = atoi(sms_value);
        settingFlag = true;
    };
    if (memcmp(sLOG, sms_command, 4) == 0)
    {
        settings->logging_period = atoi(sms_value);
        settingFlag = true;
    };
    if (memcmp(sUPL, sms_command, 4) == 0)
    {
        settings->upload_period = atoi(sms_value);
        settingFlag = true;
    };
    if (memcmp(sREC, sms_command, 4) == 0)
    {
        settings->recovery = atoi(sms_value);
        settingFlag = true;
    };
    if (memcmp(sSMS, sms_command, 3) == 0)
    {
        memcpy(phone, sms_value, strlen(sms_value));
        flagSendSMS = true;
    }

    return settingFlag;
}

void print_settings(Config settings)
{
    Serial.print("... server: ");
    Serial.println(settings.server);
    Serial.print("... stationary: ");
    Serial.println(settings.stationary_period);
    Serial.print("... logging: ");
    Serial.println(settings.logging_period);
    Serial.print("... upload: ");
    Serial.println(settings.upload_period);
    Serial.print("... recovery: ");
    Serial.println(settings.recovery);
}

bool send_command(const char *command, int timeout)
{
    Serial1.print("AT+");
    Serial1.print(command);
    Serial1.print("\r");
    return wait_ok(timeout);
}

bool wait_ok(int timeout)
{
    flagOK = false;
    flagERROR = false;
    timeout = timeout * 10;
    int t = 0;
    while (timeout > t)
    {
        t++;
        if (flagOK || flagERROR)
        {
            os_delay_Ms(100);
            return true;
        }
        os_delay_Ms(100);
    }
    modem_power();
    return false;
}

void create_command(const char *server)
{
    // http://iotnetwork.com.au:5055/?id=863922031635619&lat=-13.20416&lon=-72.20898&timestamp=1624031099&hdop=12&altitude=3400&speed=10
    sprintf(
        command_buffer,
        "HTTPPARA=\"URL\",\"%s?id=%s&lat=%s&lon=%s&timestamp=%s%%20%s&hdop=%s&altitude=%s&speed=%s&heading=%s\"",
        server, GSN, latitude, longitude, date, time, hdop, altitude, speed, course);
}

void init_http(void)
{
    send_command("HTTPINIT", 3);
}

void post_http(void)
{
    flagHTTP = false;
    send_command(command_buffer, 5);
    send_command("HTTPACTION=1", 15);
    while (!flagHTTP)
    {
        os_delay_Ms(100);
    }
}

void stop_http(void)
{
    send_command("HTTPTERM", 3);
}

bool sms_config(void)
{
    bool result = send_command("CSMS=0", 5) &&
                  send_command("CPMS=\"ME\",\"ME\",\"ME\"", 5) &&
                  send_command("CMGF=1", 5) &&
                  send_command("CNMI=2,2", 5);
    return result;
}

int save_on_memory(int memory_counter, char *command_buffer)
{
    int len = strlen(command_buffer);
    for (int z = 0; z < len; z++)
    {
        memory[memory_counter][z] = command_buffer[z];
    }
    memory[memory_counter][len] = 0;
    memory_counter++;
    if (memory_counter >= memory_buffer)
    {
        flagUpload = true;
    }
    return memory_counter;
}

void get_location(Config settings)
{
    send_command("CGNSSINFO", 10);
    if (flagGNS)
    {
        create_command(settings.server);
        memory_counter = save_on_memory(memory_counter, command_buffer);
        flagGNS = false;
    }
}

void upload_location(void)
{
    if (spd < 1)
        turn_rf_on();
    init_http();
    for (int i = 0; i < memory_counter; i++)
    {
        int z = 0;
        while (z < modem_size)
        {
            char c = memory[i][z];
            if (c == 0)
            {
                break;
            }
            else
            {
                command_buffer[z] = c;
            }
            z++;
        }
        command_buffer[z] = 0;
        post_http();
    }
    memory_counter = 0;
    stop_http();
    if (spd < 1)
        turn_rf_off();
}
/******************************************************************
*********                       EOF                       *********
*******************************************************************
********* Dev. by Sanchitha Dias (sanchithadias@gmail.com)*********
******************************************************************/