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
	Serial.print("Requested the following data: ");
	Serial.println(requestedData);
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
void TargetController::broker_status(aJsonObject* params) {
	// return broker status from data
	Serial.println(" This will return broker status");
}