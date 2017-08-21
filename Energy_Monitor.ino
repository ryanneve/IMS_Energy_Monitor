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

aJsonObject *json_response = aJson.createObject();
//aJsonObject *jsonrpc_method = aJson.createObject();
//aJsonObject *jsonrpc_id = aJson.createObject();
aJsonObject *jsonrpc_params = aJson.createObject(); //GLOBAL
aJsonObject *jsonrpc_style = aJson.createObject();

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
	Serial.println("UNC-IMS Energy Monitor");
	delay(3000);
	//Serial.println(Setup_ads() ? "ADS1115 connection successful" : "ADS1115 connection failed");
	Serial.println(ads.testConnection() ? "ADS1115 connection successful" : "ADS1115 connection failed");
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
	v_batt.subscribe(4);
	v_cc.subscribe(5);
	// END DEBUG

}

void loop()
{
	uint8_t subscriptions = 0; // How many subscriptions are ready for processing this loop.
	// Process incoming messages
	processSerial();
	// CHECK IF WE GOT A RESET MESSAGE

	//DEBUG START
	// Get data
	/*
	char _buf[10];
	double adc;
	adc = v_cc.getADCreading();
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
	double vcc = v_cc.getData();
	double vbatt = v_batt.getData();
	double currentl = current_l.getData();
	double currentc = current_c.getData();
	double powerl = power_l.getData();
	double powerc = power_c.getData();
	double energyl = energy_l.getData();
	double energyc = energy_l.getData();
	// See what subscriptions are up
	for (uint8_t obj_no = 0; obj_no < BROKERDATA_OBJECTS; obj_no++) {
		if (brokerobjs[obj_no]->subscriptionDue()) {
			data_map[obj_no] = true;
			subscriptions++;
			Serial.print("S-");
		}
		else data_map[obj_no] = false;
		Serial.print(brokerobjs[obj_no]->getName());
		Serial.print(" = ");
		Serial.print(brokerobjs[obj_no]->getValue());
		Serial.print(" ");
		Serial.print(brokerobjs[obj_no]->getUnit());
		Serial.println();
	}
	// Check for subscriptions
	// We have subscriptions to generate based on sub_due array.
	if (subscriptions > 0) {
		// This should be a function .
		uint8_t sub_idx = 0;
		aJsonObject *json_root, *json_params;
		aJsonObject *jsondataobjs[BROKERDATA_OBJECTS]; // Holds objects used for data output

		json_root = aJson.createObject();
		aJson.addItemToObject(json_root, "method", aJson.createItem("subscription"));
		aJson.addItemToObject(json_root, "params", json_params = aJson.createObject());
		// This might also be a function
		for (uint8_t obj_no = 0; obj_no < BROKERDATA_OBJECTS; obj_no++) {
			if (data_map[obj_no] == true) {
				aJson.addItemToObject(json_params, brokerobjs[obj_no]->getName(), jsondataobjs[sub_idx] = aJson.createObject());
				aJson.addNumberToObject(jsondataobjs[sub_idx], "value", brokerobjs[obj_no]->getValue());
				if (brokerobjs[obj_no]->isVerbose()) {
					aJson.addStringToObject(jsondataobjs[sub_idx], "units", brokerobjs[obj_no]->getUnit());
					aJson.addNumberToObject(jsondataobjs[sub_idx], "sample_time", (int)brokerobjs[obj_no]->getSampleTime()); // returns time in millis() since start which is wrong...
				}
				brokerobjs[obj_no]->setSubscriptionTime(); // Resets subscription time
				sub_idx++;
			}
		}
		aJson.addItemToObject(json_root, "message_time", (int)aJson.createItem(millis()));
		Serial.print("JSON string:");	aJson.print(json_root, &serial_stream);		Serial.println();	
		for (uint8_t obj_no = 0; obj_no < BROKERDATA_OBJECTS; obj_no++) {
			aJson.deleteItem(jsondataobjs[obj_no]);
		}
		aJson.deleteItem(json_root);
	}
	delay(LOOP_DELAY_TIME_MS); // MAY BE WORTH LOOKING IN TO LOWERING POWER CONSUMPTION HERE
}


uint16_t getMessageType(aJsonObject *json_in_msg) {
	// Create response
	// Check required fields
	aJsonObject *jsonrpc_method = aJson.getObjectItem(json_in_msg, "method");
	aJsonObject *jsonrpc_id = aJson.getObjectItem(json_in_msg, "id");
	jsonrpc_params = aJson.getObjectItem(json_in_msg, "params");
	Serial.print("Parameters: "); aJson.print(jsonrpc_params, &serial_stream); Serial.println();

	if (!jsonrpc_method || !jsonrpc_id) {
		// Not a valid Json-RPC 2.0 message
		aJsonObject *error = aJson.createObject();
		aJson.addItemToObject(error, "code", aJson.createItem(-32600));
		aJson.addItemToObject(error, "message", aJson.createItem("Invalid Request."));
		aJson.addItemToObject(json_response, "error", error);

		if (!jsonrpc_id) {
			aJson.addItemToObject(json_response, "id", aJson.createNull());
			aJson.addItemToObject(error, "data", aJson.createItem("Missing id."));
		}
		else {
			aJson.addItemToObject(json_response, "id", jsonrpc_id);
			aJson.addItemToObject(error, "data", aJson.createItem("Missing method."));
		}

		aJson.print(json_response, &serial_stream);
		aJson.deleteItem(json_response);
		return;
	}
	
	char *method_str = aJson.print(jsonrpc_method); // MIGHT BE A BETTER WAY TO DO THIS
	for (uint8_t json_method = 0; json_method < JSON_REQUEST_COUNT; json_method++) {
		if (strcmp(method_str, REQUEST_STRINGS[json_method])) {
			Serial.print("Found a JSON request match: ");
			Serial.println(REQUEST_STRINGS[json_method]);
			return json_method;
		}
	}
	aJson.deleteItem(jsonrpc_id);
	aJson.deleteItem(jsonrpc_method);
	return BROKER_ERROR;
}

uint8_t processBrokerStatus() {
	uint8_t status_matches_found = 0;
	Serial.println("in processBrokerStatus()");
	// First process style
	aJsonObject *jsonrpc_style = aJson.getObjectItem(jsonrpc_params, "style");
	if (!strcmp(jsonrpc_style->valuestring, "terse")) {
		status_verbose = true;
	}
	else status_verbose = false;
	aJson.deleteItem(jsonrpc_style);
	// Now data
	aJsonObject *jsonrpc_data = aJson.getObjectItem(jsonrpc_params, "data");
	aJsonObject *jsonrpc_data_item = jsonrpc_data->child;
	while (jsonrpc_data_item) { 
		for (uint8_t broker_data_idx = 0; broker_data_idx < BROKERDATA_OBJECTS; broker_data_idx++) {
			//Serial.print("Comparing "); Serial.print(jsonrpc_data_item->valuestring); Serial.print(" to "); Serial.println(brokerobjs[broker_data_idx]->getName());
			if (strcmp(jsonrpc_data_item->valuestring, brokerobjs[broker_data_idx]->getName())) {
				data_map[broker_data_idx] = 0;
			}
			else {

				Serial.print("Found broker data match: "); Serial.println(jsonrpc_data_item->valuestring);
				data_map[broker_data_idx] = 1; 
				status_matches_found++;
			}
		}
		jsonrpc_data_item = jsonrpc_data_item->next;
	}
	aJson.deleteItem(jsonrpc_data);
	// Finally ID
	aJsonObject *jsonrpc_id = aJson.getObjectItem(jsonrpc_params, "id");
	if (jsonrpc_id) json_id = jsonrpc_id->valueint;
	else json_id = 0;
	aJson.deleteItem(jsonrpc_id);
	aJson.deleteItem(jsonrpc_params); // Should no longer be needed
	return status_matches_found;
}

void generateStatusMessage() {
	// nased on contents of datamap array, generate status message.
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
            "sample_time":20110801135647605},
    "id" : 1
}
*/
	Serial.println("1 in generateStatusMessage()");
	json_response = aJson.createObject();
	aJsonObject *json_results;
	aJson.addItemToObject(json_response, "result", json_results = aJson.createObject());
	Serial.println("2 in generateStatusMessage()");
	aJsonObject *jsondataobjs[BROKERDATA_OBJECTS]; // Holds objects used for data output DOES THIS NEED TO BE DELETED?
	Serial.println("4 in generateStatusMessage()");
	uint8_t sub_idx = 0;
	for (uint8_t obj_no = 0; obj_no < BROKERDATA_OBJECTS; obj_no++) {
		Serial.println(obj_no);
		if (data_map[obj_no] == true) {
			Serial.print("    Found obj "); Serial.print(brokerobjs[obj_no]->getName());
			// generate message
			//aJson.addItemToObject(json_results, brokerobjs[obj_no]->getName(), jsondataobjs[obj_no] = aJson.createObject());
			//aJson.addNumberToObject(jsondataobjs[sub_idx], "value", brokerobjs[obj_no]->getValue());
			if (status_verbose) {
				//aJson.addStringToObject(jsondataobjs[sub_idx], "units", brokerobjs[obj_no]->getUnit());
				//aJson.addNumberToObject(jsondataobjs[sub_idx], "sample_time", brokerobjs[obj_no]->getSampleTime());

			}
			sub_idx++;
		}
	}
	aJson.addNumberToObject(json_results, "message_time", (unsigned long)aJson.createItem(millis()));
	aJson.addNumberToObject(json_response, "id", json_id);
	Serial.print("JSON string:");	aJson.print(json_response, &serial_stream);		Serial.println();
	for (uint8_t obj_no = 0; obj_no < BROKERDATA_OBJECTS; obj_no++) {
		aJson.deleteItem(jsondataobjs[obj_no]);
	}
	//aJson.deleteItem(json_results);
	aJson.deleteItem(json_response); // should also delete json_results
	
	Serial.println("in generateStatusMessage()");
}

void clearDataMap() {
	for (uint8_t i = 0; i < BROKERDATA_OBJECTS; i++) {
		data_map[i] = 0;
	}
}

// Move this to separate local library at some point.
bool processSerial() {
	if (serial_stream.available()) {
		// skip any accidental whitespace like newlines
		serial_stream.skip();
	}

	if (serial_stream.available()) {
		Serial.println("DATA  FOUND!");
		aJsonObject *msg = aJson.parse(&serial_stream);

		if (msg != NULL) {
			Serial.println("PROCESSING MESSAGE!");
			uint8_t message_type = getMessageType(msg);
			switch (message_type) {
			case (BROKER_STATUS): // Get status of items listed in jsonrpc_params
				if (processBrokerStatus()) generateStatusMessage();
				break;
			case (BROKER_SUBSCRIBE): break;
			case (BROKER_UNSUBSCRIBE): break;
			case (BROKER_SET): break;
			case (BROKER_LIST_DATA): break;
			}
		}
		else {
			serial_stream.flush();
			aJsonObject *response = aJson.createObject();
			aJsonObject *error = aJson.createObject();
			aJson.addItemToObject(response, "jsonrpc", aJson.createItem("2.0"));
			aJson.addItemToObject(response, "id", aJson.createNull());
			aJson.addItemToObject(error, "code", aJson.createItem(-32700));
			aJson.addItemToObject(error, "message", aJson.createItem("Parse error."));
			aJson.addItemToObject(response, "error", error);
			aJson.print(response, &serial_stream);
			aJson.deleteItem(error);
			aJson.deleteItem(response);
		}
		aJson.deleteItem(msg);
		return true;
	}
	else {
		Serial.println("NONE FOUND");
		return false;
	}

}
