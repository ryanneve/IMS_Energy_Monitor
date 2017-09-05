

#ifndef _BROKER_DATA_h
#define _BROKER_DATA_h
#include "ADS1115.h"

#define BROKER_DATA_NAME_LENGTH 15
#define BROKER_DATA_UNIT_LENGTH 5


# define STAT_VAL_WIDTH 5	// Used for converting double to string
# define STAT_VAL_PREC  3	// Used for converting double to string

/*
	class BrokerData is an abstract class for all data objects
*/
class BrokerData {
public:
	BrokerData(const char *name, const char *unit,bool ro,uint8_t resp_width,uint8_t resp_dec) {
		strncpy(_data_name, name, BROKER_DATA_NAME_LENGTH);
		strncpy(_data_unit, unit, BROKER_DATA_UNIT_LENGTH);
		_ro = ro;
		_resp_width = max(resp_width, resp_dec);
		_resp_dec = resp_dec;
	};
	const char	*getName() { return _data_name; }
	const char	*getUnit() { return _data_unit; }
	bool		isRO() { return _ro; }
	char *	getSplTimeStr() { return _last_sample_time_str; }
	void	dataToStr(char * out_str);
	// virtual methods
	virtual bool	subscriptionDue() { return false; }
	virtual bool	isVerbose() { return false; }
	virtual double	getMax() { return getData(); }
	virtual double	getMin() { return getData(); }
	virtual uint32_t	getSampleTime() { return 0; }
	virtual void	unsubscribe() {};
	virtual void	subscribe(uint32_t, uint32_t) {};
	virtual void	setSubscriptionTime() {};
	virtual void	setSubOnChange(bool) {};
	virtual void	setVerbose(bool) {};
	virtual void	resetMin() {};
	virtual void	resetMax() {};
	// Pure virtual methods
	virtual double	getValue() = 0;
	virtual double getData() = 0;
	virtual bool setData(double set_value) = 0;

protected:
	bool	_dynamic;
	double	_data_value;
	char	_last_sample_time_str[15]; // string representing time of last sample
	uint8_t	_resp_width;		// dtostrf() width
	uint8_t	_resp_dec;			// dtostrf() decimal places
private:
	char	_data_name[BROKER_DATA_NAME_LENGTH];
	char	_data_unit[BROKER_DATA_UNIT_LENGTH];
	bool	_ro;	// Read Only
};


/*
class StaticData is an class for all data objects which only change based on messages.
*/
class StaticData : public BrokerData {
public:
	StaticData(const char *name, const char *unit,double initial_value, uint8_t resp_width, uint8_t resp_dec) : BrokerData(name, unit, false, resp_width, resp_dec) {
		_dynamic = false;
		setData(initial_value);
	}
	double	getData() {return getValue();}
	bool setData(double set_value);
	double	getValue() { return _data_value; }
protected:
private:
};


/*
class DynamicData is an abstract intermediate class for all data objects which have data_values which change based on external readings or calculations.
*/
class DynamicData : public BrokerData {
public:
	DynamicData(const char *name, const char *unit, bool ro, uint8_t resp_width, uint8_t resp_dec) : BrokerData(name, unit, ro, resp_width, resp_dec) {
		_dynamic = true;
		_subscription_rate_ms = 0; // 0  is not subscribed
		_last_sample_time = 0;
		_subscription_time = 0;
		_sub_verbose = true;
		_sub_on_change = true;
		_data_value = NAN;
		_data_max = NAN;
		_data_min = NAN;
	}
	double	getMax() { return _data_max; }
	double	getMin() { return _data_min; }
	void	resetMin() { _data_min = NAN; }
	void	resetMax() { _data_max = NAN; }
	uint32_t	getSampleTime() { return _last_sample_time; }
	void	subscribe(uint32_t sub_min_rate_ms, uint32_t sub_max_rate_ms);
	void	setSubOnChange(bool on_change) { _sub_on_change = on_change; }
	bool	isOnChange() { return _sub_on_change; }
	void	subscribe(uint32_t sub_rate_ms) { subscribe(sub_rate_ms, 0); } // _subscription_max_ms defaults to 0 
	void	unsubscribe() { _subscription_rate_ms = 0; _subscription_time = 0; }
	bool	isSubscribed() { return (bool)_subscription_rate_ms; }
	bool	subscriptionDue();
	uint32_t	getSubscriptionRate() { return _subscription_rate_ms; }
	void		setSubscriptionTime() { _subscription_time = millis(); } // Called when subscription is generated.
	bool		hasDataChanged() { return _data_changed; }
	void		setVerbose(bool verbose) { _sub_verbose = verbose; }
	bool		isVerbose() { return _sub_verbose; }
protected:
	bool	_setDataValue(double new_value) ;
	uint8_t		_checkMinMax();  	//Checks if there is a new min and max. Returns:
	uint32_t	_getTimeDelta();	// Records current sample time, and returns time since last sample in ms.
	double	_data_max;
	double	_data_min;
private:
	uint32_t	_subscription_rate_ms; // in milli-seconds
	uint32_t	_subscription_max_ms; // Used on-change
	uint32_t	_subscription_time;	// Time of last subscription message from millis()
	uint32_t	_last_sample_time;	// Time of last sample from millis()
	bool		_data_changed;	// Set to true when data changes and to false when new value is reported in subscription
	bool	_sub_verbose;	// is this parameter's subscription verbose or terse?
	bool	_sub_on_change; // report only new values when True
};



/*
class ADCData is an abstract intermediate class for all data objects which get their data from the ADS1115 ADC
Always Read Only
*/
class ADCData : public DynamicData {
public:
	ADCData(const char *name, const char *unit, ADS1115 &ads, uint8_t ADCchannel, uint8_t resp_width, uint8_t resp_dec) : DynamicData(name,unit,true,resp_width,resp_dec) {
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
	VoltageData(const char *name, ADS1115 &ads, uint8_t ADCchannel, uint32_t high_div, uint32_t low_div,  uint8_t resp_width, uint8_t resp_dec) : ADCData(name,"V",ads,ADCchannel,resp_width,resp_dec) {
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
class CurrentData: public ADCData {
public:
	CurrentData(const char *name, ADS1115 &ads,VoltageData &vcc,uint8_t ADCchannel,double mV_per_A, uint8_t resp_width, uint8_t resp_dec) : ADCData(name,"A",ads,ADCchannel, resp_width, resp_dec) {
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
	PowerData(const char *name, CurrentData &current, VoltageData &voltage,uint8_t resp_width,uint8_t resp_dec) : DynamicData(name,"W",true, resp_width, resp_dec) {
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
	EnergyData(const char *name, PowerData &power, uint8_t resp_width, uint8_t resp_dec) : DynamicData(name,"Wh",false, resp_width, resp_dec) {
		_power = &power;
}
	double getData(); // Calculates new Energy based on most recent power
	bool	setData(double energy_value);	// Used for setting value, usually after reboot.
	bool	resetData() { return setData(0);}
	double	getValue() { return _data_value; }
private:
	PowerData	*_power;
};
/*Holds a date or a time. Figures out which based on value and acts accordingly*/
class TimeData : public DynamicData{
public:
	TimeData(const char *name, bool is_a_date, uint8_t resp_width, uint8_t resp_dec) :DynamicData(name, (is_a_date?"CCYYMMDD":"HHmmss"), false, resp_width, resp_dec) {
		_is_date = is_a_date;
	}
	double getData();
	double getValue() { return getData(); }
	bool setData(double date_or_time);
private:
	bool _is_date;
};


void getSampleTimeStr(char *splTimeStr);

#endif