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

#include "broker_util.h"
#include "E_Mon.h"
#include "ADS1115.h"
#include "broker_data.h"
#include <timelib.h>
#include <aJSON.h>


#define ACS715_mV_per_A 133.0
#define V_DIV_LOW	 10000.0
#define V_DIV_HIGH  21000.0
#define ADC_CHANNEL_LOAD_CURRENT	0
#define ADC_CHANNEL_CHARGE_CURRENT	1
#define ADC_CHANNEL_VOLTAGE	2
#define ADC_CHANNEL_VCC	3
#define BROKERDATA_OBJECTS 12
#define BROKER_MIN_UPDATE_RATE_MS 500
#define PARAM_BUFFER_SIZE  200 // should be >~100

// define constants
const uint8_t ADS1115_ADDRESS = 0x48;
const uint16_t LOOP_DELAY_TIME_MS = 2000;	// Time in ms to wait between sampling loops.
//const uint8_t STATVALWIDTH = 5;	// Used for converting double to string
//const uint8_t STATVALPREC = 3;	// Used for converting double to string


const char TZ[] = "UTC"; // Time zone is forced to UTC for now.

// Define Objects
ADS1115 ads(ADS1115_ADDRESS);
aJsonStream serial_stream(&Serial);

// Someday we might load all this from EEPROM so that the code can be as generic as possible.
StaticData	volt_div_low("V_div_low", "Ohms", V_DIV_LOW,7,2);
StaticData	volt_div_high("V_div_high", "Ohms", V_DIV_HIGH,7,2);
//VoltageData2 v_batt2("Voltage", ads, ADC_CHANNEL_VOLTAGE, volt_div_high, volt_div_low);
VoltageData v_batt("Voltage", ads, ADC_CHANNEL_VOLTAGE, V_DIV_HIGH, V_DIV_LOW,6,3);
VoltageData v_cc("Vcc", ads, ADC_CHANNEL_VCC, 0, 1,5,3);	// No voltage divider
CurrentData	current_l("Load_Current", ads, v_cc, ADC_CHANNEL_LOAD_CURRENT, ACS715_mV_per_A,6,3);
CurrentData	current_c("Charge_Current", ads, v_cc, ADC_CHANNEL_CHARGE_CURRENT, ACS715_mV_per_A,6,3);
PowerData	power_l("Load_Power", current_l, v_batt,7,3);
PowerData	power_c("Charge_Power", current_c, v_batt,7,3);
EnergyData	energy_l("Load_Energy", power_l,10,3);
EnergyData	energy_c("Charge_Energy", power_c,10,3);
TimeData	date_sys("Date_UTC",true,8,0);
TimeData	time_sys("Time_UTC",false,6,0);
// Now an array to hold above objects as their base class.
BrokerData *brokerobjs[BROKERDATA_OBJECTS];



// Global variables
int16_t	json_id = 0;
bool status_verbose = true; // true is default.
bool data_map[BROKERDATA_OBJECTS]; // Used to mark broker objects we are interested in.
char broker_start_time[] = "20000101120000"; // Holds start time

// Constants
const char ON_NEW[] = "on_new";
const char ON_CHANGE[] = "on_change";

const uint8_t JSON_REQUEST_COUNT = 8; // How many different request types are there.
enum json_r_t {
	BROKER_STATUS = 0,
	BROKER_SUBSCRIBE = 1,
	BROKER_UNSUBSCRIBE = 2,
	BROKER_SET = 3,
	BROKER_LIST_DATA = 4,
	BROKER_RESET = 5,
	BROKER_B_STATUS = 6,
	BROKER_ERROR = 7
};
const char *REQUEST_STRINGS[JSON_REQUEST_COUNT] = { "status","subscribe","unsubscribe","set","list_data","reset","broker_status","" };


#ifdef __cplusplus
extern "C" {
#endif
	void startup_early_hook() {
		WDOG_TOVALL = 10000; // time in ms between resets. 10000 = 10 seconds
		WDOG_TOVALH = 0;
		WDOG_PRESC = 0; // prescaler
		WDOG_STCTRLH = (WDOG_STCTRLH_ALLOWUPDATE | WDOG_STCTRLH_WDOGEN); // Enable WDG
	}
#ifdef __cplusplus
}
#endif


void setup() {
	startup_early_hook();
	// set the Time library to use Teensy 3.0's RTC to keep time
	setSyncProvider(getTeensy3Time);
    Wire.begin();
	Serial.begin(57600);	//USB
	while (!Serial);	// Leonardo seems to need this
	WatchdogReset();
	Serial.println(F("UNC-IMS Power Monitor 0.3"));
	delay(3000);
	WatchdogReset();
	Serial.println(ads.testConnection() ? F("ADS1115 connected") : F("ADS1115  failed"));
	ads.initialize();
	// We're going to do single shot sampling
	ads.setMode(ADS1115_MODE_SINGLESHOT);
	// Slow things down so that we can see that the "poll for conversion" code works
	ads.setRate(ADS1115_RATE_8);
	// Set the gain (PGA) +/- 6.144v
	// Note that any analog input must be higher than �0.3V and less than VDD +0.3
	ads.setGain(ADS1115_PGA_6P144);

	if (timeStatus() != timeSet) {
		Serial.println("Unable to sync with the RTC");
	}
	else {
		Serial.println("RTC has set the system time");
	}
	// Add parameter objects to brokerobjs[]
	brokerobjs[0] = &v_batt;
	brokerobjs[1] = &v_cc;
	brokerobjs[2] = &current_l;
	brokerobjs[3] = &current_c;
	brokerobjs[4] = &power_l;
	brokerobjs[5] = &power_c;
	brokerobjs[6] = &energy_l;
	brokerobjs[7] = &energy_c;
	brokerobjs[8] = &volt_div_low;
	brokerobjs[9] = &volt_div_high;
	brokerobjs[10] = &date_sys;
	brokerobjs[11] = &time_sys;
	getSampleTimeStr(broker_start_time);
	WatchdogReset();
}

void loop()
{
	printFreeRam("loop0");
	WatchdogReset();
	//datetime.getData(); // Update clock values.
	// Process incoming messages
	processSerial();
	WatchdogReset();
	// retreive new data from ADC
	v_cc.getData();
	v_batt.getData();
	current_l.getData();
	current_c.getData();
	power_l.getData();
	power_c.getData();
	energy_l.getData();
	energy_l.getData();
	WatchdogReset();
	// See what subscriptions are up
	if (checkSubscriptions() > 0) processSubscriptions2();
	WatchdogReset();
	delay(LOOP_DELAY_TIME_MS); // MAY BE WORTH LOOKING IN TO LOWERING POWER CONSUMPTION HERE
}

uint8_t checkSubscriptions() {
	printFreeRam("cS start");
	uint8_t subs = 0;
	for (uint8_t obj_no = 0; obj_no < BROKERDATA_OBJECTS; obj_no++) {
		if (::brokerobjs[obj_no]->subscriptionDue()) {
			::data_map[obj_no] = true;
			subs++;
		}
		else ::data_map[obj_no] = false;
	}
	return subs;
}

void processSubscriptions2() {
	/* Based on settings in data_map, generates a subscrition message
	Currently uses aJson to generate message, but this may be un-necessary
	*/
	printFreeRam("pSub2 start");
	Serial.print("{\"method\":\"subscription\",");
	Serial.print("\"params\":{");
	char statusBuffer[PARAM_BUFFER_SIZE]; // Should be plenty big to hold output for one parameter
	uint8_t dataIdx = 0;
	bool first = true;
	for (uint8_t obj_no = 0; obj_no < BROKERDATA_OBJECTS; obj_no++) {
		if (::data_map[obj_no] == true) {
			::brokerobjs[obj_no]->getData(); // Update values
			char dataStr[20];	// HOLDS A STRING REPRESENTING A SINGLE VALUE
			if (!first) dataIdx += sprintf(statusBuffer + dataIdx, ",");
			else first = false;
			dataIdx += sprintf(statusBuffer + dataIdx, "\"%s\":{", ::brokerobjs[obj_no]->getName());
			::brokerobjs[obj_no]->dataToStr(dataStr);
			dataIdx += sprintf(statusBuffer + dataIdx, "\"value\":%s", dataStr);
			if (::brokerobjs[obj_no]->isVerbose()) {
				dataIdx += sprintf(statusBuffer + dataIdx, ",\"units\":\"%s\"", ::brokerobjs[obj_no]->getUnit());
				// Only report min and max if they exist
				double min_d = ::brokerobjs[obj_no]->getMin();
				double max_d = ::brokerobjs[obj_no]->getMax();
				if (min_d == min_d) dataIdx += sprintf(statusBuffer + dataIdx, ",\"min\":\"%f\"", min_d);
				if (max_d == max_d) dataIdx += sprintf(statusBuffer + dataIdx, ",\"max\":\"%f\"", max_d);
				dataIdx += sprintf(statusBuffer + dataIdx, ",\"sample_time\":\"%s\"", ::brokerobjs[obj_no]->getSplTimeStr());
			}
			dataIdx += sprintf(statusBuffer + dataIdx, "}"); // Close out this parameter
			Serial.print(statusBuffer);
			dataIdx = 0;
		}
	}
	dataIdx = addMsgTime(statusBuffer, 0,::TZ);
	dataIdx += sprintf(statusBuffer + dataIdx, "}}"); // Close out params and message
	Serial.println(statusBuffer);
	//printFreeRam("pSub end");
}


/*
void processSubscriptions() {
	//Based on settings in data_map, generates a subscrition message
	//Currently uses aJson to generate message, but this may be un-necessary
	
	printFreeRam("pSub start");
	// This should be a function .
	uint8_t sub_idx = 0;
	aJsonObject *json_root, *json_params, *json_msgtime;
	aJsonObject *jsondataobjs[BROKERDATA_OBJECTS]; // Holds objects used for data output
	json_root = aJson.createObject();
	aJson.addItemToObject(json_root, "method", aJson.createItem("subscription"));
	aJson.addItemToObject(json_root, "params", json_params = aJson.createObject());
	for (uint8_t obj_no = 0; obj_no < BROKERDATA_OBJECTS; obj_no++) {
		if (::data_map[obj_no] == true) {
			aJson.addItemToObject(json_params, brokerobjs[obj_no]->getName(), jsondataobjs[sub_idx] = aJson.createObject());
			aJson.addNumberToObject(jsondataobjs[sub_idx], "value", brokerobjs[obj_no]->getValue());
			if (brokerobjs[obj_no]->isVerbose()) {
				aJson.addStringToObject(jsondataobjs[sub_idx], "units", brokerobjs[obj_no]->getUnit());
				// Only report min and max if they exist
				double min_d = brokerobjs[obj_no]->getMin();
				double max_d = brokerobjs[obj_no]->getMax();
				if ( min_d == min_d ) aJson.addNumberToObject(jsondataobjs[sub_idx], "min", min_d); // min and max are specific to this broker.
				if ( max_d == max_d ) aJson.addNumberToObject(jsondataobjs[sub_idx], "max", max_d);
				aJson.addNumberToObject(jsondataobjs[sub_idx], "sample_time", (unsigned long)brokerobjs[obj_no]->getSampleTime()); // returns time in millis() since start which is wrong...
			}
			brokerobjs[obj_no]->setSubscriptionTime(); // Resets subscription time
			sub_idx++;
		}
	}
	aJson.addItemToObject(json_root, "message_time", json_msgtime = aJson.createObject());
	char msgTime[15];
	getSampleTimeStr(msgTime);
	aJson.addStringToObject(json_msgtime, "value", msgTime); // THIS REALLY SHOULD BE AN INTEGER, but aJson doesn't support uint64_t which would hold an integer this big.
	aJson.addStringToObject(json_msgtime, "units", TZ);
	//Serial.print(F("JSON string:"));
	char * aJsonPtr = aJson.print(json_root);
	Serial.println(aJsonPtr); // Prints out subscription message
	free(aJsonPtr); // So we don't have a memory leak
	aJson.deleteItem(json_root);
	//printFreeRam("pSub end");
}
*/



uint16_t getMessageType(aJsonObject** json_in_msg,int16_t * json_id, const uint8_t json_req_count) {
	// Extract method and ID from message.
	//printFreeRam("gMT start");
	uint8_t json_method = BROKER_ERROR;

	aJsonObject *jsonrpc_method = aJson.getObjectItem(*json_in_msg, "method");
	for (uint8_t j_method = 0; j_method < json_req_count; j_method++) {
		if (!strcmp(jsonrpc_method->valuestring, ::REQUEST_STRINGS[j_method])) {
			//Serial.print(F("JSON request: "));Serial.println(REQUEST_STRINGS[j_method]);
			json_method = j_method;
			break;
		}
	}
	// Get ID here
	aJsonObject *jsonrpc_id = aJson.getObjectItem(*json_in_msg, "id");
	*json_id = jsonrpc_id->valueint;
	//printFreeRam("gMT end3");
	return json_method;
}





uint8_t processStatus(aJsonObject *json_in_msg) {
	//printFreeRam("pBS start");
	uint8_t status_matches_found = 0;
	// get params which will contain data and style
	// First process style
	aJsonObject *jsonrpc_params = aJson.getObjectItem(json_in_msg, "params");
	// Extract Style from params
	aJsonObject *jsonrpc_style = aJson.getObjectItem(jsonrpc_params, "style");
	if (!strcmp(jsonrpc_style->valuestring, "terse")) ::status_verbose = false;
	else ::status_verbose = true;
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
			if (!strcmp(jsonrpc_data_item->valuestring, ::brokerobjs[broker_data_idx]->getName())) {
				//Serial.print(F("B data: ")); Serial.println(jsonrpc_data_item->valuestring);
				::data_map[broker_data_idx] = true; 
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
	printResultStr();
	char statusBuffer[PARAM_BUFFER_SIZE]; // Should be plenty big to hold output for one parameter

	for (uint8_t obj_no = 0; obj_no < BROKERDATA_OBJECTS; obj_no++) {
		if (::data_map[obj_no] == true) {
			char statusValue[20] = "-999"; // Holds status double value as a string
			::brokerobjs[obj_no]->dataToStr(statusValue);
			if (!first) dataIdx += sprintf(statusBuffer + dataIdx, ","); // preceding comma
			dataIdx += sprintf(statusBuffer + dataIdx, "\"%s\":{", ::brokerobjs[obj_no]->getName());
			dataIdx += sprintf(statusBuffer + dataIdx, "\"value\":%s", statusValue);
			if (::status_verbose == true) {
				dataIdx += sprintf(statusBuffer + dataIdx, ",\"units\":\"%s\"", ::brokerobjs[obj_no]->getUnit());
				 // Only report min and max if they exist
				double min_d = ::brokerobjs[obj_no]->getMin();
				double max_d = ::brokerobjs[obj_no]->getMax();
				if (min_d == min_d) {
					::brokerobjs[obj_no]->dataToStr(statusValue);
					dataIdx += sprintf(statusBuffer + dataIdx, ",\"min\":%s", statusValue);
				}
				if (max_d == max_d) {
					::brokerobjs[obj_no]->dataToStr(statusValue);
					dataIdx += sprintf(statusBuffer + dataIdx, ",\"max\":%s", statusValue);
				}
				dataIdx += sprintf(statusBuffer + dataIdx, ",\"sample_time\":%s", ::brokerobjs[obj_no]->getSplTimeStr());
			}
			dataIdx += sprintf(statusBuffer + dataIdx, "}");
			Serial.print(statusBuffer);
			dataIdx = 0; // reset for next Parameter.
			first = false;
		}
	}
	dataIdx = addMsgTime(statusBuffer, dataIdx,::TZ);
	dataIdx = addMsgId(statusBuffer, dataIdx,json_id);
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
	printResultStr();

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
				if (!strcmp(jsonrpc_data_item->valuestring, ::brokerobjs[broker_data_idx]->getName())) {
					unsubscribe_matches_found++;
					// Set subscription up
					::brokerobjs[broker_data_idx]->unsubscribe();
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
	dataIdx = addMsgId(statusBuffer, 0,json_id);
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
		if (!strcmp(jsonrpc_style->valuestring, "terse")) subscribe_verbose = false;
		else subscribe_verbose = true;
	}

	// Extract Optional Updates
	aJsonObject *jsonrpc_updates = aJson.getObjectItem(jsonrpc_params, "updates");
	if (jsonrpc_updates) {
		if (!strcmp(jsonrpc_updates->valuestring, "on_new")) subscribe_on_change = false;
		else subscribe_on_change = true;
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
	bool first = true;
	uint16_t dataIdx = 0; // should never exceed PARAM_BUFFER_SIZE
	printResultStr();
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
				if (!first) dataIdx += sprintf(statusBuffer + dataIdx, ",");
				first = false;
				dataIdx += sprintf(statusBuffer + dataIdx, "\"%s\":{", jsonrpc_data_item->valuestring); // even if it's bad data
				if (!strcmp(jsonrpc_data_item->valuestring, ::brokerobjs[broker_data_idx]->getName())) {
					subscribe_matches_found++;
					// Set subscription up
					::brokerobjs[broker_data_idx]->subscribe(subscribe_min_update_ms, subscribe_max_update_ms);
					::brokerobjs[broker_data_idx]->setSubOnChange(subscribe_on_change);
					::brokerobjs[broker_data_idx]->setVerbose(subscribe_verbose);
					found = true;
					dataIdx += sprintf(statusBuffer + dataIdx, "\"status\":\"ok\"}");
					break; // break out of for loop
				}
			}
			if ( found == false ) dataIdx += sprintf(statusBuffer + dataIdx, "\"status\":\"error\"}"); // There should be more to this, but that's all for now.
			Serial.print(statusBuffer);
			jsonrpc_data_item = jsonrpc_data_item->next; // Set pointer for jsonrpc_data_item to next item.
		}
	}
	// Now finish output
	// Should add update rates....
	dataIdx = 0;
	dataIdx += sprintf(statusBuffer + dataIdx, ",\"max_update_rate\":%lu", subscribe_max_update_ms);
	dataIdx += sprintf(statusBuffer + dataIdx, ",\"min_update_rate\":%lu", subscribe_min_update_ms);
	dataIdx += sprintf(statusBuffer + dataIdx, ",\"updates\":\"%s\"", subscribe_on_change?ON_CHANGE:ON_NEW); //ON_NEW ON_CHANGE
	Serial.print(statusBuffer);
	dataIdx = addMsgTime(statusBuffer, 0,::TZ);
	dataIdx = addMsgId(statusBuffer, dataIdx,json_id);
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
	printResultStr();
	// So now we have 1 to n items of unknown name. Will have to iterate, and check existance.
	bool first = true;
	for (uint8_t broker_data_idx = 0; broker_data_idx < BROKERDATA_OBJECTS; broker_data_idx++) {
		dataIdx = 0;
		aJsonObject *jsonrpc_set_param = aJson.getObjectItem(jsonrpc_params, ::brokerobjs[broker_data_idx]->getName());
		if (jsonrpc_set_param) {
			// Found one!
			//Serial.println(broker_data_idx);
			//char *aJsonPtr = aJson.print(jsonrpc_set_param);
			//Serial.println(aJsonPtr);
			//free(aJsonPtr); // So we don't have a memory leak
			if (!first) dataIdx += sprintf(statusBuffer + dataIdx, ","); // preceding comma
			dataIdx += sprintf(statusBuffer + dataIdx, "\"%s\":{\"status\":", ::brokerobjs[broker_data_idx]->getName()); // name of parameter and status...
			if (!::brokerobjs[broker_data_idx]->isRO()) {
				// Settable
				double setValue = -999;
				if (jsonrpc_set_param->type == aJson_Int) setValue = (double)jsonrpc_set_param->valueint;
				else if (jsonrpc_set_param->type == aJson_Long) setValue = (double)jsonrpc_set_param->valuelong;
				else if (jsonrpc_set_param->type == aJson_Float) setValue = (double)jsonrpc_set_param->valuefloat;
				bool success = ::brokerobjs[broker_data_idx]->setData((double)setValue);
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
			first = false;
			Serial.print(statusBuffer);
			dataIdx = 0;
		}
	}
	// Now finish output
	// Should add update rates....
	dataIdx = addMsgTime(statusBuffer, 0,::TZ);
	dataIdx = addMsgId(statusBuffer, dataIdx,json_id);
	Serial.println(statusBuffer);
	return parameters_set;
}

void processListData() {
	/* List data parameters available.
	{"method" : "list_data","id" : 18}
	*/
	// Start output
	char statusBuffer[PARAM_BUFFER_SIZE]; // Should be plenty big to hold output for one parameter
	bool first = true;
	uint16_t dataIdx = 0; // should never exceed PARAM_BUFFER_SIZE
	char param_type[] = "\"Rx\"";
	printResultStr();
	for (uint8_t broker_data_idx = 0; broker_data_idx < BROKERDATA_OBJECTS; broker_data_idx++) {
		if (::brokerobjs[broker_data_idx]->isRO()) param_type[2] = 'O';
		else param_type[2] = 'W';
		// Break this into multiple lines just to make it easier to read.
		if (!first) dataIdx += sprintf(statusBuffer + dataIdx, ",");
		dataIdx += sprintf(statusBuffer + dataIdx, "\"%s\":", ::brokerobjs[broker_data_idx]->getName());
		dataIdx += sprintf(statusBuffer + dataIdx, "{\"units\":\"%s\",", ::brokerobjs[broker_data_idx]->getUnit());
		dataIdx += sprintf(statusBuffer + dataIdx, "\"type\":%s}", param_type);
		Serial.print(statusBuffer);
		dataIdx = 0;
		first = false;
	}
	dataIdx = addMsgId(statusBuffer, 0,json_id);
	Serial.println(statusBuffer);
}

uint8_t processReset(aJsonObject *json_in_msg) {
	/*Call reset for requested parameters. This will set min and max to 0. if RW, will also set value to 0.
	Message format is like "status", but no "style". 
	Response is like "subscribe";
	*/
	//printFreeRam("pR start");
	uint8_t reset_matches_found = 0;
	// First process style
	aJsonObject *jsonrpc_params = aJson.getObjectItem(json_in_msg, "params");
	clearDataMap(); // Sets all data_map array values to false
	// Now Extract data list
	aJsonObject *jsonrpc_data = aJson.getObjectItem(jsonrpc_params, "data");
	char statusBuffer[PARAM_BUFFER_SIZE]; // Should be plenty big to hold output for one parameter
	bool first = true;
	bool found = false;
	uint16_t dataIdx = 0; // should never exceed PARAM_BUFFER_SIZE
	printResultStr();

	// data will be list of parameters: ["Voltage","Vcc",Current_Load"]
	// Now parse data list
	aJsonObject *jsonrpc_data_item = jsonrpc_data->child;
	while (jsonrpc_data_item) {
		dataIdx = 0;
		found = false;
		if (!first) dataIdx += sprintf(statusBuffer + dataIdx, ",");
		first = false;
		dataIdx += sprintf(statusBuffer + dataIdx, "\"%s\":{", jsonrpc_data_item->valuestring); // even if it's bad data
		for (uint8_t broker_data_idx = 0; broker_data_idx < BROKERDATA_OBJECTS; broker_data_idx++) {
			if (!strcmp(jsonrpc_data_item->valuestring, ::brokerobjs[broker_data_idx]->getName())) {
				// got a match
				found = true;
				dataIdx += sprintf(statusBuffer + dataIdx, "\"status\":\"ok\"}");
				::brokerobjs[broker_data_idx]->resetMin();
				::brokerobjs[broker_data_idx]->resetMax();
				if (!brokerobjs[broker_data_idx]->isRO()) ::brokerobjs[broker_data_idx]->setData(0); // only for "RW" parameters
				reset_matches_found++;
				break; // break out of for loop
			}
		}
		if (found == false) dataIdx += sprintf(statusBuffer + dataIdx, "\"status\":\"error\"}"); // There should be more to this, but that's all for now.
		Serial.print(statusBuffer);
		jsonrpc_data_item = jsonrpc_data_item->next; // Set pointer for jsonrpc_data_item to next item.
	}
	dataIdx = addMsgTime(statusBuffer, 0,::TZ);
	dataIdx = addMsgId(statusBuffer, dataIdx,::json_id);
	Serial.println(statusBuffer);
	return reset_matches_found;
}



void processBrokerStatus() {
	/*
	{
	"result" : {
		"suspended":True|False,
		"power_on":True|False|"unknown",
		"instr_connected":True|False,
		"db_connected":True|False,
		"start_time":<timestamp>,
		"last_data_time":<timestamp>|"None",
		"last_db_time": <timestamp>|"None",
		"message_time" : {
			"value" : 2010092416310000,
			"units" : "EST"}
	},
	"id":8
	}
	*/

	uint16_t dataIdx = 0; // should never exceed PARAM_BUFFER_SIZE
	char statusBuffer[PARAM_BUFFER_SIZE]; // Should be plenty big to hold output for one parameter
	printResultStr();
	Serial.print(F("\"suspended\":\"False\""));
	Serial.print(F(",\"power_on\":\"True\""));
	Serial.print(F(",\"instr_connected\":\"True\""));
	Serial.print(F(",\"db_connected\":\"False\""));
	dataIdx += sprintf(statusBuffer + dataIdx, ",\"start_time\":%s", ::broker_start_time);
	dataIdx += sprintf(statusBuffer + dataIdx, ",\"last_data_time\":%s", ::v_batt.getSplTimeStr());
	dataIdx += sprintf(statusBuffer + dataIdx, ",\"last_db_time\":\"None\"");
	dataIdx = addMsgTime(statusBuffer, dataIdx,::TZ);
	dataIdx = addMsgId(statusBuffer, dataIdx,::json_id);
	Serial.println(statusBuffer);
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
			message_type = getMessageType(&serial_msg, &json_id, JSON_REQUEST_COUNT);
			printFreeRam("pSer gMT");
			//printFreeRam("pSer 1");
			switch (message_type) {
				case (BROKER_STATUS): // Get status of items listed in jsonrpc_params
					if (processStatus(serial_msg)) {
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
				case (BROKER_LIST_DATA): 
					processListData();
					break;
				case (BROKER_RESET):
					processReset(serial_msg);
					break;
				case (BROKER_B_STATUS):
					processBrokerStatus();
					break;
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

const char * BoolToString(const bool b)
{
	return b ? "true" : "false";
}

time_t getTeensy3Time()
{
	return Teensy3Clock.get();
}


void WatchdogReset()
{
	//Serial.println("WatchdogReset");
	// use the following 4 lines to kick the dog
	noInterrupts();
	WDOG_REFRESH = 0xA602;
	WDOG_REFRESH = 0xB480;
	interrupts();
	// if you don't refresh the watchdog timer before it runs out, the system will be rebooted
	delay(1); // the smallest delay needed between each refresh is 1ms. anything faster and it will also reboot.
}