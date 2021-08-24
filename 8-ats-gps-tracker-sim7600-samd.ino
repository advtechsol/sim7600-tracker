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
#include <FlashStorage.h>

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

/******************************************************************
*********                  Local Objects                  *********
******************************************************************/
FlashStorage(storage, Config);
/******************************************************************
*********                  Local Variables                *********
******************************************************************/
extern volatile bool flagOK;
extern volatile bool flagERROR;
volatile bool flagREG = false;
volatile bool flagCheckGPS = false;
extern volatile bool flagHTTP;
volatile bool flagDOWNLOAD = false;
volatile bool flagProcessing = false;
extern volatile bool flagSendSMS;

const int modem_size = 180;      // Size of Modem serila buffer
char modem_buffer[modem_size];   // Modem serial buffer
char modem_char;                 // Modem serial char
volatile int modem_i;            // Modem serial index

volatile bool flagLocate = false;
extern volatile bool flagUpload;

Config settings;
TaskHandle_t Handle_rxTask;
TaskHandle_t Handle_txTask;
/******************************************************************
*********          Local Function Definitions             *********
******************************************************************/

/******************************************************************
*********              Application Firmware               *********
******************************************************************/
void setup()
{
    delay(1000); // keep this to avoid USB crash
    pinMode(GPIO_OUTPUT_STATUS_LED_0, OUTPUT);
    pinMode(GPIO_OUTPUT_MODEM_PWR_EN, OUTPUT);
    digitalWrite(GPIO_OUTPUT_STATUS_LED_0, HIGH);
    digitalWrite(GPIO_OUTPUT_MODEM_PWR_EN, HIGH);
    Serial.begin(230400);
    delay(1000); // keep this to avoid USB crash
    Serial1.begin(115200);
    delay(5000); // delay to start software
    Serial.println("... start!");
    check_config();
    xTaskCreate(task_tx_modem, "txModem", 512, NULL, tskIDLE_PRIORITY + 2, &Handle_txTask);
    xTaskCreate(task_rx_modem, "rxModem", 512, NULL, tskIDLE_PRIORITY + 1, &Handle_rxTask);
    vTaskStartScheduler();

    while (true)
    {
        Serial.println("OS Failed! \n");
        delay(1000);
    }
}

void loop()
{
    int logging_counter = 0;
    int upload_counter = 0;
    int stationary_counter = 0;
    int recovery_counter = 0;
    int i = 0;

    while (true)
    {
        if (settings.recovery)
        {
            if (recovery_counter >= 10)
            {
                recovery_counter = 0;
                flagLocate = true;
                flagUpload = true;
            }
        }
        else
        {
            if (get_speed() >= 1)
            {
                if (logging_counter >= settings.logging_period)
                {
                    logging_counter = 0;
                    flagLocate = true;
                }
            }
            else
            {
                if (stationary_counter >= settings.stationary_period)
                {
                    stationary_counter = 0;
                    flagLocate = true;
                }
            }
            if (upload_counter >= settings.upload_period)
            {
                upload_counter = 0;
                flagUpload = true;
            }
        }

        if (i >= 10)
        {
            i = 0;
            flagCheckGPS = true;
        };

        logging_counter++;
        upload_counter++;
        stationary_counter++;
        recovery_counter++;
        i++;

        delay(1000);
    }
}

static void task_rx_modem(void *pvParameters)
{
    const char mOK[] = "OK";
    const char mERROR[] = "ERRO";
    const char mCLOSED[] = "CLOS";
    const char mCBC[] = "+CBC";
    const char mCOP[] = "+COP";
    const char mCSQ[] = "+CSQ";
    const char mCGR[] = "+CGR";
    const char mIPC[] = "+IPC";
    const char mCGN[] = "+CGN";
    const char mGSN[] = "8639"; // this is SIMCOMS id found on serial number
    const char mRIN[] = "RING"; // when someones makes a call.. it should hang up
    const char mHUP[] = "ATH";  // when someones makes a call.. it should hang up
    const char mHTTP[] = "+HTTP";
    const char mSMS[] = "#*,";

    while (true)
    {
        while (Serial1.available() && !flagProcessing)
        {
            modem_char = Serial1.read();
            Serial.print(modem_char);
            modem_buffer[modem_i] = modem_char;
            modem_i++;
            if (modem_i >= modem_size)
                modem_i = 0;
            if ((modem_i >= 2) && ((modem_char == '\n') || (modem_char == '\n')))
            {
                flagProcessing = true;
                modem_buffer[modem_i] = '\0';
                if (memcmp(mOK, modem_buffer, 2) == 0)
                    flagOK = true;
                if (memcmp(mERROR, modem_buffer, 4) == 0)
                    flagERROR = true;
                if (memcmp(mHTTP, modem_buffer, 4) == 0)
                    flagHTTP = true;
                if (memcmp(mSMS, modem_buffer, 3) == 0)
                {
                    if (process_sms(&settings, modem_buffer))
                    {
                        storage.write(settings);
                        print_settings(settings);
                        Serial.println("... settings saved");
                    }
                }
                if (memcmp(mCGN, modem_buffer, 4) == 0)
                    process_gcn(modem_buffer);
                if (memcmp(mGSN, modem_buffer, 4) == 0)
                    process_gsn(modem_buffer);
                modem_i = 0;
                for (int i = 0; i < modem_size; i++)
                    modem_buffer[i] = 0;
                flagProcessing = false;
            }
        }
        os_delay_Ms(1);
    }
}

static void task_tx_modem(void *pvParameters)
{
    while (true)
    {
        if (sms_config())
        {
            send_command("GSN", 3);
            send_command("CMEE=2", 10);
            while (true)
            {
                if (flagLocate)
                {
                    get_location(settings);
                    flagLocate = false;
                    flagCheckGPS = false;
                }
                if (flagUpload)
                {
                    upload_location();
                    flagUpload = false;
                }
                if (flagCheckGPS)
                {
                    send_command("CGNSSINFO", 10);
                    flagCheckGPS = false;
                }

                if (flagSendSMS)
                {
                    send_sms();
                    flagSendSMS = false;
                }
                os_delay_Ms(100);
            }
        }
        os_delay_S(1);
    }
}

void check_config()
{
    settings = storage.read();
    if (settings.valid)
    {
        Serial.println("... settings found");
    }
    else
    {
        Serial.println("... settings not found");
        const char default_domain[] = "http://iotnetwork.com.au:5055/";
        memcpy(settings.server, default_domain, strlen(default_domain));
        settings.stationary_period = 300;
        settings.logging_period = 30;
        settings.upload_period = 300;
        settings.recovery = false;
        settings.valid = true;
        storage.write(settings);
        Serial.println("... settings saved");
    }
    print_settings(settings);
}
/******************************************************************
*********                       EOF                       *********
******************************************************************/