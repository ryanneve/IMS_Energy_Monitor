/* Functions specific to the Energry Monitoring application*/

#include "E_Mon.h"

#define MS_PER_HR 3600000


uint16_t ADCData::getADCreading() {
	uint32_t pin_value = _adc->analogRead(_channel,ADC_0);
	uint32_t max_value = _adc->getMaxValue(ADC_0);
	uint16_t mV = 3300 * ((double)pin_value/ (double)max_value);
	_getTimeDelta();
	//Serial.print("pin: "); Serial.print(_channel);
	//Serial.print(" value: "); Serial.print(pin_value);
	//Serial.print("/"); Serial.print(max_value);
	//Serial.print(",in mV = "); Serial.println(mV);
	return mV;
}


double	CurrentData::getData() {
	//method returns current in Amps
	// To get current, subtract offset from reading. (Offset is supply_mV * 0.1) then divide the result by 133 to get amps.
	double Vcc_mV = 3.3;
	uint16_t current_mV = getADCreading();
	if (current_mV >= (Vcc_mV / 10.0)) current_mV -= (Vcc_mV / 10.0);
	else current_mV = 0;
	double current_A = (double)current_mV / _mV_per_A;
	// May want to check if data seems valid 
	_setDataValue(current_A);
	_getTimeDelta();
	_checkMinMax();
	return _data_value;
}


double VoltageData::getData() {
	//method returns voltage in volts
	uint16_t voltage_mV = getADCreading();
	voltage_mV *= _v_div();
	// May want to check if data seems valid 
	_setDataValue((double)voltage_mV / 1000.0);
	_checkMinMax();
	return _data_value;
}


double PowerData::getData() {
	// Power is voltage times current. We have _voltage and _current.
	// MAYT WANT TO MAKE SURE VOLTAGE AND CURRENT DATA ISN'T TOO OLD.
	_setDataValue(_voltage->getValue() * _current->getValue());
	_checkMinMax();
	_getTimeDelta();
	return _data_value;
}


bool PowerData::setData(double set_value) {
	if (set_value == 0) {
		resetData();
		return true;
	}
	else return false;
}

void PowerData::resetData() {
	_data_value = 0;
	resetMin();
	resetMax();
}

double EnergyData::getData() {
	_setDataValue((_power->getValue() * (double)_getTimeDelta()) / (double)MS_PER_HR);
	return _data_value;
}
bool EnergyData::setData(double power_Wh) {
	// In certain instances, like after a reboot, _data_value should be initialized to a non-zero value.
	_setDataValue(power_Wh);
	_getTimeDelta();
	return true;
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