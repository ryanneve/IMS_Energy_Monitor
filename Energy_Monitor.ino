/* Energy Monitor

Reads current values from ADS1015
Channel, function
0, Load current
1, Charge Current
2, Battery Voltage (through divider)
3, Vcc (nominal 5v)

Commuincates via simplified JSON-RPC 1.0 (no ids);

Data Values:
Voltage_battery
Current_load
Current_charge
Power_load
Power_charge
Energy_load
Energy_charge
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
#include <JsonRPCServer.h>
// this includes all functions relating to specific JSON messahe parsing and generation.
#include "Energy_JSON.h"
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

TargetController jsonController(&Serial);

aJsonStream serial_stream(&Serial);

// Global variables

int16_t	json_id = 0;

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
	jsonController.begin(2);

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
	// Define variables used for debug messages
	double vcc; // nominal 5V supply  in V
	double vbatt;
	double currentl, currentc;
	double	powerl,	powerc;
	double	energyl,energyc;
	// Process incoming messages
	jsonController.process();
	// CHECK IF WE GOT A RESET MESSAGE

	char _buf[10];
	//DEBUG START
	// Get data
	/*
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
	vcc = v_cc.getData();
	vbatt = v_batt.getData();
	currentl = current_l.getData();
	currentc = current_c.getData();
	powerl = power_l.getData();
	powerc = power_c.getData();
	energyl = energy_l.getData();
	energyc = energy_l.getData();

	for (uint8_t obj_no = 0; obj_no < BROKERDATA_OBJECTS; obj_no++) {
		if (brokerobjs[obj_no]->subscriptionDue()) Serial.print("S-");
		Serial.print(brokerobjs[obj_no]->getName());
		Serial.print(" = ");
		Serial.print(brokerobjs[obj_no]->getValue());
		Serial.print(" ");
		Serial.print(brokerobjs[obj_no]->getUnit());
		Serial.println();
	}

	// Check for subscriptions
	// We have subscriptions to generate based on sub_due array.
	aJsonObject *json_root, *json_params;
	aJsonObject *jsondataobjs[BROKERDATA_OBJECTS];

	json_root = aJson.createObject();
	aJson.addItemToObject(json_root, "method", aJson.createItem("subscription"));
	aJson.addItemToObject(json_root, "params", json_params = aJson.createObject());
	uint8_t subscriptions = 0;
	for (uint8_t obj_no = 0; obj_no < BROKERDATA_OBJECTS; obj_no++) {
		if (brokerobjs[obj_no]->subscriptionDue()) {
			aJson.addItemToObject(json_params, brokerobjs[obj_no]->getName(), jsondataobjs[subscriptions] = aJson.createObject());
			aJson.addNumberToObject(jsondataobjs[subscriptions], "value", brokerobjs[obj_no]->getValue());
			if (brokerobjs[obj_no]->isVerbose()) {
				aJson.addStringToObject(jsondataobjs[subscriptions], "units", brokerobjs[obj_no]->getUnit());
				aJson.addNumberToObject(jsondataobjs[subscriptions], "sample_time", (int)brokerobjs[obj_no]->getSampleTime()); // returns time in millis() since start which is wrong...
			}
			brokerobjs[obj_no]->setSubscriptionTime(); // Resets subscription time
			subscriptions++;
		}
	}
	aJson.addItemToObject(json_root, "message_time", (int)aJson.createItem(millis()));
	if (subscriptions > 0) {
		char* json_str = aJson.print(json_root);
		Serial.println(json_str);
		free(json_str);
	}
	aJson.deleteItem(json_root);

	
	//if ((uint32_t)(millis() - time_last_message) >= (SUBSCRIPRION_RATE * 1000)) {
		//printFreeRam("Before message: ");
		//time_last_message = millis();
		// First Load
		//Serial.println("Load message");
		//createJSON_subscription(v_batt, current_load,	power_load,		energy_load,	source_load);
		//sendJSONRPCsubString();
		// Now charge
		//Serial.println("Charge message");
		//createJSON_subscription(v_batt, current_charge,	power_charge,	energy_charge,	source_charge);
		//sendJSONRPCsubString();
		//printFreeRam("After message: ");
	//}
	//else {
		// Not time to report.
		delay(LOOP_DELAY_TIME_MS); // MAY BE WORTH LOOKING IN TO LOWERING POWER CONSUMPTION HERE
	//}
}

void dataDebug(){
  // Print out values.
}


bool readJSONRPCstring() {
	/* Look for these methods:
	status - probably pointless due to subscription messages.
	subscribe - probably pointless due to subscription asumptions.
	unsubscribe - probably pointless due to subscription asumptions.
	power - ignored
	token* - ignored, single interface so no token used.
	sampling - ignored, always sampling
	logging - ignored, no database logging done.
	initialize - ? Could be used to reset.
	list_data - return list of data available.
	suspend - ignored at this level
	resume - ignored at this level.
	shutdown - ignored?
	broker_status - report status
	set - col be used to reset totalizers.
	*/
}

void sendJSONRPCsubString() {

}

