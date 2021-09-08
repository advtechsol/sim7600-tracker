/******************************************************************
*********                       Includes                  *********
******************************************************************/
#include <Arduino.h>
#include <SD.h>

#include "..\Board.h"
#include "..\Defines.h"

#include "logger.h"
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
File sensorDataFile; //file to store all the sensor data
File readIndexFile; //file to store data read index file
/******************************************************************
*********                  Local Variables                *********
******************************************************************/

/******************************************************************
*********          Local Function Definitions             *********
******************************************************************/

/******************************************************************
*********              Application Firmware               *********
******************************************************************/
bool init_logger(void)
{
    debug_println("Init SD");
    pinMode(SPI_LORA_RFM95_CS, OUTPUT);
    digitalWrite(SPI_LORA_RFM95_CS, HIGH);

    os_delay_Ms(500);
    // check if the SD card is present and can be initialized:
    if (!SD.begin(SPI_SD_CARD_CS))
    {
        debug_println("card failed");
        os_delay_Ms(500);
        // don't do anything more:
        return false;
    }
    return true;
}

void writetosd(char *payload)
{
    debug_print("Sensor Data: ");
    debug_println(payload);
    if(SD.exists("data.csv"))
    {
        debug_println("data.csv found!");
    }
    else
    {
        debug_println("data.csv does not exist!");
    }

    sensorDataFile = SD.open("data.csv", FILE_WRITE);
    if (sensorDataFile)
    {
        sensorDataFile.println(payload);
        sensorDataFile.close();
    }
    // if the file isn't open, pop up an error:
    else
    {
        debug_println("could not open datalog file");
    }
} // end writetosd()

bool read_from_sd(char *payload, uint32_t *packetCount, uint32_t *readIndexInt)
{
    char readIndex[] = "0000";
    String dataString = "";

    if(SD.exists("readIndex.txt"))
    {
        debug_println("readIndex.txt found!");
        readIndexFile = SD.open("readIndex.txt", FILE_READ);
        if(readIndexFile.size() >= sizeof(readIndex))
        {
            readIndexFile.seek(0);
            *readIndexInt = readIndexFile.readStringUntil('\n').toInt();
            readIndexFile.close();
        }
        else
        {
            readIndexFile.close();
            readIndexFile = SD.open("readIndex.txt", FILE_WRITE);
            readIndexFile.seek(0);
            readIndexFile.println(readIndex);
            readIndexFile.close();
        }
    }
    else
    {
        debug_println("readIndex.txt does not exist!");
        readIndexFile = SD.open("readIndex.txt", FILE_WRITE);
        readIndexFile.seek(0);
        readIndexFile.println(readIndex);
        readIndexFile.close();
    }

    if(!SD.exists("data.csv"))
    {
        return false;
    }

    sensorDataFile = SD.open("data.csv", FILE_READ);

    if(sensorDataFile.size() == *readIndexInt && sensorDataFile.size() > 5*188)
    {
        sensorDataFile.close();
        SD.remove("data.csv");
        return false;
    }

    sensorDataFile.seek(*readIndexInt);
    dataString = sensorDataFile.readStringUntil('\n');
    debug_println(dataString);
    memcpy(payload, dataString.c_str(), dataString.length());
    *readIndexInt = sensorDataFile.position();
    *readIndexInt += 2;
    *packetCount = (sensorDataFile.size()-*readIndexInt)/188;
    sensorDataFile.close();

    return true;
}

void data_sent_confirm(uint32_t *readIndexInt)
{
    char readIndex[] = "0000";
    sprintf(readIndex, "%04d", *readIndexInt);
    readIndexFile = SD.open("readIndex.txt", FILE_WRITE);
    readIndexFile.seek(0);
    readIndexFile.println(readIndex);
    readIndexFile.close();
}

void sleep_sd(void)
{

}

void wake_sd(void)
{

}
/******************************************************************
*********                       EOF                       *********
*******************************************************************
********* Dev. by Sanchitha Dias (sanchithadias@gmail.com)*********
******************************************************************/
