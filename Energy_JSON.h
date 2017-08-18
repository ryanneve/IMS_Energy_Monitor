
// include the aJSON library
#include <aJSON.h>
// include the JsonRPC library
#include <JsonRPCServer.h>

#define JSON_RPC_PROCEDURES 6 // Number of procedures

class TargetController: public JsonRPCServer {
public:
	TargetController(Stream* stream);

	DECLARE_JSON_PROC(TargetController, status, String);
	DECLARE_JSON_PROC(TargetController, subscribe, void);
	DECLARE_JSON_PROC(TargetController, unsubscribe, void);
	DECLARE_JSON_PROC(TargetController, set_data, bool);
	DECLARE_JSON_PROC(TargetController, list_data,	void);
	DECLARE_JSON_PROC(TargetController, broker_status, void);

	BEGIN_JSON_REGISTRATION
		REGISTER_JSON_PROC(status, JSON_RPC_RET_TYPE_STRING);
		REGISTER_JSON_PROC(subscribe, JSON_RPC_RET_TYPE_NONE);
		REGISTER_JSON_PROC(unsubscribe, JSON_RPC_RET_TYPE_NONE);
		REGISTER_JSON_PROC(set_data, JSON_RPC_RET_TYPE_BOOL);
		REGISTER_JSON_PROC(list_data,	JSON_RPC_RET_TYPE_NONE);
		REGISTER_JSON_PROC(broker_status, JSON_RPC_RET_TYPE_NONE);
	END_JSON_REGISTRATION
private:
	
};
