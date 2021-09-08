#ifndef MODEM_H
#define MODEM_H

#include "util.h"

typedef enum{
    state_no_response = -1,
    state_modem_ok,
    state_modem_error,
    state_modem_http,
    state_modem_start,
    state_modem_gsn,
    state_modem_cgn,
    state_modem_sms
}ModemResponseState_t;

bool init_modem(void);
bool modem_turn_pwr(bool on_off_state);
void process_gsn(char *modem_buffer);
void process_gcn(char *modem_buffer);
float get_speed(void);
void send_sms(void);
bool process_sms(char *modem_buffer);
ModemResponseState_t send_command(const char *command, uint16_t timeout);
bool sms_config(void);
void get_location(void);
bool upload_location(void);
bool modem_stop(void);

#endif 