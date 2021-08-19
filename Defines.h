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
#define		APP_DEVICE_IN_MOTION_INTERVAL_S          5 * 60UL
#define		APP_DEVICE_NO_MOTION_INTERVAL_S          4 * 60 * 60UL

/**
 * @brief LoRaWAN related definitions
 */
#define     DATA_TX_INTERVAL_S                      30

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