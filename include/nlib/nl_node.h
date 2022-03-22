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
	void init ();
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

	std::shared_ptr<DerivedSinks> sinks ();
	std::shared_ptr<DerivedSources> sources ();
	Derived &derived () { return static_cast<Derived&> (*this); }
	const Derived &derived () const { return static_cast<const Derived &> (*this); }

private:
	std::string getStdTopic (const std::string &name, bool sub);

protected:
	std::shared_ptr<ros::NodeHandle> _nh;
	NlParams _nlParams;
	int _argc;
	char **_argv;

private:
	NlModFlow::Ptr _nlModFlow;
	std::map<std::string, ros::Publisher> _publishers;
	std::map<std::string, ros::Subscriber> _subscribers;
	ros::Timer _clock;
	std::string _name;
	bool _synchronous;
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
    using Base::init; \
    using Base::finalizeModFlow; \
    using Base::sinks; \
    using Base::sources; \
    public:\
    using Base::spin; \
    private: \


		    template<class Derived>
		    NlNode<Derived>::NlNode (int &_argc, char **_argv, const std::string &_name, uint32_t options):
	 _name(_name),
	 _argc(_argc),
	 _argv(_argv)
{
	ros::init (_argc, _argv, _name, options);

	_nh = std::make_shared<ros::NodeHandle> ();

	initParams ();
	initROS ();
}

template<class Derived>
template<typename T>
void NlNode<Derived>::publish (const std::string &name,
						 const T &msg) const {
	return _publishers.at (name).publish (msg);
}


template<class Derived>
template<typename T>
void NlNode<Derived>::addSub (const std::string &name,
						const std::string &topic,
						uint32_t queueSize,
						void (Derived::*fp)(T),
						const ros::TransportHints &transportHints) {
	_subscribers.insert ({name, _nh->subscribe (topic, queueSize, fp, &derived(), transportHints )});
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
	_publishers.insert ({name, _nh->advertise<T> (topic, queueSize, latch)});
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
	    _nh->advertise<T> (topicPrefix + "/" + name, queueSize, latch));
}

template<class Derived>
std::string NlNode<Derived>::getStdTopic (const std::string &name, bool sub) {
	try {
		std::stringstream pathSS;
		pathSS << "topics/" << name << "_" << (sub ? "sub" : "pub");
		return _nlParams.get<std::string> (pathSS.str ().c_str());
	} catch (const XmlRpc::XmlRpcException &) {
		std::stringstream pathSS;
		pathSS << "topics/" << (sub ? "subs/" : "pubs/") << name;
		return _nlParams.get<std::string> (pathSS.str ().c_str());
	}
}

template<class Derived>
void NlNode<Derived>::initParams ()
{
	if (_nh->hasParam (_name)) {
		XmlRpc::XmlRpcValue xmlParams;

		_nh->getParam (_name, xmlParams);

		if (xmlParams.begin () != xmlParams.end ())
			_nlParams = xmlParams;

	}

	   // If no params are loaded, nlParams is empty
	   // Every trial of getting a param from nlParams will result in an exception
}

template<class Derived>
void NlNode<Derived>::initROS ()
{
	try {
		float clockPeriod = 1 / _nlParams.get<float> ("rate");

		_clock = _nh->createTimer (ros::Duration(clockPeriod), &Derived::onSynchronousClock, &derived(), false, false);

		_synchronous = true;
	} catch (const XmlRpc::XmlRpcException &e) {
		_synchronous = false;
	}
}

template<class Derived>
std::shared_ptr<typename NlNode<Derived>::DerivedSinks>
    NlNode<Derived>::sinks() {
	return _nlModFlow->sinks<DerivedSinks> ();
}

template<class Derived>
std::shared_ptr<typename NlNode<Derived>::DerivedSources>
    NlNode<Derived>::sources() {
	return _nlModFlow->sources<DerivedSources> ();
}

template<class Derived>
void NlNode<Derived>::init ()
{
	_nlModFlow = std::make_shared<DerivedModFlow> ();

	try {
		derived().initParams ();
		derived().initROS ();
		_nlModFlow->init<DerivedSources, DerivedSinks> (_nlParams);

	} catch (const XmlRpc::XmlRpcException &e) {
		ROS_ERROR_STREAM(e.getMessage ());
	}
}

template<class Derived>
void NlNode<Derived>::finalizeModFlow () {
	try {
		_nlModFlow->finalize ();
	} catch (const XmlRpc::XmlRpcException &e) {
		ROS_ERROR_STREAM(e.getMessage ());
	}
}

template<class Derived>
int NlNode<Derived>::spin ()
{
	ros::AsyncSpinner spinner(2);

	spinner.start ();

	if (_synchronous)
		_clock.start ();

	ros::waitForShutdown ();

	return 0;
}








}

#endif // NL_NODE_H
