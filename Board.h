/******************************************************************
 **  Contains all the HW IO configuration definitions of 
 **	 the GPS Tracker Device device.
******************************************************************/
#ifndef BOARD_H
#define BOARD_H

/**
 * @brief Power sections controls
 * */
#define GPIO_OUTPUT_MODEM_PWR_EN        11//8
#define ADC_BATTERY_VOLTAGE             A7

/**
 * @brief RFM95 Pin Connections
 * */
#define SPI_LORA_RFM95_CS               8
#define GPIO_OUTPUT_LORA_RFM95_RST      4
#define GPIO_INTERRUPT_LORA_RFM95_DIO0  3
#define GPIO_INTERRUPT_LORA_RFM95_DIO1  6

/**
 * @brief Keypad Pin Connections
 * */
#define ONEWIRE_INPUT_KEYPAD            A0

/**
 * @brief Tilt Sensor
 * */
#define GPIO_INTERRUPT_TILT_SENSOR      12//A5

/**
 * @brief SD Card Pin Connections
 * */
#define SPI_SD_CARD_CS                  5

/**
 * @brief SIMCom cellular modem SIM7500/SIM7600 Pin Connections
 * */
#define UART_1_MODEM_SIM7600_TX         1
#define UART_1_MODEM_SIM7600_RX         0  

/**
 * @brief Status outputs
 * */
#define GPIO_OUTPUT_STATUS_LED_0        13
#define GPIO_OUTPUT_STATUS_LED_1        10
#define GPIO_OUTPUT_STATUS_LED_2        12

/**
 * @brief Builtin LED for Debug purposes
 **/
#ifndef LED_BUILTIN
#define LED_BUILTIN 	               13
#endif

#endif
/******************************************************************
*********                       EOF                       *********
*******************************************************************
********* Dev. by Sanchitha Dias (sanchithadias@gmail.com)*********
******************************************************************/
