/* Energy Monitor

Reads current values from ADS1015
Reads Voltage from A0

Commuincates via simplified JSON-RPC 1.0 (no ids);

Assumes all values are "subscribed" so sends this out every second:
{
"method":"subscription",
"params": {
"Voltage_Battery": {
"Value" : <v_batt>,
"units" : "Volts"},
"Power_Load" : {
"value":<power_load>,
"units":"Watts"},
"Power_Charge" : {
"value":<power_charge>,
"units":"Watts"},
"Energy_Load" : {
"value":<power_load>,
"units":"Watt-hours"},
"Energy_Charge" : {
"value":<power_charge>,
"units":"Watt-hours"}
}
}

*/

#include <Wire.h>
#include <Adafruit_ADS1015.h>
// include the aJSON library
#include <aJSON.h>
// include the JsonRPC library
#include <JsonRPCServer.h>

#include "Energy_JSON.h"



#define V_DIV_HIGH	20000.0
#define V_DIV_LOW	10000.0
#define V_DIV_PIN	A0
#define ADS1015_ADDRESS 0x48
#define MS_PER_HR 3600000
#define SUBSCRIPRION_RATE 1	// Rate at which "subscription" message should be repeated in seconds.
#define LOOP_DELAY_TIME_MS 100	// Time in ms to wait between sampling loops.







// Define Objects
Adafruit_ADS1015 ads(ADS1015_ADDRESS);     /* Use this for the 12-bit version */

TargetController jsonController(&Serial);

// Global variables
int16_t adc_load, adc_charge;
uint32_t time_last_sample_ms;	// When did we take the last sample?
uint32_t time_delta_ms;						// Time difference between last sample and now
uint32_t time_last_message;					// Time last subscription message was sent.
float	v_batt, power_load, power_charge;	// Holds IO values
float	energy_load, energy_charge;


void setup() {
	ads.setGain(GAIN_TWOTHIRDS);  // 2/3x gain +/- 6.144V  1 bit = 3mV      0.1875mV (default)
	ads.begin();
	initialize();
}

void loop()
{
	getData(); // Refreshes global values and totalizes energy.
	if (readJSONRPCstring()) {
		// Do something
	}
	if ((uint32_t)(millis() - time_last_message) >= (SUBSCRIPRION_RATE * 1000)) {
		time_last_message = millis();
		buildJSONRPCsubscriptionString();
		sendJSONRPCsubString();
	}
	else {
		// Not time to report.
		delay(LOOP_DELAY_TIME_MS); // MAY BE WORTH LOOKING IN TO LOWERING POWER CONSUMPTION HERE
	}
}


void getData() {
	time_delta_ms = (uint32_t)(millis() - time_last_sample_ms);	// Time between this sample and last.
	time_last_sample_ms = millis();
	adc_load = ads.readADC_SingleEnded(0);
	adc_charge = ads.readADC_SingleEnded(1);
	v_batt = getVolts(10, 2);
	power_load = getPower(v_batt, adc_load);
	power_charge = getPower(v_batt, adc_charge);
	energy_load += getEnergy(power_load, time_delta_ms);
	energy_charge += getEnergy(power_charge, time_delta_ms);
}


float getVolts(uint8_t iters = 10, uint8_t throw_outs = 2) {
	// Since it can take a few readings for Analog pin to stabilize, take some readings, throw out the first few and average the rest.
	float v_tot = 0;
	float volt_spl = 0;
	for (uint8_t sample = 0; sample < iters; sample++) {
		volt_spl = analogRead(V_DIV_PIN) * V_DIV_HIGH / V_DIV_LOW;
		if (sample >= throw_outs) {
			v_tot += volt_spl;
		}
	}
	return v_tot / (float)(iters - throw_outs);
}

float getPower(float volts, float amps) {
	return  volts * amps;
}

float getEnergy(float power, uint32_t time_delta) {
	return (power * time_delta) / (float)MS_PER_HR;
}

void initialize() {
	// Re-initialize energy totalizers
	energy_load = 0.0;
	energy_charge = 0.0;
	time_last_sample_ms = millis();
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

void buildJSONRPCsubscriptionString() {

}

void sendJSONRPCsubString() {

}