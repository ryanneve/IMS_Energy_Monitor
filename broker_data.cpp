
#include <Arduino.h>
#include "ADS1115.h"
#include "broker_data.h"
//// Define Objects
//ADS1115 ads(ADS1115_ADDRESS);



uint16_t ADCData::getADCreading() {
	uint16_t mV;
	switch (getChannel()) {
	case 0: _ads->setMultiplexer(ADS1115_MUX_P0_NG); break;
	case 1: _ads->setMultiplexer(ADS1115_MUX_P1_NG); break;
	case 2: _ads->setMultiplexer(ADS1115_MUX_P2_NG); break;
	case 3: _ads->setMultiplexer(ADS1115_MUX_P3_NG); break;
	}
	// Do conversion polling via I2C on this last reading: 
	mV = _ads->getMilliVolts(true);
	// DEBUG
	//Serial.print("channel:"); Serial.print(channel);
	//Serial.print(" , value: "); Serial.print(mV); Serial.println("mV");
	// DEBUG ENDS
	return mV;
}


void	CurrentData::setVcc(double vcc) {
	_Vcc = vcc;
}
double	CurrentData::getData() {
	//method returns current in Amps
	// To get current, subtract offset from reading. (Offset is supply_mV * 0.1) then divide the result by 133 to get amps.
	double Vcc_mV = _Vcc/1000.0;
	uint16_t current_mV = getADCreading();
	if (current_mV >= (Vcc_mV / 10.0)) current_mV -= (Vcc_mV / 10.0);
	else current_mV = 0;
	double current_A = (double)current_mV / _mV_per_A;
	// May want to check if data seems valid 
	_data_value = current_A;
	_sample_time = millis();
	if (_data_value > _data_max) _data_max = _data_value;
	if (_data_value < _data_min) _data_min = _data_value;
	return _data_value;
}


double VoltageData::getData() {
	//method returns voltage in volts
	uint16_t voltage_mV = getADCreading();
	voltage_mV *= _v_div;
	// May want to check if data seems valid 
	_data_value = (double)voltage_mV / 1000.0;
	_sample_time = millis();
	if (_data_value > _data_max) _data_max = _data_value;
	if (_data_value < _data_min) _data_min = _data_value;
	return _data_value;
}

double PowerData::getData() {

}

double EnergyData::getData() {

}

void EnergyData::setdata(double energy_value) {

}

double EnergyData::addData(double power_value) {

}
