// broker_util.h

#ifndef _BROKER_UTIL_h
#define _BROKER_UTIL_h

#include <Arduino.h> 
#include "broker_data.h"

#define PARAM_BUFFER_SIZE  200 // should be >~100


void	printResultStr();
uint16_t	addMsgTime(char *stat_buff, uint16_t  d_idx, const char * tz);
uint16_t	addMsgId(char *stat_buff, uint16_t  d_idx, const int16_t json_id);

void		printFreeRam(const char * msg);
uint32_t	freeRam();


uint8_t checkSubscriptions(bool datamap[], BrokerData *broker_objs[], const uint8_t broker_obj_count);
void processSubscriptions(const bool datamap[], BrokerData *broker_objs[], const uint8_t broker_obj_count,const char tz[]);

#endif

