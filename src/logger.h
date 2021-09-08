#ifndef LOGGER_H
#define LOGGER_H

bool init_logger(void);
void writetosd(char *payload);
bool read_from_sd(char *payload, uint32_t *packetCount, uint32_t *readIndexInt);
void data_sent_confirm(uint32_t *readIndexInt);
void sleep_sd(void);
void wake_sd(void);

#endif 
