#ifndef MODEM_H
#define MODEM_H

#include "util.h"

bool init_radio(void);
void process_gsn(char *modem_buffer);
void process_gcn(char *modem_buffer);
float get_speed(void);
void send_sms(void);
bool process_sms(Config *settings, char *modem_buffer);
void print_settings(Config settings);
bool send_command(const char *command, int timeout);
bool sms_config(void);
void get_location(Config settings);
void upload_location(void);

#endif 