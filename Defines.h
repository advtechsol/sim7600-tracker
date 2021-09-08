/******************************************************************
 **  Contains all the runtime configuration definitions of 
 **	 the GPS Tracker device.
******************************************************************/
#ifndef	DEFINES_H
#define DEFINES_H

/**
 * @brief Firware debugging through serial
 */
#define		APP_MODE_DEBUG		                    true

/**
 * @brief Operating mode intervals
 */
#define		APP_DEVICE_IN_MOTION_INTERVAL_S          30
#define		APP_DEVICE_NO_MOTION_INTERVAL_S          2 * 60 * 60
#define		APP_DEVICE_IN_MOTION_UPLOAD_INTERVAL_S   5 * 60
#define		APP_DEVICE_NO_MOTION_UPLOAD_INTERVAL_S   6 * 60 * 60
#define		APP_DEVICE_RECOVERY_INTERVAL_S           10

/**
 * @brief LoRaWAN related definitions
 */
#define     DATA_TX_INTERVAL_S                      30

/**
 * @brief Voltage lower threshold
 */
#define     BATTERY_LOW_VOLTAGE_THRESHOLD_MV        3000 // Minimum voltage for MPM3610 is 3 Volts

/**
 * @brief LED Blink interval
 */
#define     BLINK_TIME_INTERVAL                     50

#endif
/******************************************************************
*********                       EOF                       *********
*******************************************************************
********* Dev. by Sanchitha Dias (sanchithadias@gmail.com)*********
******************************************************************/