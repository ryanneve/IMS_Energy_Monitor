
#include <Arduino.h>
#include <timelib.h>
#include "ADS1115.h"
#include "broker_data.h"
//// Define Objects

#define MS_PER_HR 3600000


void BrokerData::dataToStr(char * out_str) {
	dtostrf(_data_value, _resp_width, _resp_dec, out_str);
}

void DynamicData::subscribe(uint32_t sub_min_rate_ms, uint32_t sub_max_rate_ms) {
	_subscription_rate_ms = sub_min_rate_ms;
	_subscription_max_ms = sub_max_rate_ms;
	_subscription_time = 0; // Should sync items already subscribed.
}


bool DynamicData::subscriptionDue() {
	/* Used to determine if it's time to report a subscription*/
	bool report_value = false;
	if (_subscription_rate_ms == 0) return false; // Not subscribed
	else { // subscribed
		if (isOnChange()) {
			// only report if value has changed or we have exceeded _subscription_max_ms
			if (_data_changed && ((millis() - _subscription_time) > _subscription_rate_ms)) report_value = true;
			else if ((millis() - _subscription_time) > _subscription_max_ms) report_value =  true;
		}
		else { // it's "on new", so it's purely time based.
			if ((millis() - _subscription_time) > _subscription_rate_ms) report_value = true;
		}
	}
	if (report_value) _data_changed = false; // indicate that this value has been reported via subscription
	return report_value;
}

uint32_t DynamicData::_getTimeDelta() {
	// Records current sample time, and returns time since last sample in ms.
	uint32_t current_sample_time = millis();
	// Works with millis() roll over since we are using unsigned long
	uint32_t time_delta = current_sample_time - _last_sample_time;
	_last_sample_time = current_sample_time;
	getSampleTimeStr(_last_sample_time_str);
	return time_delta;
}


uint8_t DynamicData::_checkMinMax() {
	//Checks if there is a new min and max. Returns:
	// 0 = no changes
	// 1 = New min
	// 2 = new max
	// 3 = new min & max (usally first value)
	uint8_t newMinMax = 0;
	if (!isnan(_data_value)) {
		if ((_data_value > _data_max) | isnan(_data_max)) {
			_data_max = _data_value;
			newMinMax +=2;
		}
		if ((_data_value < _data_min) | isnan(_data_min)) {
			_data_min = _data_value;
			newMinMax +=1;
		}
	}
	return newMinMax;
}

bool StaticData::setData(double new_value) {
	_data_value = new_value;
	getSampleTimeStr(_last_sample_time_str);
	return true;

}
bool DynamicData::_setDataValue(double new_value) {
	// See if value has changed.
	if (new_value == _data_value) {
		return false;
	}
	else {
		_data_value = new_value;
		_data_changed = true;
		getSampleTimeStr(_last_sample_time_str);
		_last_sample_time = millis();
		return true;
	}

}
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
	_getTimeDelta();
	// DEBUG
	//Serial.print("channel:"); Serial.print(channel);
	//Serial.print(" , value: "); Serial.print(mV); Serial.println("mV");
	// DEBUG ENDS
	return mV;
}


double	CurrentData::getData() {
	//method returns current in Amps
	// To get current, subtract offset from reading. (Offset is supply_mV * 0.1) then divide the result by 133 to get amps.
	double Vcc_mV = _vcc->getData()/1000.0;
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

/*
double VoltageData2::getData() {
	//method returns voltage in volts
	uint16_t voltage_mV = getADCreading();
	double _v_div = (_h_div->getData() + _l_div->getData())/ _l_div->getData();
	voltage_mV *= _v_div;
	// May want to check if data seems valid 
	_setDataValue((double)voltage_mV / 1000.0);
	_checkMinMax();
	return _data_value;
}
*/

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


double TimeData::getData() {
	double data_out;
	if (_is_date) {
		data_out = (double)year() * 10000;
		data_out += ((double)month() * 100);
		data_out += (double)day();
	}
	else {
		data_out = (double)(hour()) * 10000;
		data_out += (double)(minute()) * 100;
		data_out += (double)(second());
	}
	_data_value = data_out;
	return _data_value;
}

bool TimeData::setData(double date_or_time) {
	/* date_or_time format is ccyymmdd or hhmmss
	Could probably check for valid values before setting.
	*/
	//Serial.print("Processing: "); Serial.println((uint32_t)date_or_time);
	uint16_t ccyy;
	uint8_t MM, DD, HH, mm, ss;
	_data_value = date_or_time;
	if (_is_date) {
		// it's a date
		ccyy = date_or_time/10000;
		date_or_time -= (ccyy * 10000);
		MM = date_or_time / 100;
		DD = date_or_time - (MM * 100);
		HH = hour();
		mm = minute();
		ss = second();
	}
	else {
		// it's a time
		ccyy = year();
		MM = month();
		DD = day();
		HH = date_or_time / 10000;
		date_or_time -= (HH * 10000);
		mm = date_or_time / 100;
		ss = date_or_time - (mm * 100);
	}
	//Serial.print("CCYY: "); Serial.println(ccyy);
	//Serial.print("MM: "); Serial.println(MM);
	//Serial.print("DD: "); Serial.println(DD);
	//Serial.print("HH: "); Serial.println(HH);
	//Serial.print("mm: "); Serial.println(mm);
	//Serial.print("ss: "); Serial.println(ss);
	setTime(HH, mm, ss, DD, MM, ccyy);
	return true;
}



void getSampleTimeStr(char splTimeStr[15]) { // CCYYMMDDHHmmss\0
	/* Set date_time string to current date and time
	*/
	snprintf(splTimeStr, 15,"%4u%02u%02u%02u%02u%02u", year(), month(), day(), hour(), minute(), second());
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