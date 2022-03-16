#include "example_modflow.h"

using namespace nlib;
using namespace std;

void ExampleModFlow::loadModules()
{
	loadModule<Module1> ();
	loadModule<Module2> ();
	loadModule<Module3> ();
}

void Module1::setupNetwork ()
{
	createChannel<int> ("processed_integer");
	requestConnection ("integer_source", &Module1::processInteger);
}

void Module2::setupNetwork ()
{
	createChannel<string> ("processed_string");
	requestConnection ("string_source", &Module2::processString);
}

void Module3::setupNetwork()
{
	createChannel<string> ("finalized_string");
	requestConnection ("integer_source", &Module3::updateInteger);
	requestConnection ("processed_string", &Module3::updateString);
}

void ExampleSinks::setupNetwork ()
{
	connectToSink<std::string> ("finalized_string", "publish_string");
}

void Module1::initParams(const NlParams &nlParams)
{
	_params = {
		.integer = nlParams.get<int> ("integer"),
		.boolean = nlParams.get<bool> ("boolean")
	};
}

void Module2::initParams(const NlParams &nlParams)
{
	_params = {
		.stringParam = nlParams.get<string> ("string_param")
	};
}

void Module1::processInteger (int value)
{
	int processed = _seq + value * _params.integer;

	if (_seq % 2)
		emit ("processed_integer", processed);

	_seq++;
}

void Module2::processString(const std::string &value)
{
	std::string processed = value + _params.stringParam;

	emit ("processed_string", processed);
}

void Module3::updateInteger(int value) {
	_integer = value;
}

void Module3::updateString(const std::string &value) {
	_string = _string + value;
	
	emit ("finalized_string", _string + to_string (_integer));
}











