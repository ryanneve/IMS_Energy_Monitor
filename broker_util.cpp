// 
// Utilities for Arduino based broker. Would like to move most functions from main ino here and make this a class.
// 

#include "broker_util.h"
#include "broker_data.h"


void printResultStr() {
	Serial.print(F("{\"result\":{"));

}

uint16_t addMsgTime(char *stat_buff, uint16_t  d_idx,const char * tz) {
	char msgTime[15];
	getSampleTimeStr(msgTime);
	d_idx += sprintf(stat_buff + d_idx, ",\"message_time\":{\"value\":%s,\"units\":\"%s\"}", msgTime, tz);
	return d_idx;
}

uint16_t addMsgId(char *stat_buff, uint16_t  d_idx, const int16_t json_id) {
	d_idx += sprintf(stat_buff + d_idx, "},\"id\":%u}", json_id);
	return d_idx;
}


uint32_t freeRam() {
#ifdef __AVR__
	//arduino
	extern int __heap_start, *__brkval;
	int v;
	return (uint32_t)&v - (__brkval == 0 ? (uint32_t)&__heap_start : (uint32_t)__brkval);
#elif defined(__arm__)
	// for Teensy 3.0
	uint32_t stackTop;
	uint32_t heapTop;

	// current position of the stack.
	stackTop = (uint32_t)&stackTop;

	// current position of heap.
	void* hTop = malloc(1);
	heapTop = (uint32_t)hTop;
	free(hTop);

	// The difference is (approximately) the free, available ram.
	return stackTop - heapTop;
#else
	return 0;
#endif
}

void printFreeRam(const char * msg) {
	//Serial.print(F("Free RAM ("));Serial.print(msg);Serial.print("):");Serial.println(freeRam());
}


uint8_t checkSubscriptions(bool datamap[], BrokerData *broker_objs[], const uint8_t broker_obj_count) {
	uint8_t subs = 0;
	for (uint8_t obj_no = 0; obj_no < broker_obj_count; obj_no++) {
		if (broker_objs[obj_no]->subscriptionDue()) {
			datamap[obj_no] = true;
			subs++;
		}
		else datamap[obj_no] = false;
	}
	return subs;
}

void processSubscriptions(const bool datamap[], BrokerData *broker_objs[], const uint8_t broker_obj_count,const char tz[]) {
	/* Based on settings in data_map, generates a subscrition message
	Currently uses aJson to generate message, but this may be un-necessary
	*/
	Serial.print("{\"method\":\"subscription\",");
	Serial.print("\"params\":{");
	char statusBuffer[PARAM_BUFFER_SIZE]; // Should be plenty big to hold output for one parameter
	uint8_t dataIdx = 0;
	bool first = true;
	for (uint8_t obj_no = 0; obj_no < broker_obj_count; obj_no++) {
		if (datamap[obj_no] == true) {
			broker_objs[obj_no]->getData(); // Update values
			char dataStr[20];	// HOLDS A STRING REPRESENTING A SINGLE VALUE
			if (!first) dataIdx += sprintf(statusBuffer + dataIdx, ",");
			else first = false;
			dataIdx += sprintf(statusBuffer + dataIdx, "\"%s\":{", broker_objs[obj_no]->getName());
			broker_objs[obj_no]->dataToStr(dataStr);
			dataIdx += sprintf(statusBuffer + dataIdx, "\"value\":%s", dataStr);
			if (broker_objs[obj_no]->isVerbose()) {
				dataIdx += sprintf(statusBuffer + dataIdx, ",\"units\":\"%s\"", broker_objs[obj_no]->getUnit());
				// Only report min and max if they exist
				double min_d = broker_objs[obj_no]->getMin();
				double max_d = broker_objs[obj_no]->getMax();
				if (min_d == min_d) dataIdx += sprintf(statusBuffer + dataIdx, ",\"min\":\"%f\"", min_d);
				if (max_d == max_d) dataIdx += sprintf(statusBuffer + dataIdx, ",\"max\":\"%f\"", max_d);
				dataIdx += sprintf(statusBuffer + dataIdx, ",\"sample_time\":\"%s\"", broker_objs[obj_no]->getSplTimeStr());
			}
			dataIdx += sprintf(statusBuffer + dataIdx, "}"); // Close out this parameter
			Serial.print(statusBuffer);
			dataIdx = 0;
		}
	}
	dataIdx = addMsgTime(statusBuffer, 0, tz);
	dataIdx += sprintf(statusBuffer + dataIdx, "}}"); // Close out params and message
	Serial.println(statusBuffer);
	//printFreeRam("pSub end");
}