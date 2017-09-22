
#include <Arduino.h>
#include "broker_data.h"
//// Define Objects


void BrokerData::dataToStr(char * out_str) {
	dtostrf(_data_value, _resp_width, _resp_dec, out_str);
}


bool StaticData::setData(double new_value) {
	_data_value = new_value;
	getSampleTimeStr(_last_sample_time_str);
	return true;

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



void getSampleTimeStr(char splTimeStr[15],bool islong) { // CCYYMMDDHHmmss\0
	/* Set date_time string to current date and time
	long: "2017-09-06 11:24:23.96"
	*/
	if (islong) snprintf(splTimeStr, 15, "%4u-%02u-%02u %02u:%02u:%02u.00", year(), month(), day(), hour(), minute(), second());
	else snprintf(splTimeStr, 15,"%4u%02u%02u%02u%02u%02u", year(), month(), day(), hour(), minute(), second());
}
