/******************************************************************
*********                       Includes                  *********
******************************************************************/
#include <Arduino.h>

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

/******************************************************************
*********          Local Function Definitions             *********
******************************************************************/
bool init_modem_gpio(void);
bool modem_reset_state(bool reset_state);
bool modem_turn_pwr(bool on_off_state);
bool modem_reset(void);
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
/******************************************************************
*********                       EOF                       *********
*******************************************************************
********* Dev. by Sanchitha Dias (sanchithadias@gmail.com)*********
******************************************************************/