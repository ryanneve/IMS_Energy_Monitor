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

Expects, but ignores the following methods:
"subscribe" - We assume everything is subscribed
"unsubscribe"
"tokenAcquire", "tokenForceAcquire", "tokenRelease", "tokenOwner" - Only one data client, no tokens used at this level.
"power" - doesn't really apply
"initialize" - could be used to reset energy values
"suspend", "resume", - is there any point?
"restart", "shutdown" - may not apply in this environment
"broker_status" - no point

TO DO
	- Make resistor values setable and store them in EEPROM.
*/



//#include <Wire.h>
//#include <Adafruit_ADS1015.h>
#include "ADS1115.h"
// include the aJSON library
#include <aJSON.h>
// include the JsonRPC library
#include <JsonRPCServer.h>
// this includes all functions relating to specific JSON messahe parsing and generation.
#include "Energy_JSON.h"


#define MS_PER_HR 3600000
#define ACS715_mV_per_A 133.0
#define V_DIV_LOW	 10000.0
#define V_DIV_HIGH  20000.0

const double V_DIVIDER = (V_DIV_LOW + V_DIV_HIGH) / V_DIV_LOW;	// An overriding value should be saved to EEPROM.
const uint8_t LOAD_CHANNEL = 0;
const uint8_t CHARGE_CHANNEL = 1;
const uint8_t VBATT_CHANNEL = 2;
const uint8_t VCC_CHANNEL = 3;
const uint8_t ADS1115_ADDRESS = 0x49;
const uint8_t  SUBSCRIPRION_RATE = 5;	// Rate at which "subscription" message should be repeated in seconds.
const uint16_t LOOP_DELAY_TIME_MS = 100;	// Time in ms to wait between sampling loops.


// Define Objects
ADS1115 ads(ADS1115_ADDRESS);

TargetController jsonController(&Serial);

aJsonStream serial1_stream(&Serial1);

// Global variables


void setup() {
  Wire.begin();
	Serial.begin(57600);	//USB
	Serial1.begin(57600);	// PINS
	while (!Serial);	// Leonardo seems to need this
	while (!Serial1);	// Leonardo seems to need this
	Serial.println("UNC-IMS Energy Monitor");
	delay(3000);
	Serial.println(ads.testConnection() ? "ADS1115 connection successful" : "ADS1115 connection failed");
	ads.initialize();
	// We're going to do single shot sampling
	ads.setMode(ADS1115_MODE_SINGLESHOT);
	// Slow things down so that we can see that the "poll for conversion" code works
	ads.setRate(ADS1115_RATE_8);
	// Set the gain (PGA) +/- 6.144v
	// Note that any analog input must be higher than �0.3V and less than VDD +0.3
	ads.setGain(ADS1115_PGA_6P144);

}

void loop()
{
	// define static variables
	static uint16_t	Vcc_mV; // nominal 5V supply  in mV
	static double	current_load,	current_charge, v_batt;
	static double	power_load,		power_charge;
	static double	energy_load = 0;
	static double	energy_charge = 0;
	static uint32_t time_delta_ms = 0;						// Time difference between last sample and now
	static uint32_t time_last_sample_ms = 0;	// When did we take the last sample?
	static uint32_t time_last_message = millis();					// Time last subscription message was sent.
	static const char source_load[] = "load";
	static const char source_charge[] = "charge";
	while(serial1_stream.available()) {
		aJsonObject *msg = aJson.parse(&serial1_stream);

	}
	if (readJSONRPCstring()) {
		// Do something if necessary
		// This is where we reset the energy_ variables
	}
	// Get data
	v_batt = getBatteryVotage();
	Vcc_mV = getADCreading(VCC_CHANNEL);	// Needs to be done before current is read.
	current_load = getCurrent(LOAD_CHANNEL, Vcc_mV);
	current_charge = getCurrent(CHARGE_CHANNEL, Vcc_mV);
	
	// Does power calculations. 
	time_delta_ms = (uint32_t)(millis() - time_last_sample_ms);	// Time between this sample and last.
	time_last_sample_ms = millis();
	power_load = getPower(v_batt, current_load);
	power_charge = getPower(v_batt, current_charge);
	energy_load += getEnergy(power_load, time_delta_ms);
	energy_charge += getEnergy(power_charge, time_delta_ms);
	if ((uint32_t)(millis() - time_last_message) >= (SUBSCRIPRION_RATE * 1000)) {
		time_last_message = millis();
		Serial.println("Load message");
		createJSON_subscription(v_batt, current_load,	power_load,		energy_load,	source_load);
		Serial.println("Charge message");
		createJSON_subscription(v_batt, current_charge,	power_charge,	energy_charge,	source_charge);
		sendJSONRPCsubString();
	}
	else {
		// Not time to report.
		delay(LOOP_DELAY_TIME_MS); // MAY BE WORTH LOOKING IN TO LOWERING POWER CONSUMPTION HERE
	}
}

void dataDebug(){
  // Print out values.
}

double	getBatteryVotage() {
	//Returns battery voltage in volts.
	double v_adc = (double)getADCreading(VBATT_CHANNEL)/1000.0;
	double battery_voltage = v_adc * V_DIVIDER;
	//char _buf[10]; 
	//dtostrf(battery_voltage, 6, 3, _buf);
	//Serial.print("Battery Voltage: ");
	//Serial.println(_buf);
	return battery_voltage;
}

double	getCurrent(const uint8_t channel, const uint16_t supply_mV) {
	//function returns current in Amps
	// To get current, subtract offset from reading. (Offset is supply_mV * 0.1) then divide the result by 133 to get amps.
	uint16_t current_mV = getADCreading(channel);
	if (current_mV >= (supply_mV /10)) current_mV -= (supply_mV /10);
	else current_mV = 0;
	double current_A = (double)current_mV / ACS715_mV_per_A;
	// DEBUG
	//char _buf[10];
	//dtostrf(current_A, 7, 4, _buf);
	//Serial.print("Channel "); Serial.print(channel); Serial.print(" current:");
	//Serial.println(_buf);
	// DEBUG ENDS
	return current_A;
}

uint16_t getADCreading(const uint8_t channel) {
	uint16_t mV;
	switch (channel) {
		case 0: ads.setMultiplexer(ADS1115_MUX_P0_NG); break;
		case 1: ads.setMultiplexer(ADS1115_MUX_P1_NG); break;
		case 2: ads.setMultiplexer(ADS1115_MUX_P2_NG); break;
		case 3: ads.setMultiplexer(ADS1115_MUX_P3_NG); break;
	}
	// Do conversion polling via I2C on this last reading: 
	mV = ads.getMilliVolts(true);
	// DEBUG
	//Serial.print("channel:"); Serial.print(channel); 
	//Serial.print(" , value: "); Serial.print(mV); Serial.println("mV");
	// DEBUG ENDS
	return mV;
}

double getPower(double voltage, double current_amps) {
	double power_W =  voltage * current_amps;
	return power_W;
}

double getEnergy(double power_watts, uint32_t time_delta) {
	double energy_Wh =  (power_watts * time_delta) / (float)MS_PER_HR;
	return energy_Wh;
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

/*

FOUND CODE:

double calcIrms(uint8_t NUMBER_OF_SAMPLES, uint8_t Channel, double supply_voltage) {
	int ADC_COUNTS = 65536 / 2; //15bit due to single ended.
	int ICAL = 100; //current constant = (100 / 0.050) / 20 = 100

	for (uint8_t n = 0; n < NUMBER_OF_SAMPLES; n++) {
		lastSampleI = sampleI;
		sampleI = ads.readADC_SingleEnded(Channel);
		lastFilteredI = filteredI;
		filteredI = 0.996 * (lastFilteredI + sampleI - lastSampleI);

		// Root-mean-square method current
		// 1) square current values
		sqI = filteredI * filteredI;
		// 2) sum
		sumI += sqI;
	}

	double I_RATIO = ICAL * ((supply_voltage) / (ADC_COUNTS));
	double Irms = I_RATIO * sqrt(sumI / NUMBER_OF_SAMPLES);

	//Reset accumulators
	sumI = 0;

	return Irms;
}

*/
