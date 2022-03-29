#include "example_node.h"

using namespace std;
using namespace nlib;

ExampleNode::ExampleNode (int &argc, char **argv, const std::string &name, uint32_t options):
	 Base (argc, argv, name, options)
{
	init<ModFlow> ();

	sources()->declareSource<int> ("integer_source");
	sources()->declareSource<string> ("string_source");
	sinks()->declareSink ("publish_string", &ExampleNode::publishString, this);

	finalizeModFlow ();
}

void ExampleNode::initROS ()
{
	addSub ("string_in", 1, &ExampleNode::stringCallback);
	addPub<std_msgs::String> ("string_out", 1);
}

void ExampleNode::initParams ()
{}

void ExampleNode::stringCallback (const std_msgs::String &stringMsg)
{
	string value = stringMsg.data;

	sources()->callSource ("string_source", value);
}

void ExampleNode::onSynchronousClock (const ros::TimerEvent &timerEvent) {
	sources()->callSource ("integer_source", 1234);
}

void ExampleNode::publishString(const std::string &value)
{
	std_msgs::String stringMsg;
	stringMsg.data = value;

	publish ("string_out", stringMsg);
}

int main (int argc, char *argv[])
{
	//WAIT_GDB;
	ExampleNode en(argc, argv, "example_node");

	return en.spin ();
}
