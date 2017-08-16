#include "ADS1115.h"

#define BROKERDATANAMELENGTH 30

class BrokerData {
public:
	BrokerData(char *name) {
		strncpy(_data_name, name, BROKERDATANAMELENGTH);
		_subscribed = false;
		_sample_time = 0;
		_subscription_time = 0;
		_data_value = NAN;
		_data_max = NAN;
		_data_min = NAN;
	};
	double	getvalue() { return _data_value; }
	double	getMax() { return _data_max; }
	double	getMin() { return _data_min; }
	const char	*getName() { return _data_name; }
	void	subscribe() { _subscribed = true; }
	void	unsubscribe() { _subscribed = false; }
	bool	isSubscribed() { return _subscribed; }
	uint32_t	getSampleTime() { return _sample_time; }

protected:
	double	_data_value;
	double	_data_max;
	double	_data_min;
	uint32_t	_sample_time;	// Time of last sample
	uint32_t	_subscription_time;	// Time of last subscription message
private:
	char	_data_name[BROKERDATANAMELENGTH]; // Shorter?
	bool	_subscribed;
};

class ADCData : public BrokerData {
public:
	ADCData(char *name, ADS1115 &ads, uint8_t ADCchannel) : BrokerData(name) {
		_channel = ADCchannel; // should be 0-4
		_ads = &ads;
	}
	uint16_t getADCreading();
	uint8_t	getChannel() { return _channel; }
protected:
	uint8_t	_channel;
	ADS1115	*_ads;
};


class CurrentData: public ADCData {
public:
	CurrentData(char *name, ADS1115 &ads, uint8_t ADCchannel, double mV_per_A) : ADCData(name, ads, ADCchannel) {
		_mV_per_A = mV_per_A;
	}
	void	setVcc(double vcc);
	double	getData();
private:
	double	_mV_per_A;
	double	_Vcc;	// Nominally 5.0 volts
};

class VoltageData : public ADCData {
public:
	VoltageData(char *name, ADS1115 &ads, uint8_t ADCchannel,uint32_t high_div, uint32_t low_div) : ADCData(name,ads,ADCchannel) {
		_v_div = (high_div + low_div) / low_div;
	}
	double getData();
private:
	double	_v_div;
};

class PowerData : public BrokerData {
public:
	PowerData(char *name,CurrentData &current, VoltageData &voltage) : BrokerData(name) {
		_voltage = &voltage;
		_current = &current;
	}
	double	getData();
	void	resetData() { _data_value = 0; }
private:
	CurrentData	*_current;
	VoltageData	*_voltage;
};

class EnergyData : public BrokerData {
public:
	EnergyData(char *name,PowerData &power) : BrokerData(name) {
		_power = &power;
}
	double getData(); // Calculates new Energy based on current power
	void	setdata(double energy_value);	// Used for setting value, usually after reboot.
	double	addData(double power_value);	// called often with new power values
	void	resetData() { _data_value = 0; }
private:
	PowerData	*_power;
};

// Now non-class functions