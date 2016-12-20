#include "Energy_JSON.h"

TargetController::TargetController(Stream* stream) : JsonRPCServer(stream), _led(13) {
	// TODO Auto-generated constructor stub

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


{
	"method":"subscription",
		"params" : {
		"Voltage_Battery": {
			"Value" : <v_batt>,
			"units" : "Volts"},
		"Power_Load" : {
			"value":<power_load>,
			"units" : "Watts"},
		"Power_Charge" : {
			"value":<power_charge>,
			"units" : "Watts"},
		"Energy_Load" : {
			"value":<energy_load>,
			"units" : "Watt-hours"},
		"Energy_Charge" : {
			"value":<energy_charge>,
			"units" : "Watt-hours"}
	}
}

void createJSON_subscription(double v_batt, double power_load, double power_charge, double energy_load, double energy_charge) {
	// Generate JSONRPC subscription message and send it to serial port.
	// Inspired by https://github.com/interactive-matter/aJson
	aJsonObject *root, *params, *VBatt, *PowerLoad, *PowerCharge, *EnergyLoad, *EnergyCharge;
	root = aJson.createObject();
	aJson.addItemToObject(root, "method", aJson.createItem("subscription"));
	aJson.addItemToObject(root, "params", params = aJson.createObject());
		aJson.addItemToObject(params, "Voltage_battery", VBatt = aJson.createObject());
			aJson.addNumberToObject(VBatt, "value", v_batt);
			aJson.addStringToObject(VBatt, "units", "Volts");
		aJson.addItemToObject(params, "Power_Load", PowerLoad = aJson.createObject());
			aJson.addNumberToObject(PowerLoad, "value", power_load);
			aJson.addStringToObject(PowerLoad, "units", "Watts");
		aJson.addItemToObject(params, "Power_Charge", PowerCharge = aJson.createObject());
			aJson.addNumberToObject(PowerCharge, "value", power_charge);
			aJson.addStringToObject(PowerCharge, "units", "Watts");
		aJson.addItemToObject(params, "Energy_Load", EnergyLoad = aJson.createObject());
			aJson.addNumberToObject(EnergyLoad, "value", energy_load);
			aJson.addStringToObject(EnergyLoad, "units", "Watt-Hours");
		aJson.addItemToObject(params, "Energy_Charge", EnergyCharge = aJson.createObject());
			aJson.addNumberToObject(EnergyCharge, "value", energy_charge);
			aJson.addStringToObject(EnergyCharge, "units", "Watt-Hours");
		root.print(&Serial);
}
