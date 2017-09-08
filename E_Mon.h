// E_Mon.h

#ifndef _E_MON_h
#define _E_MON_h
#include "broker_data.h"


/*
class ADCData is an abstract intermediate class for all data objects which get their data from the ADS1115 ADC
Always Read Only
*/
class ADCData : public DynamicData {
public:
	ADCData(const char *name, const char *unit, ADS1115 &ads, uint8_t ADCchannel, uint8_t resp_width, uint8_t resp_dec) : DynamicData(name, unit, true, resp_width, resp_dec) {
		_channel = ADCchannel; // should be 0-4
		_ads = &ads;
	}
	uint16_t getADCreading();
	uint8_t	getChannel() { return _channel; }
	double	getValue() { return _data_value; }
protected:
	ADS1115	*_ads;
private:
	uint8_t	_channel;
};

/*
class VoltageData is a class for all data objects which represent a voltage object
*/
class VoltageData : public ADCData {
public:
	VoltageData(const char *name, ADS1115 &ads, uint8_t ADCchannel, uint32_t high_div, uint32_t low_div, uint8_t resp_width, uint8_t resp_dec) : ADCData(name, "V", ads, ADCchannel, resp_width, resp_dec) {
		//_v_div = (high_div + low_div) / low_div;
		_high_div = high_div;
		_low_div = low_div;
	}
	double getData();
	bool	setData(double set_value) { return false; }
private:
	double	_v_div() { return (_high_div + _low_div) / _low_div; }
	double	_high_div;
	double	_low_div;
};
/*
class VoltageData2 : public ADCData {
public:
VoltageData2(const char *name, ADS1115 &ads, uint8_t ADCchannel, StaticData &high_div, StaticData &low_div) : ADCData(name, "V", ads, ADCchannel) {
_h_div = &high_div;
_l_div = &low_div;
}
double	getData();
bool	setData(double set_value) { return false; }
private:
StaticData *_h_div;
StaticData *_l_div;
//double	_v_div() { return (_high_div + _low_div) / _low_div; }
};
*/



/*
class CurrentData is a class for all data objects which represent a current object
*/
class CurrentData : public ADCData {
public:
	CurrentData(const char *name, ADS1115 &ads, VoltageData &vcc, uint8_t ADCchannel, double mV_per_A, uint8_t resp_width, uint8_t resp_dec) : ADCData(name, "A", ads, ADCchannel, resp_width, resp_dec) {
		_vcc = &vcc;
		_mV_per_A = mV_per_A;
	}
	double	getData();
	bool	setData(double set_value) { return false; }
private:
	double	_mV_per_A;
	VoltageData	*_vcc;
};

/*
class PowerData is a class for all data objects which represent a power object
Always Read Only
*/
class PowerData : public DynamicData {
public:
	PowerData(const char *name, CurrentData &current, VoltageData &voltage, uint8_t resp_width, uint8_t resp_dec) : DynamicData(name, "W", true, resp_width, resp_dec) {
		_voltage = &voltage;
		_current = &current;
	}
	double	getData();	// Calculates new Power based on most recent current and voltage
	bool	setData(double set_value);
	void	resetData();
	double	getValue() { return _data_value; }
private:
	CurrentData	*_current;
	VoltageData	*_voltage;
};

/*
class EnergyData is a class for all data objects which represent an energy object
*/
class EnergyData : public DynamicData {
public:
	EnergyData(const char *name, PowerData &power, uint8_t resp_width, uint8_t resp_dec) : DynamicData(name, "Wh", false, resp_width, resp_dec) {
		_power = &power;
	}
	double getData(); // Calculates new Energy based on most recent power
	bool	setData(double energy_value);	// Used for setting value, usually after reboot.
	bool	resetData() { return setData(0); }
	double	getValue() { return _data_value; }
private:
	PowerData	*_power;
};



#endif

