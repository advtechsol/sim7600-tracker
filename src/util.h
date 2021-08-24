#ifndef UTIL_H
#define UTIL_H

typedef struct
{
    char server[100];
    int stationary_period;
    int logging_period;
    int upload_period;
    bool recovery;
    bool valid;
} Config;

bool init_leds(void);
void blinky(int repeats, int time);
bool init_rtc(void);
void set_rtc();
void set_alarm();
void rtc_sleep();
void alarmMatch();
void wake_device();
bool get_status_log_sensor_data(void);
void get_epoch_time_bytes(uint8_t *dataBuf);
void os_delay_Us(int us);
void os_delay_Ms(int ms);
void os_delay_S(int s);
int find_chr(const char *text, const int start, const char chr);
void find_edges(const char *text, int order, const char chr);
void split_chr(char *destination, const char *source, const char chr, const int part);
float to_geo(char *coordinate, char *indicator);
void to_date(char *date);
void to_time(char *time);

#endif 
