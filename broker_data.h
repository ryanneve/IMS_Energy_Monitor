#include "ADS1115.h"

#define BROKER_DATA_NAME_LENGTH 15
#define BROKER_DATA_UNIT_LENGTH 5
/*
	class BrokerData is an abstract class for all data objects
*/
class BrokerData {
public:
	BrokerData(char *name, char *unit) {
		strncpy(_data_name, name, BROKER_DATA_NAME_LENGTH);
		strncpy(_data_unit, unit, BROKER_DATA_UNIT_LENGTH);
		_subscription_rate = 0;
		_last_sample_time = NAN;
		_subscription_time = NAN;
		_data_value = NAN;
		_data_max = NAN;
		_data_min = NAN;
	};
	double	getValue() { return _data_value; }
	double	getMax() { return _data_max; }
	double	getMin() { return _data_min; }
	void	resetMin() { _data_min = NAN; }
	void	resetMax() { _data_max = NAN; }
	const char	*getName() { return _data_name; }
	const char	*getUnit() { return _data_unit; }
	void	subscribe(uint8_t sub_rate_s) { _subscription_rate = sub_rate_s;}
	void	unsubscribe() { _subscription_rate = 0; _subscription_time = NAN; }
	bool	isSubscribed() { return (bool)_subscription_rate; }
	bool	subscriptionDue();
	uint8_t		getSubscriptionRate() { return _subscription_rate; }
	void		setSubscriptionTime() { _subscription_time = millis(); } // Called when subscription is generated.
	uint32_t	getSampleTime() { return _last_sample_time; }
	virtual double getData() = 0;

protected:
	uint32_t	_getTimeDelta();
	uint8_t		_checkMinMax();
	bool		_setDataValue(double new_value); // handles seeing if value changed.
	double	_data_value;
	double	_data_max;
	double	_data_min;
private:
	char	_data_name[BROKER_DATA_NAME_LENGTH];
	char	_data_unit[BROKER_DATA_UNIT_LENGTH];
	uint8_t	_subscription_rate; // in seconds
	uint32_t	_subscription_time;	// Time of last subscription message
	bool		_data_changed;	// Set to true when data changes and to false when new value is reported in subscription
	uint32_t	_last_sample_time;	// Time of last sample
};

/*
class ADCData is an abstract intermediate class for all data objects which get their data from the ADS1115 ADC
*/
class ADCData : public BrokerData {
public:
	ADCData(char *name, char *unit, ADS1115 &ads, uint8_t ADCchannel) : BrokerData(name,unit) {
		_channel = ADCchannel; // should be 0-4
		_ads = &ads;
	}
	uint16_t getADCreading();
	uint8_t	getChannel() { return _channel; }
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
	VoltageData(char *name, ADS1115 &ads, uint8_t ADCchannel, uint32_t high_div, uint32_t low_div) : ADCData(name,"V",ads,ADCchannel) {
		_v_div = (high_div + low_div) / low_div;
	}
	double getData();
private:
	double	_v_div;
};
/*
class CurrentData is a class for all data objects which represent a current object
*/
class CurrentData: public ADCData {
public:
	CurrentData(char *name, ADS1115 &ads,VoltageData &vcc,uint8_t ADCchannel,double mV_per_A) : ADCData(name,"A",ads,ADCchannel) {
		_vcc = &vcc;
		_mV_per_A = mV_per_A;
	}
	double	getData();
private:
	double	_mV_per_A;
	VoltageData	*_vcc;
};

/*
class PowerData is a class for all data objects which represent a power object
*/
class PowerData : public BrokerData {
public:
	PowerData(char *name, CurrentData &current, VoltageData &voltage) : BrokerData(name,"W") {
		_voltage = &voltage;
		_current = &current;
	}
	double	getData();	// Calculates new Power based on most recent current and voltage
	void	resetData();
private:
	CurrentData	*_current;
	VoltageData	*_voltage;
};

/*
class EnergyData is a class for all data objects which represent an energy object
*/
class EnergyData : public BrokerData {
public:
	EnergyData(char *name, PowerData &power) : BrokerData(name,"Wh") {
		_power = &power;
}
	double getData(); // Calculates new Energy based on most recent power
	void	setData(double energy_value);	// Used for setting value, usually after reboot.
	void	resetData() { setData(0);}
private:
	PowerData	*_power;
};