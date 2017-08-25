/* Energy Monitor

Reads current values from ADS1015
Channel, function
0, Load current
1, Charge Current
2, Battery Voltage (through divider)
3, Vcc (nominal 5v)

Commuincates via JSON-RPC

Data Values:
Voltage
Vcc
Current_Load
Current_Charge
Power_Load
Power_Charge
Energy_Load
Energy_Charge
Assumes all values are "subscribed" so sends this out periodically:

Accepts the following inputs:
{"method:"set","params":{"Energy_load":<integer>}}
{"method:"set","params":{"Energy_charge":<integer>}}
where integer will usually be 0 and set at the start of every day.
{"method:"status","params":{"data":[<list of Data Values]}}
{"method":list_data,"params",{}} - Returns list of data values with units and type (RO or RW)
{"method":"initialize","params":{}} - Resets all counters and min/max values. TYpically called once per day.

Expects, but ignores the following methods:
"subscribe" - We assume everything is subscribed
"unsubscribe"
"tokenAcquire", "tokenForceAcquire", "tokenRelease", "tokenOwner" - Only one data client, no tokens used at this level.
"power" - doesn't really apply
"suspend", "resume", - is there any point?
"restart", "shutdown" - may not apply in this environment
"broker_status" - no point

TO DO
	- Make resistor values setable and store them in EEPROM.
*/

#include "ADS1115.h"
// include the aJSON library
#include <aJSON.h>
// include the JsonRPC library
//#include <JsonRPCServer.h>
// this includes all functions relating to specific JSON messahe parsing and generation.
//#include "Energy_JSON.h"
// data objects
#include "broker_data.h"


#define ACS715_mV_per_A 133.0
#define V_DIV_LOW	 10000.0
#define V_DIV_HIGH  21000.0
#define ADC_CHANNEL_LOAD_CURRENT	0
#define ADC_CHANNEL_CHARGE_CURRENT	1
#define ADC_CHANNEL_VOLTAGE	2
#define ADC_CHANNEL_VCC	3
#define BROKERDATA_OBJECTS 8
#define BROKER_MIN_UPDATE_RATE_MS 500
#define PARAM_BUFFER_SIZE  200 // should be >~100

// define constants
const uint8_t ADS1115_ADDRESS = 0x48;
const uint16_t LOOP_DELAY_TIME_MS = 2000;	// Time in ms to wait between sampling loops.

// Define Objects
ADS1115 ads(ADS1115_ADDRESS);

VoltageData v_batt("Voltage", ads, ADC_CHANNEL_VOLTAGE, V_DIV_LOW, V_DIV_HIGH);
VoltageData v_cc("Vcc", ads, ADC_CHANNEL_VCC, 0, 1);	// No voltage divider
CurrentData	current_l("Load_Current", ads, v_cc, ADC_CHANNEL_LOAD_CURRENT, ACS715_mV_per_A);
CurrentData	current_c("Charge_Current", ads, v_cc, ADC_CHANNEL_CHARGE_CURRENT, ACS715_mV_per_A);
PowerData	power_l("Load_Power",current_l,v_batt);
PowerData	power_c("Charge_Power",current_c,v_batt);
EnergyData	energy_l("Load_Energy", power_l);
EnergyData	energy_c("Charge_Energy",power_c);

BrokerData *brokerobjs[BROKERDATA_OBJECTS];

//TargetController jsonController(&Serial);

aJsonStream serial_stream(&Serial);


// Global variables

int16_t	json_id = 0;
bool status_verbose = true; // true is default.
bool data_map[BROKERDATA_OBJECTS]; // Used to mark broker objects we are interested in.


const uint8_t JSON_REQUEST_COUNT = 6; // How many different request types are there.
enum json_r_t {
	BROKER_STATUS = 0,
	BROKER_SUBSCRIBE = 1,
	BROKER_UNSUBSCRIBE = 2,
	BROKER_SET = 3,
	BROKER_LIST_DATA = 4,
	BROKER_ERROR = 5
};
const char *REQUEST_STRINGS[JSON_REQUEST_COUNT] = { "status","subscribe","unsubscribe","set","list_data","" };

void setup() {
    Wire.begin();
	Serial.begin(57600);	//USB
	//Serial1.begin(57600);	// PINS
	while (!Serial);	// Leonardo seems to need this
	//while (!Serial1);	// Leonardo seems to need this
	Serial.println(F("IMS Power Monitor"));
	delay(3000);
	Serial.println(ads.testConnection() ? F("ADS1115 connected") : F("ADS1115  failed"));
	ads.initialize();
	// We're going to do single shot sampling
	ads.setMode(ADS1115_MODE_SINGLESHOT);
	// Slow things down so that we can see that the "poll for conversion" code works
	ads.setRate(ADS1115_RATE_8);
	// Set the gain (PGA) +/- 6.144v
	// Note that any analog input must be higher than �0.3V and less than VDD +0.3
	ads.setGain(ADS1115_PGA_6P144);

	brokerobjs[0] = &v_batt;
	brokerobjs[1] = &v_cc;
	brokerobjs[2] = &current_l;
	brokerobjs[3] = &current_c;
	brokerobjs[4] = &power_l;
	brokerobjs[5] = &power_c;
	brokerobjs[6] = &energy_l;
	brokerobjs[7] = &energy_c;
	//BEGIN DEBUG
	//v_batt.subscribe(4000);
	//v_cc.subscribe(15000);
	// END DEBUG

}

void loop()
{
	printFreeRam("loop0");
	// Process incoming messages
	processSerial();
	// CHECK IF WE GOT A RESET MESSAGE
	//DEBUG START
	// Get data
	/*
	char _buf[10];
	double adc= v_cc.getADCreading();
	Serial.print("Channel: "); Serial.print(v_cc.getChannel());
	dtostrf(adc, 7, 4, _buf);
	Serial.print("  ADC Vcc "); Serial.println(_buf);
	adc = v_batt.getADCreading();
	Serial.print("Channel: "); Serial.print(v_batt.getChannel());
	dtostrf(adc, 7, 4, _buf);
	Serial.print("  ADC Vbatt "); Serial.println(_buf);
	//DEBUG END
	*/
	// retreive new data
	v_cc.getData();
	v_batt.getData();
	current_l.getData();
	current_c.getData();
	power_l.getData();
	power_c.getData();
	energy_l.getData();
	energy_l.getData();
	// See what subscriptions are up
	if (checkSubscriptions() > 0) processSubscriptions();
	delay(LOOP_DELAY_TIME_MS); // MAY BE WORTH LOOKING IN TO LOWERING POWER CONSUMPTION HERE
}

void processSubscriptions() {
	//printFreeRam("pSub start");
	// This should be a function .
	uint8_t sub_idx = 0;
	aJsonObject *json_root, *json_params;
	aJsonObject *jsondataobjs[BROKERDATA_OBJECTS]; // Holds objects used for data output
	json_root = aJson.createObject();
	aJson.addItemToObject(json_root, "method", aJson.createItem("subscription"));
	aJson.addItemToObject(json_root, "params", json_params = aJson.createObject());
	for (uint8_t obj_no = 0; obj_no < BROKERDATA_OBJECTS; obj_no++) {
		if (data_map[obj_no] == true) {
			aJson.addItemToObject(json_params, brokerobjs[obj_no]->getName(), jsondataobjs[sub_idx] = aJson.createObject());
			aJson.addNumberToObject(jsondataobjs[sub_idx], "value", brokerobjs[obj_no]->getValue());
			if (brokerobjs[obj_no]->isVerbose()) {
				aJson.addStringToObject(jsondataobjs[sub_idx], "units", brokerobjs[obj_no]->getUnit());
				aJson.addNumberToObject(jsondataobjs[sub_idx], "sample_time", (unsigned long)brokerobjs[obj_no]->getSampleTime()); // returns time in millis() since start which is wrong...
			}
			brokerobjs[obj_no]->setSubscriptionTime(); // Resets subscription time
			sub_idx++;
		}
	}
	aJson.addItemToObject(json_root, "message_time", aJson.createItem((unsigned long)millis()));
	//Serial.print(F("JSON string:"));
	char * aJsonPtr = aJson.print(json_root);
	Serial.println(aJsonPtr); // Prints out subscription message
	free(aJsonPtr); // So we don't have a memory leak
	aJson.deleteItem(json_root);
	//printFreeRam("pSub end");
}
uint8_t checkSubscriptions() {
	//printFreeRam("cS start");
	uint8_t subs = 0;
	for (uint8_t obj_no = 0; obj_no < BROKERDATA_OBJECTS; obj_no++) {
		if (::brokerobjs[obj_no]->subscriptionDue()) {
			::data_map[obj_no] = true;
			subs++;
			//Serial.print("S-");
		}
		else ::data_map[obj_no] = false;
	}
	return subs;
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
	/*Serial.print(F("Free RAM ("));
	Serial.print(msg);
	Serial.print("):");
	Serial.println(freeRam());
	*/
}

uint16_t getMessageType(aJsonObject** json_in_msg) {
	// Extract method and ID from message.
	//printFreeRam("gMT start");
	uint8_t json_method = BROKER_ERROR;

	aJsonObject *jsonrpc_method = aJson.getObjectItem(*json_in_msg, "method");
	for (uint8_t j_method = 0; j_method < JSON_REQUEST_COUNT; j_method++) {
		if (!strcmp(jsonrpc_method->valuestring, REQUEST_STRINGS[j_method])) {
			//Serial.print(F("JSON request: "));
			//Serial.println(REQUEST_STRINGS[j_method]);
			json_method = j_method;
			break;
		}
	}
	// Get ID here
	aJsonObject *jsonrpc_id = aJson.getObjectItem(*json_in_msg, "id");
	::json_id = jsonrpc_id->valueint;

	//printFreeRam("gMT end3");
	return json_method;
}

uint8_t processBrokerStatus(aJsonObject *json_in_msg) {
	//printFreeRam("pBS start");
	uint8_t status_matches_found = 0;
	// First process style
	aJsonObject *jsonrpc_params = aJson.getObjectItem(json_in_msg, "params");
	// Extract Style
	aJsonObject *jsonrpc_style = aJson.getObjectItem(jsonrpc_params, "style");
	if (!strcmp(jsonrpc_style->valuestring, "terse")) status_verbose = true;
	else status_verbose = false;
	

	clearDataMap(); // Sets all data_map array values to false

	// Now Extract data list
	aJsonObject *jsonrpc_data = aJson.getObjectItem(jsonrpc_params, "data");
	// data will be list of parameters: ["Voltage","Vcc",Current_Load"]
	// Now parse data list and set data_map array values
	aJsonObject *jsonrpc_data_item = jsonrpc_data->child;
	//Serial.print(jsonrpc_data_item->valuestring);
	//printFreeRam("pBS data 1");
	while (jsonrpc_data_item) { 
		for (uint8_t broker_data_idx = 0; broker_data_idx < BROKERDATA_OBJECTS; broker_data_idx++) {
			//Serial.print("Comparing "); Serial.print(jsonrpc_data_item->valuestring); Serial.print(" to "); Serial.println(brokerobjs[broker_data_idx]->getName());
			if (!strcmp(jsonrpc_data_item->valuestring, brokerobjs[broker_data_idx]->getName())) {
				//Serial.print(F("B data: ")); Serial.println(jsonrpc_data_item->valuestring);
				data_map[broker_data_idx] = true; 
				//Serial.print(broker_data_idx); Serial.print("="); Serial.println(::data_map[broker_data_idx]);
				status_matches_found++;
				break; // break out of for loop
			}
		}
		jsonrpc_data_item = jsonrpc_data_item->next; // Set pointer for jsonrpc_data_item to next item.
	}
	return status_matches_found;
}

void generateStatusMessage() {
	//printFreeRam("gSM start");
	const uint8_t STATVALWIDTH = 5;
	const uint8_t STATVALPREC = 3;
	uint16_t dataIdx = 0; // should never exceed PARAM_BUFFER_SIZE
	// based on contents of datamap array, generate status message.
	/*
	{
    "result" : {
        "Voltage" : {
            "value" : 13.5,
            "units" : "V",
            "sample_time":20110801135647605},
        "Vcc" : {
            "value" : 4.85,
            "units" : "V",
            "sample_time":20110801135647605}}
    "id" : 1
	}*/
	bool first = true;
	Serial.print(F("\"result\":{"));
	char statusBuffer[PARAM_BUFFER_SIZE]; // Should be plenty big to hold output for one parameter

	for (uint8_t obj_no = 0; obj_no < BROKERDATA_OBJECTS; obj_no++) {
		if (::data_map[obj_no] == true) {
			char statusValue[10] = "-999"; // Holds status double value as a string
			dtostrf((double)brokerobjs[obj_no]->getValue(), STATVALWIDTH, STATVALPREC, statusValue); // convert (double) statusValue to string
			if (!first) dataIdx += sprintf(statusBuffer + dataIdx, ","); // preceding comma
			dataIdx += sprintf(statusBuffer + dataIdx, "\"%s\":{", brokerobjs[obj_no]->getName());
			dataIdx += sprintf(statusBuffer + dataIdx, "\"value\":%s,", statusValue);
			dataIdx += sprintf(statusBuffer + dataIdx, "\"units\":\"%s\"}", brokerobjs[obj_no]->getUnit());
			Serial.print(statusBuffer);
			dataIdx = 0; // reset for next Parameter.
			first = false;
		}
	}
	dataIdx = 0;
	dataIdx += sprintf(statusBuffer + dataIdx, "},\"id\":%u}", json_id);
	Serial.println(statusBuffer);
	//printFreeRam("gSM end");
}




uint8_t processBrokerUnubscribe(aJsonObject *json_in_msg) {
	/*Processes an un-subscribe message
	For example:
	{"method" : "unsubscribe",
	"params" : {"data":["Voltage", "Vcc"]}
	"id" : 14}
	*/

	uint8_t unsubscribe_matches_found = 0;
	aJsonObject *jsonrpc_params = aJson.getObjectItem(json_in_msg, "params");
	// Now Extract data list
	aJsonObject *jsonrpc_data = aJson.getObjectItem(jsonrpc_params, "data");

	// Start output
	char statusBuffer[PARAM_BUFFER_SIZE]; // Should be plenty big to hold output for one parameter
	uint16_t dataIdx = 0; // should never exceed PARAM_BUFFER_SIZE
	Serial.print(F("{\"result\":{"));

	// Now parse data list
	if (jsonrpc_data) {
		aJsonObject *jsonrpc_data_item = jsonrpc_data->child;
		//Serial.print(jsonrpc_data_item->valuestring);
		//printFreeRam("pBS data 1");
		while (jsonrpc_data_item) {
			bool found = false;
			for (uint8_t broker_data_idx = 0; broker_data_idx < BROKERDATA_OBJECTS; broker_data_idx++) {
				dataIdx = 0;
				dataIdx += sprintf(statusBuffer + dataIdx, "\"%s\":{", jsonrpc_data_item->valuestring); // even if it's bad data
				if (!strcmp(jsonrpc_data_item->valuestring, brokerobjs[broker_data_idx]->getName())) {
					unsubscribe_matches_found++;
					// Set subscription up
					brokerobjs[broker_data_idx]->unsubscribe();
					found = true;
					dataIdx += sprintf(statusBuffer + dataIdx, "\"status\":\"ok\"},");
					break; // break out of for loop
				}
			}
			if (found == false) dataIdx += sprintf(statusBuffer + dataIdx, "\"status\":\"error\"},"); // There should be more to this, but that's all for now.
			Serial.print(statusBuffer);
			jsonrpc_data_item = jsonrpc_data_item->next; // Set pointer for jsonrpc_data_item to next item.
		}
	}
	// Now finish output
	// Should add update rates....
	dataIdx = 0;
	dataIdx += sprintf(statusBuffer + dataIdx, "},\"id\":%u}", json_id);
	Serial.println(statusBuffer);
	return unsubscribe_matches_found;
}

uint8_t processBrokerSubscribe(aJsonObject *json_in_msg) {
	/*Processes a subscribe message
	For example:
	{"method" : "subscribe",
	 "params" : {
		"data":["Voltage", "Vcc"],
		"style":"terse",
		"updates":"on_change",
		"min_update_rate":1000}
	"id" : 14}
	*/
	printFreeRam("pBSub start");
	uint8_t subscribe_matches_found = 0;
	bool subscribe_verbose = true;
	bool subscribe_on_change = true;
	uint32_t subscribe_min_update_ms = __LONG_MAX__;
	uint32_t subscribe_max_update_ms = __LONG_MAX__;
	// First process style
	aJsonObject *jsonrpc_params = aJson.getObjectItem(json_in_msg, "params");

	//char *aJsonPtr; // For debug prints
	//Serial.print("jsonrpc_params: ");	aJsonPtr = aJson.print(jsonrpc_params);	Serial.println(aJsonPtr); free(aJsonPtr); // So we don't have a memory leak

	// Extract Optional Style
	aJsonObject *jsonrpc_style = aJson.getObjectItem(jsonrpc_params, "style");
	//Serial.print("jsonrpc_style: "); aJsonPtr = aJson.print(jsonrpc_style);	Serial.println(aJsonPtr); free(aJsonPtr); // So we don't have a memory leak
	if (jsonrpc_style) {
		if (!strcmp(jsonrpc_style->valuestring, "terse")) subscribe_verbose = true;
		else subscribe_verbose = false;
	}

	// Extract Optional Updates
	aJsonObject *jsonrpc_updates = aJson.getObjectItem(jsonrpc_params, "updates");
	if (jsonrpc_updates) {
		if (!strcmp(jsonrpc_updates->valuestring, "on_new")) subscribe_verbose = true;
		else subscribe_verbose = false;
	}

	// Extract Optional Min Update Rate
	aJsonObject *jsonrpc_min_rate = aJson.getObjectItem(jsonrpc_params, "min_update_ms");
	//Serial.print("jsonrpc_min_rate: ");	aJsonPtr = aJson.print(jsonrpc_min_rate);	Serial.println(aJsonPtr); free(aJsonPtr); // So we don't have a memory leak
	if (jsonrpc_min_rate) 	subscribe_min_update_ms = (uint32_t)jsonrpc_min_rate->valueint; 

	// Extract Optional Max Update Rate
	aJsonObject *jsonrpc_max_rate = aJson.getObjectItem(jsonrpc_params, "max_update_ms");
	//Serial.print("jsonrpc_max_rate: "); aJsonPtr = aJson.print(jsonrpc_max_rate);	Serial.println(aJsonPtr); free(aJsonPtr); // So we don't have a memory leak
	if (jsonrpc_max_rate) subscribe_max_update_ms = (uint32_t)jsonrpc_max_rate->valueint;

	// Now some calculations based on https://sites.google.com/site/verticalprofilerupgrade/home/ControllerSoftware/ipc-specification
	//
	if ( subscribe_min_update_ms == __LONG_MAX__ && subscribe_max_update_ms == __LONG_MAX__) {
		// Neither provided, set based on spec
		subscribe_min_update_ms = max(subscribe_min_update_ms, (uint32_t)BROKER_MIN_UPDATE_RATE_MS);
		subscribe_max_update_ms = subscribe_min_update_ms * 4; 
	}
	else {
		// at least one parameter provided
		if (subscribe_max_update_ms == __LONG_MAX__) {
			// Only minimum provided
			subscribe_min_update_ms = max(subscribe_min_update_ms, (uint32_t)BROKER_MIN_UPDATE_RATE_MS); // make minimum is big enough
			subscribe_max_update_ms = subscribe_min_update_ms * 4; // set Max, This is the spec
		}
		else if (subscribe_min_update_ms == __LONG_MAX__) {
			// Only maximum provided
			subscribe_max_update_ms = max(subscribe_max_update_ms, (uint32_t)BROKER_MIN_UPDATE_RATE_MS); // make minimum is big enough
			subscribe_min_update_ms = max(subscribe_max_update_ms / 4, (uint32_t)BROKER_MIN_UPDATE_RATE_MS); // set Min, This is the spec
		}
		else {
			// both provided
			// can't be less than broker minimum
			subscribe_min_update_ms = max(subscribe_min_update_ms, (uint32_t)BROKER_MIN_UPDATE_RATE_MS);
			subscribe_max_update_ms = max(subscribe_max_update_ms, subscribe_min_update_ms); // Max should be at least as big as min
		}
	}

	// Now Extract data list
	aJsonObject *jsonrpc_data = aJson.getObjectItem(jsonrpc_params, "data");


	//Serial.print("jsonrpc_data: ");	aJsonPtr = aJson.print(jsonrpc_data);	Serial.println(aJsonPtr); free(aJsonPtr); // So we don't have a memory leak


	// Start output
	char statusBuffer[PARAM_BUFFER_SIZE]; // Should be plenty big to hold output for one parameter
	uint16_t dataIdx = 0; // should never exceed PARAM_BUFFER_SIZE
	Serial.print(F("{\"result\":{"));
	// data will be list of parameters: ["Voltage","Vcc",Current_Load"]
	// Now parse data list
	if (jsonrpc_data) {
		aJsonObject *jsonrpc_data_item = jsonrpc_data->child;
		//Serial.print(jsonrpc_data_item->valuestring);
		//printFreeRam("pBS data 1");
		while (jsonrpc_data_item) {
			bool found = false;
			for (uint8_t broker_data_idx = 0; broker_data_idx < BROKERDATA_OBJECTS; broker_data_idx++) {
				dataIdx = 0;
				dataIdx += sprintf(statusBuffer + dataIdx, "\"%s\":{", jsonrpc_data_item->valuestring); // even if it's bad data
				if (!strcmp(jsonrpc_data_item->valuestring, brokerobjs[broker_data_idx]->getName())) {
					subscribe_matches_found++;
					// Set subscription up
					brokerobjs[broker_data_idx]->subscribe(subscribe_min_update_ms, subscribe_min_update_ms);
					brokerobjs[broker_data_idx]->setSubOnChange(subscribe_on_change);
					brokerobjs[broker_data_idx]->setVerbose(subscribe_verbose);
					found = true;
					dataIdx += sprintf(statusBuffer + dataIdx, "\"status\":\"ok\"},");
					break; // break out of for loop
				}
			}
			if ( found == false ) dataIdx += sprintf(statusBuffer + dataIdx, "\"status\":\"error\"},"); // There should be more to this, but that's all for now.
			Serial.print(statusBuffer);
			jsonrpc_data_item = jsonrpc_data_item->next; // Set pointer for jsonrpc_data_item to next item.
		}
	}
	// Now finish output
	// Should add update rates....
	dataIdx = 0;
	dataIdx += sprintf(statusBuffer + dataIdx, "\"max_update_rate\": %lu,", subscribe_max_update_ms);
	dataIdx += sprintf(statusBuffer + dataIdx, "\"min_update_rate\": %lu,", subscribe_min_update_ms);
	dataIdx += sprintf(statusBuffer + dataIdx, "},\"id\":%u}", json_id);
	Serial.println(statusBuffer);
	return subscribe_matches_found;
}


uint8_t processSet(aJsonObject *json_in_msg) {
	/* process set message
	{"method" : "set", "params" : {"Load_Energy":0,"Charge_Energy":0},"id" : 17}
	*/
	aJsonObject *jsonrpc_params = aJson.getObjectItem(json_in_msg, "params");
	uint8_t parameters_set = 0;
	// Start output
	char statusBuffer[PARAM_BUFFER_SIZE]; // Should be plenty big to hold output for one parameter
	uint16_t dataIdx = 0; // should never exceed PARAM_BUFFER_SIZE
	Serial.print(F("{\"result\":{"));
	// So now we have 1 to n items of unknown name. Will have to iterate, and check existance.
	bool first = false;
	for (uint8_t broker_data_idx = 0; broker_data_idx < BROKERDATA_OBJECTS; broker_data_idx++) {
		dataIdx = 0;
		aJsonObject *jsonrpc_set_param = aJson.getObjectItem(jsonrpc_params, brokerobjs[broker_data_idx]->getName());
		if (jsonrpc_set_param) {
			// Found one!
			if (!first) dataIdx += sprintf(statusBuffer + dataIdx, ","); // preceding comma
			dataIdx += sprintf(statusBuffer + dataIdx, "\"%s\":{\"status\":", jsonrpc_set_param->valuestring);
			if (!brokerobjs[broker_data_idx]->isRO()) {
				// Settable
				double setValue = jsonrpc_set_param->valuefloat;
				bool success = brokerobjs[broker_data_idx]->setData((double)setValue);
				if (success) {
					dataIdx += sprintf(statusBuffer + dataIdx, "\"ok\"}");
					parameters_set++;
				}
				else {
					// couldn't set
					dataIdx += sprintf(statusBuffer + dataIdx, "\"error, couldn't set\"}");
				}
			}
			else dataIdx += sprintf(statusBuffer + dataIdx, "\"error, RO\"}");
			first = true;
			Serial.print(statusBuffer);
			dataIdx = 0;
		}
	}
	// Now finish output
	// Should add update rates....
	dataIdx = 0;
	dataIdx += sprintf(statusBuffer + dataIdx, "},\"id\":%u}", json_id);
	Serial.println(statusBuffer);
	return parameters_set;
}

void genereateSubscribeMessage() {
	// Generates response to subscribe message
}

void clearDataMap() {
	for (uint8_t i = 0; i < BROKERDATA_OBJECTS; i++) {
		::data_map[i] = false;
	}
}

// Move this to separate local library at some point.
bool processSerial() {
	printFreeRam("pSer start");
	if (serial_stream.available()) {
		// skip any accidental whitespace like newlines
		serial_stream.skip();
	}

	if (serial_stream.available()) {
		//Serial.println(F("DATA  FOUND!"));
		uint8_t message_type;
		aJsonObject *serial_msg = aJson.parse(&serial_stream);
		printFreeRam("pSer s_msg");
		
		if (serial_msg != NULL) {
			/*
			Serial.println(F("PROCESSING MESSAGE!"));
			Serial.print(F("AMsg: ")); 
			char *aJsonPtr = aJson.print(serial_msg); 
			Serial.println(aJsonPtr);
			free(aJsonPtr); // So we don't have a memory leak
			*/
			message_type = getMessageType(&serial_msg);
			printFreeRam("pSer gMT");
			//printFreeRam("pSer 1");
			switch (message_type) {
				case (BROKER_STATUS): // Get status of items listed in jsonrpc_params
					if (processBrokerStatus(serial_msg)) {
						printFreeRam("pSer status");
						generateStatusMessage();
					}
					break;
				case (BROKER_SUBSCRIBE): 
					processBrokerSubscribe(serial_msg);
					printFreeRam("pSer sub");
					break;
				case (BROKER_UNSUBSCRIBE): 
					processBrokerUnubscribe(serial_msg); 
					break;
				case (BROKER_SET): 
					processSet(serial_msg);
					break;
				case (BROKER_LIST_DATA): break;
			}
		}
		else {
			aJsonObject *response = aJson.createObject();
			aJsonObject *error = aJson.createObject();
			aJson.addItemToObject(response, "jsonrpc", aJson.createItem("2.0"));
			aJson.addItemToObject(response, "id", aJson.createNull());
			aJson.addItemToObject(error, "code", aJson.createItem(-32700));
			aJson.addItemToObject(error, "message", aJson.createItem("Parse error."));
			aJson.addItemToObject(response, "error", error);
			//aJson.print(response, &serial_stream); // Memory Leak?
			aJson.deleteItem(error);
			aJson.deleteItem(response);
		}
		serial_stream.flush(); // No longer need data
		//printFreeRam("pSer near end 3");
		if (serial_msg) {
			//Serial.println(F("Deleting serial_msg"));
			aJson.deleteItem(serial_msg); // done with incoming message
		}
		//printFreeRam("pSer end 3");
		return true;
	}
	else {
		//Serial.println(F("NO DATA FOUND"));
		return false;
	}

}

