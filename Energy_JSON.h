
// include the aJSON library
#include <aJSON.h>
// include the JsonRPC library
#include <JsonRPCServer.h>

enum rw_data {
	ENERGY_LOAD,
	ENERGY_CHARGE
};

class TargetController: public JsonRPCServer {
public:
	TargetController(Stream* stream);

	DECLARE_JSON_PROC(TargetController, set_data, bool);
	DECLARE_JSON_PROC(TargetController, initialize, void);
	DECLARE_JSON_PROC(TargetController, status,		String);
	DECLARE_JSON_PROC(TargetController, list_data,	void);
	

	BEGIN_JSON_REGISTRATION
		REGISTER_JSON_PROC(set_data, JSON_RPC_RET_TYPE_BOOL);
		REGISTER_JSON_PROC(initialize, JSON_RPC_RET_TYPE_NONE);
		REGISTER_JSON_PROC(status,		JSON_RPC_RET_TYPE_STRING);
		REGISTER_JSON_PROC(list_data,	JSON_RPC_RET_TYPE_NONE);
	END_JSON_REGISTRATION
	

private:
	
	// on most arduino boards, pin 13 is connected to a LED
	int _led;
	
};


void createJSON_subscription(double v_batt, double current, double power, double energy, const char* source_str);


int freeRAM();

void printFreeRam(const char* msg);
