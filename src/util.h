#ifndef UTIL_H
#define UTIL_H

#include <FlashStorage.h>

typedef struct
{
    char server[100];
    uint16_t noMotionPeriod;
    uint16_t inMotionPeriod;
    uint16_t noMotionUploadPeriod;    
    uint16_t inMotionUploadPeriod;
    uint16_t recoveryPeriod;
    bool recovery;
    bool valid;
} Config;

bool init_leds(void);
void blinky(int repeats, int time);

bool init_rtc(void);
void set_rtc();
void set_rtc(const char* dateToday, const char* timeNow);
void set_alarm();
void rtc_sleep();
void alarmMatch();
void wake_device();
bool get_status_log_sensor_data(void);
void get_epoch_time_bytes(uint8_t *dataBuf);

void check_config();
void get_settings(Config *ptrSettings);
void store_settings(void);
void print_settings(void);

void init_debug_serial(void);
void debug_print(String buf);
void debug_print(char c);
void debug_print(int i);
void debug_print(float f);
void debug_print(float f, uint8_t decimals);
void debug_println(String buf);
void debug_println(char c);
void debug_println(int i);
void debug_println(float f);
void debug_println(float f, uint8_t decimals);
void close_debug_serial(void);

void os_delay_Us(uint16_t us);
void os_delay_Ms(uint16_t ms);
void os_delay_S(uint16_t s);

int find_chr(const char *text, const int start, const char chr);
void find_edges(const char *text, int order, const char chr);
void split_chr(char *destination, const char *source, const char chr, const int part);
float to_geo(char *coordinate, char *indicator);
void to_date(char *date);
void to_time(char *time);

#endif 
