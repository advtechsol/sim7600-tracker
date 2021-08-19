#ifndef UTIL_H
#define UTIL_H

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

#endif 
