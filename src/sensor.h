#ifndef SENSOR_H
#define SENSOR_H

bool init_gps(void);
bool gps_turn_pwr(bool on_off_state);
bool readGPS(void);
bool gps_stop(void);
float get_latitude(void);
float get_longitude(void);
float get_altitude(void);
bool init_tilt_sensor(void);
void tilt_sensor_isr();
bool check_movement_timeout(void);
bool log_sensor_data(void);
float read_batV_buf(uint8_t *dataBuf);

#endif 
