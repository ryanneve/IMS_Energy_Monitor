// broker_util.h

#ifndef _BROKER_UTIL_h
#define _BROKER_UTIL_h

#include <Arduino.h> 

void	printResultStr();
uint16_t	addMsgTime(char *stat_buff, uint16_t  d_idx, const char * tz);
uint16_t	addMsgId(char *stat_buff, uint16_t  d_idx, const int16_t json_id);

void		printFreeRam(const char * msg);
uint32_t	freeRam();

#endif

