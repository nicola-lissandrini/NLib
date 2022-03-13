#ifndef NL_NODE_H
#define NL_NODE_H

#include <ros/ros.h>
#include <xmlrpcpp/XmlRpc.h>
#include "nl_utils.h"
#include "nl_params.h"
#include "nl_modflow.h"
#include <sstream>

namespace nlib {

template<class Derived>
struct traits;

template<class Derived>
class NlNode
{
	using DerivedModFlow = typename traits<Derived>::ModFlow;
	using DerivedSources = typename traits<Derived>::Sources;
	using DerivedSinks = typename traits<Derived>::Sinks;

public:
	NlNode (int &argc, char **argv, const std::string &_name, uint32_t options = 0);

	int spin ();

protected:
	void initModFlow ();
	void finalizeModFlow ();

	template<typename T>
	void addSub (const std::string &name,
			   const std::string &topic,
			   uint32_t queueSize,
			   void (Derived::*fp)(T),
			   const ros::TransportHints &transportHints = ros::TransportHints ());

	   // Add subscriber with topic names from standard params
	template<typename T>
	void addSub (const std::string &name,
			   uint32_t queueSize,
			   void (Derived::*fp)(T),
			   const ros::TransportHints &transportHints = ros::TransportHints ());

	template<typename T>
	void addPub (const std::string &name,
			   const std::string &topic,
			   uint32_t queueSize,
			   bool latch = false);

	   // Add publisher with topic names from standard params
	template<typename T>
	void addPub (const std::string &name,
			   uint32_t queueSize,
			   bool latch = false);

	   // Create publisher for output manager
	template<typename T>
	std::shared_ptr<ros::Publisher> createOutput (const std::string &topicPrefix,
										 const std::string &name,
										 uint32_t queueSize,
										 bool latch = false);

	template<typename T>
	void publish (const std::string &name,
			    const T &msg) const;

	void initParams ();
	void initROS ();

	typename DerivedSinks::Ptr sinks ();
	typename DerivedSources::Ptr sources ();
	Derived &derived () { return static_cast<Derived&> (*this); }
	const Derived &derived () const { return static_cast<const Derived &> (*this); }

private:
	std::string getStdTopic (const std::string &name, bool sub);

protected:
	std::shared_ptr<ros::NodeHandle> nh;
	NlParams params;
	int argc;
	char **argv;

private:
	NlModFlow::Ptr nlModFlow;

	std::map<std::string, ros::Publisher> publishers;
	std::map<std::string, ros::Subscriber> subscribers;
	ros::Timer clock;
	std::string name;
	bool synchronous;
};


template<typename OutputType>
class OutputManager
{
	using PubsMap = std::map<OutputType, std::shared_ptr<ros::Publisher>>;
	PubsMap outputPubs;

	template<typename RosMsg, typename DataType, typename ...ExtraArgs>
	void dataToMsg (RosMsg &outputMsg, const DataType &data, const ExtraArgs &...);
	template<typename DataType, typename RosMsg, typename ...ExtraArgs>
	void publish (const OutputType &id, const DataType &data, const ExtraArgs &...);

public:
	OutputManager () {}

	void addOutput (const OutputType &id, const std::shared_ptr<ros::Publisher> &publisher);
	template<typename DataType, typename ...ExtraArgs>
	void outputData (const OutputType &id, const DataType &data, const ExtraArgs &...extraArgs);

	typename PubsMap::iterator begin ();
	typename PubsMap::iterator end ();


	DEF_SHARED (OutputManager)
};

template<typename OutputType>
template<typename DataType, typename RosMsg, typename ...ExtraArgs>
void OutputManager<OutputType>::publish (const OutputType &id, const DataType &data, const ExtraArgs &...extraArgs)
{
	RosMsg outputMsg;
	dataToMsg (outputMsg, data, extraArgs...);
	outputPubs[id].publish (outputMsg);
}

template<typename  OutputType>
void OutputManager<OutputType>::addOutput (const OutputType &id, const std::shared_ptr<ros::Publisher> &publisher) {
	outputPubs[id] = publisher;
}

template<typename OutputType>
typename OutputManager<OutputType>::PubsMap::iterator
    OutputManager<OutputType>::begin () {
	return outputPubs.begin ();
}

template<typename OutputType>
typename OutputManager<OutputType>::PubsMap::iterator
    OutputManager<OutputType>::end () {
	return outputPubs.end ();
}

#define NL_NODE(Derived) \
using Base = nlib::NlNode<Derived>;\
    friend Base; \
    using Base::initModFlow; \
    using Base::finalizeModFlow; \
    using Base::sinks; \
    using Base::sources; \
    public:\
    using Base::spin; \
    private: \


		    template<class Derived>
		    NlNode<Derived>::NlNode (int &_argc, char **_argv, const std::string &_name, uint32_t options):
	 name(_name),
	 argc(_argc),
	 argv(_argv)
{
	ros::init (argc, argv, name, options);

	nh = std::make_shared<ros::NodeHandle> ();

	initParams ();
	initROS ();
}

template<class Derived>
template<typename T>
void NlNode<Derived>::publish (const std::string &name,
						 const T &msg) const {
	return publishers.at (name).publish (msg);
}


template<class Derived>
template<typename T>
void NlNode<Derived>::addSub (const std::string &name,
						const std::string &topic,
						uint32_t queueSize,
						void (Derived::*fp)(T),
						const ros::TransportHints &transportHints) {
	subscribers.insert ({name, nh->subscribe (topic, queueSize, fp, &derived(), transportHints )});
}

template<class Derived>
template<typename T>
void NlNode<Derived>::addSub (const std::string &name,
						uint32_t queueSize,
						void (Derived::*fp)(T),
						const ros::TransportHints &transportHints) {
	addSub (name, getStdTopic (name, true), queueSize, fp, transportHints);
}

template<class Derived>
template<typename T>
void NlNode<Derived>::addPub (const std::string &name,
						const std::string &topic,
						uint32_t queueSize,
						bool latch) {
	publishers.insert ({name, nh->advertise<T> (topic, queueSize, latch)});
}

template<class Derived>
template<typename T>
void NlNode<Derived>::addPub (const std::string &name,
						uint32_t queueSize,
						bool latch) {
	addPub<T> (name, getStdTopic (name, false), queueSize, latch);
}

template<class Derived>
template<typename T>
std::shared_ptr<ros::Publisher> NlNode<Derived>::createOutput (const std::string &topicPrefix,
												   const std::string &name,
												   uint32_t queueSize,
												   bool latch)
{
	return std::make_shared<ros::Publisher> (
	    nh->advertise<T> (topicPrefix + "/" + name, queueSize, latch));
}

template<class Derived>
std::string NlNode<Derived>::getStdTopic (const std::string &name, bool sub) {
	try {
		std::stringstream pathSS;
		pathSS << "topics/" << name << "_" << (sub ? "sub" : "pub");
		return params.get<std::string> (pathSS.str ().c_str());
	} catch (const XmlRpc::XmlRpcException &) {
		std::stringstream pathSS;
		pathSS << "topics/" << (sub ? "subs/" : "pubs/") << name;
		return params.get<std::string> (pathSS.str ().c_str());
	}
}

template<class Derived>
void NlNode<Derived>::initParams ()
{
	if (nh->hasParam (name)) {
		XmlRpc::XmlRpcValue xmlParams;

		nh->getParam (name, xmlParams);

		if (xmlParams.begin () != xmlParams.end ())
			params = xmlParams;

	}

	   // If no params are loaded, nlParams is empty
	   // Every trial of getting a param from nlParams will result in an exception
}

template<class Derived>
void NlNode<Derived>::initROS ()
{
	try {
		float clockPeriod = 1 / params.get<float> ("rate");

		clock = nh->createTimer (ros::Duration(clockPeriod), &Derived::onSynchronousClock, &derived(), false, false);

		synchronous = true;
	} catch (const XmlRpc::XmlRpcException &e) {
		synchronous = false;
	}
}

template<class Derived>
typename NlNode<Derived>::DerivedSinks::Ptr
    NlNode<Derived>::sinks() {
	return nlModFlow->sinks<DerivedSinks> ();
}

template<class Derived>
typename NlNode<Derived>::DerivedSources::Ptr
    NlNode<Derived>::sources() {
	return nlModFlow->sources<DerivedSources> ();
}

template<class Derived>
void NlNode<Derived>::initModFlow ()
{
	nlModFlow = std::make_shared<DerivedModFlow> ();

	try {
		derived().initParams ();
		derived().initROS ();
		nlModFlow->init<DerivedSources, DerivedSinks> (params);

	} catch (const XmlRpc::XmlRpcException &e) {
		ROS_ERROR_STREAM(e.getMessage ());
	}
}

template<class Derived>
void NlNode<Derived>::finalizeModFlow () {
	try {
		nlModFlow->finalize ();
	} catch (const XmlRpc::XmlRpcException &e) {
		ROS_ERROR_STREAM(e.getMessage ());
	}
}

template<class Derived>
int NlNode<Derived>::spin ()
{
	ros::AsyncSpinner spinner(2);

	spinner.start ();

	if (synchronous)
		clock.start ();

	ros::waitForShutdown ();

	return 0;
}








}

#endif // NL_NODE_H
