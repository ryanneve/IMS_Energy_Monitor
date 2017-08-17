#include "Energy_JSON.h"


/* Things that can be set:
	- Energy readings can be reset.
	- Current readings can be calibrated to 0. May not be necesary.
	- Vbatt reading can be calibrated to supplied value
*/
// Filter?
//char** jsonFilter = (char *[]){ "method","params","data","style",NULL};
//aJsonObject* jsonObject = aJson.parse(json_string,jsonFilter);


TargetController::TargetController(Stream* stream) : JsonRPCServer(stream) {
	// Constructor
}

void TargetController::initialize(aJsonObject* params) {
	// re-initialize global variables
}

String TargetController::status(aJsonObject* params) {
	/* return the status of the requested parameter(s)
	{
		"method" : "status",
			"params" : {
				"data" : ["Voltage", "Load_Current", "Charge_Power"],
				"style" : "terse"},
			"id" : 1
	}
	*/
	aJsonObject* dataParam = aJson.getObjectItem(params, "data");
	aJsonObject* styleParam = aJson.getObjectItem(params, "style");
	aJsonObject* idParam = aJson.getObjectItem(params, "id");
	char * requestedData = dataParam->valuestring;
	char * requestedStyle = styleParam->valuestring;
	int	requestedId = idParam->valueint;
	// reusestedData should point to a string like this:
	// ["depth_m", "spcond_mScm", "sonde_ID"]

	// Figure out how many dataitems.
	uint8_t n = 0;
	uint8_t count = 1;
	for (bool eol = false; eol = true & n < 255; n++ ) {
		if (requestedData[n] == (const char)",") count++;
		else if (requestedData[n] = 0) eol = true;
	}

	// Is it terse or verbose?
	// requestedStyle should be either "terse" or "verbose". If absent, assume verbose.


	/* Now generate response. Omit sample_time and message_time for now. Units only if verbose.
	{
		"result" : {
			"Voltage" : {
				"value" : 13.45,
					"units" : "V",
					"sample_time" : 20110801135647605},
			"Load_Current" : {
				"value" : 1.345,
					"units" : "A",
					"sample_time" : 20110801135647605},
			"Charge_Power" : {
				"value" : 18.090,
							"units" : "W",
							"sample_time" : 20110801023000000},
			"message_time" : {
				"value" : 20110801135647605,
							"units" : "EST"}},
			"id" : 1
	}
	*/
}

void createJSON_subscription(double v_batt, double current_A, double power_W, double energy_Wh, const char* source_str) {
	// Generate JSONRPC subscription message and send it to serial port.
	// Inspired by https://github.com/interactive-matter/aJson
	Serial.print("Building subscription message for "); Serial.println(source_str);
	Serial.print("Including v_batt="); Serial.println(v_batt);
	char current_label[20],	power_label[20], energy_label[20];
	strcpy(current_label, "Current_");
	strcpy(power_label, "Power_");
	strcpy(energy_label, "Energy_");
	strcat(current_label, source_str);
	strcat(power_label, source_str);
	strcat(energy_label, source_str);
	// Now build JSON object
	aJsonObject *json_root, *json_params, *json_VBatt, *json_Current, *json_Power, *json_Energy;
	json_root = aJson.createObject();
	aJson.addItemToObject(json_root, "method", aJson.createItem("subscription"));
	aJson.addItemToObject(json_root, "params", json_params = aJson.createObject());
		aJson.addItemToObject(json_params, "Voltage_battery", json_VBatt = aJson.createObject());
			aJson.addNumberToObject(json_VBatt, "value", v_batt);
			aJson.addStringToObject(json_VBatt, "units", "V");
		aJson.addItemToObject(json_params, current_label, json_Current = aJson.createObject());
			aJson.addNumberToObject(json_Current, "value", current_A);
			aJson.addStringToObject(json_Current, "units", "A");
		aJson.addItemToObject(json_params, power_label, json_Power = aJson.createObject());
			aJson.addNumberToObject(json_Power, "value", power_W);
			aJson.addStringToObject(json_Power, "units", "W");
		aJson.addItemToObject(json_params, energy_label, json_Energy = aJson.createObject());
			aJson.addNumberToObject(json_Energy, "value", energy_Wh);
			aJson.addStringToObject(json_Energy, "units", "Wh");
    char* json_str = aJson.print(json_root);
	aJson.deleteItem(json_root);
    Serial.println(json_str);
	free(json_str);
}

bool TargetController::set_data(aJsonObject* params) {
	/* Message will look something like this:
	{"method":"set","params":{<param>:<value>}}

	So now we have params as an aJson object containing {<param>,<value>}
	*/
	//params-->print();
}
void TargetController::list_data(aJsonObject* params) {
  // List available data
  Serial.println(" This will be a list of available data");
}
void TargetController::subscribe(aJsonObject* params) {
	// subscribe to value
	Serial.println(" This will subscribe to parameters");
}
void TargetController::unsubscribe(aJsonObject* params) {
	// unsubscribe from data
	Serial.println(" This will unsubscribe from parameters");
}