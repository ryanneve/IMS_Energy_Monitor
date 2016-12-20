


class TargetController: public JsonRPCServer {
public:
	TargetController(Stream* stream);

	DECLARE_JSON_PROC(TargetController, init, int);
	DECLARE_JSON_PROC(TargetController, toggleLED, String);
	

	BEGIN_JSON_REGISTRATION
		REGISTER_JSON_PROC(init, JSON_RPC_RET_TYPE_NUMERIC);
		REGISTER_JSON_PROC(toggleLED, JSON_RPC_RET_TYPE_STRING);
	END_JSON_REGISTRATION
	

private:
	
	// on most arduino boards, pin 13 is connected to a LED
	int _led;
	
};
