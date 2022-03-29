#ifndef EXAMPLE_NODE_H
#define EXAMPLE_NODE_H

#include <nlib/nl_node.h>
#include <std_msgs/String.h>

#include "example_modflow.h"


class ExampleNode : public nlib::NlNode<ExampleNode>
{
	NL_NODE(ExampleNode)

	using ModFlow = ExampleModFlow;

public:
	ExampleNode (int &argc, char **argv, const std::string &name, uint32_t options = 0);

	void initROS ();
	void initParams ();

	void stringCallback (const std_msgs::String &stringMsg);
	void publishString (const std::string &value);

	DEF_SHARED (ExampleNode)

protected:
	void onSynchronousClock (const ros::TimerEvent &timerEvent);
};


#endif // EXAMPLE_NODE_H
