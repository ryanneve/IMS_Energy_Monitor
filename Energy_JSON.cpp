


TargetController::TargetController(Stream* stream) : JsonRPCServer(stream), _led(13) {
	// TODO Auto-generated constructor stub

}

int TargetController::init(aJsonObject* params) {

	// initialize the digital pin as an output
	pinMode(_led, OUTPUT);

	return true;
}

String TargetController::toggleLED(aJsonObject* params)
{
	aJsonObject* statusParam = aJson.getObjectItem(params, "status");
	boolean requestedStatus = statusParam->valuebool;

	if (requestedStatus)
	{
		digitalWrite(_led, HIGH);
		return "led HIGH";
	}
	else
	{
		digitalWrite(_led, LOW);
		return "led LOW";
	}
}
