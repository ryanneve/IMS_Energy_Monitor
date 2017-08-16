#include "Energy_JSON.h"


extern uint8_t* __brkval;
extern uint8_t __heap_start;


/* Things that can be set:
	- Energy readings can be reset.
	- Current readings can be calibrated to 0. May not be necesary.
	- Vbatt reading can be calibrated to supplied value
*/
// Filter?
//char** jsonFilter = { "method","format","height","width",NULL};
//aJsonObject* jsonObject = aJson.parse(json_string,jsonFilter);


TargetController::TargetController(Stream* stream) : JsonRPCServer(stream) {
	// Constructor
}

void TargetController::initialize(aJsonObject* params) {
	// re-initialize global variables
}

String TargetController::status(aJsonObject* params) {
	// return the status of the requested parameter(s)
	aJsonObject* statusParam = aJson.getObjectItem(params, "status");
	boolean requestedStatus = statusParam->valuebool;

	if (requestedStatus)
	{
		digitalWrite(_led, HIGH);
		return "led HIGH";
	}
	else
	{
		digitalWrite(_led, LOW);
		return "led LOW";
	}
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
	aJsonObject *root, *params, *VBatt, *Current, *Power, *Energy;
	root = aJson.createObject();
	aJson.addItemToObject(root, "method", aJson.createItem("subscription"));
	aJson.addItemToObject(root, "params", params = aJson.createObject());
		aJson.addItemToObject(params, "Voltage_battery", VBatt = aJson.createObject());
			aJson.addNumberToObject(VBatt, "value", v_batt);
			aJson.addStringToObject(VBatt, "units", "V");
		aJson.addItemToObject(params, current_label, Current = aJson.createObject());
			aJson.addNumberToObject(Current, "value", current_A);
			aJson.addStringToObject(Current, "units", "A");
		aJson.addItemToObject(params, power_label, Power = aJson.createObject());
			aJson.addNumberToObject(Power, "value", power_W);
			aJson.addStringToObject(Power, "units", "W");
		aJson.addItemToObject(params, energy_label, Energy = aJson.createObject());
			aJson.addNumberToObject(Energy, "value", energy_Wh);
			aJson.addStringToObject(Energy, "units", "Wh");
    char* json_str = aJson.print(root);
	printFreeRam("IN message: ");
	aJson.deleteItem(root);
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



int freeRAM() {
	int v;
	int mem = ((__brkval == 0) ? (int)&__heap_start : (int)__brkval);
	return ((int)&v - mem);
}

void printFreeRam(const char* msg) {
	Serial.print(msg);
	Serial.print(" free RAM: ");
	Serial.println(freeRAM());
}