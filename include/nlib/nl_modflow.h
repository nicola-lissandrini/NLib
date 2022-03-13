#ifndef NL_MODFLOW_H
#define NL_MODFLOW_H

#include <ros/ros.h>
#include <typeindex>
#include "nl_params.h"
#include "nl_utils.h"

/**
 * @file nl_modflow.h
 * @author Nicola Lissandrini
 */

namespace nlib {

using ChannelId = int64_t;

class NlModFlow;
class NlModule;
class NlSources;
class NlSinks;

template<typename T>
using Slot = std::function<void(const T &)>;

/// @defgroup modflow NlModFlow: a graph based modular interface


/**
 * @brief Defines a channel that each module can define and to which other modules can connect
 */
class Channel
{
public:
	/**
	 * @brief Create a new channel given:
	 * @param id  Unique identifier of the channel
	 * @param name Unique name that can resolve to the id from a ModFlow handler
	 * @param type Channel type identifier: only slots with same type as channel can be connected
	 * @param owner Pointer to owner module: only owner can emit events on channels has itself created
	 * @param isSink Sink channels are connected to Parent methods, external to modflow
	 * @ingroup modflow
	 */
	Channel (ChannelId id,
		    const std::string &name,
		    const std::type_index &type,
		    const NlModule *owner,
		    bool isSink = false);
	/**
	 * @brief Copy constructor
	 */
	Channel (const Channel &) = default;
	Channel () = default;

	/// @brief Get unique identifier of the channel
	ChannelId id () const;

	/// @brief Get name of the channel
	const std::string &name () const;

	/// @brief Check whether supplied type is compatible with Channel type
	template<typename T>
	bool checkType () const;

	/// @brief Check whether caller is actually the owner of the channel
	bool checkOwnership (const NlModule *caller) const;

	DEF_SHARED(Channel)

private:
	ChannelId _id;
	std::string _name;
	std::shared_ptr<std::type_index> _type;
	bool _isSink;
	const NlModule *_owner;
};


/**
 * @brief This is the core node of a ModFlow graph. Inherit this class to define the main computation to happen in this module. Each module
 * can define new Channels to which it can emit output events after computation is done.
 */
class NlModule
{
public:
	/**
	 * @brief Construct the parent NlModule object
	 * @param modFlow Pointer to ModFlow handler, automatically supplied by @ref NlModFlow::loadModule (Args &&...args)
	 * @param name Unique name of the module
	 */
	NlModule (const std::shared_ptr<NlModFlow> &modFlow,
			const std::string &name);

	const std::string &name () const;
	virtual void initParams (const NlParams &params) {};
	virtual void configureChannels () = 0;
	virtual bool lateConfiguration () const { return false; }

	DEF_SHARED (NlModule)

protected:
	template<typename T, typename M>
	std::enable_if_t<std::is_base_of<NlModule, M>::value>
	    requestConnection (const std::string &channelName, void (M::*slot)(T));

	template<typename T>
	Channel createChannel (const std::string &name);

	template<typename T>
	void emit (const Channel &channel, const T &value);

	template<typename T>
	void emit (const std::string &channelName, const T &value);

protected:
	std::shared_ptr<NlModFlow> _modFlow;

private:
	std::string _name;
};

class NlSources : public NlModule
{
public:
	NlSources (const std::shared_ptr<NlModFlow> &modFlow):
		 NlModule (modFlow, "sources")
	{}

	virtual void initParams (const NlParams &params) override {};
	virtual void configureChannels () override {};
	bool lateConfiguration () const override { return true; }

	template<typename T>
	Channel declareSource (const std::string &name);

	template<typename T>
	void callSource (const std::string &name,
				  const T &value);

	template<typename T>
	void callSource (const Channel &channel,
				  const T &value);

	DEF_SHARED (NlSources)

private:
	// Sources do not receive connections
	using NlModule::requestConnection;
};

class NlSinks : public NlModule
{
public:
	NlSinks (const std::shared_ptr<NlModFlow> &modFlow):
		 NlModule (modFlow, "sinks")
	{}

	void initParams (const NlParams &params) {};
	virtual void configureChannels () = 0;
	bool lateConfiguration () const override { return true; }

	template<typename T, class ParentClass>
	void declareSink (const std::string &name,
				   void (ParentClass::*parentSlot)(T) const,
				   const ParentClass *parent);

	DEF_SHARED(NlSinks)

protected:
	template<typename T>
	void connectToSink (const std::string &channelName,
					const std::string &sinkName);

private:
	// Sinks can only connect channels to sinks directly
	using NlModule::createChannel;
	using NlModule::emit;
	using NlModule::requestConnection;
};

class SerializedSlot
{
	using SerializedFcn = std::function<void(const void *)>;

public:
	template<typename T>
	SerializedSlot (const Slot<T> &slot);

	template<typename T>
	void invoke (const T &arg) const;

	DEF_SHARED (SerializedSlot)
private:
	SerializedFcn serialized;
};

/**
 * @brief The NlModFlow class
 */
class NlModFlow
{
	using Connection = std::vector<SerializedSlot>;

public:
	NlModFlow ();

	template<class DerivedSources, class DerivedSinks>
	void init (const NlParams &nlParams);
	void finalize ();

	template<class DerivedSources>
	std::enable_if_t<std::is_base_of<NlSources, DerivedSources>::value, typename DerivedSources::Ptr>
	    sources ();

	template<class DerivedSinks>
	std::enable_if_t<std::is_base_of<NlSinks, DerivedSinks>::value, typename DerivedSinks::Ptr>
	    sinks ();

	DEF_SHARED (NlModFlow)

protected:
	/**
	 * @brief loadModule
	 * @param args
	 * @return
	 */
	template<class DerivedModule, typename ...Args>
	typename DerivedModule::Ptr
	    loadModule (Args &&...args);

	template<class DerivedModule>
	typename DerivedModule::Ptr
	    getModule (const std::string &name);

	virtual void loadModules () = 0;

private:
	friend class NlModule;
	friend class NlSources;
	friend class NlSinks;

	void configureChannels ();

	template<typename T>
	Channel createChannel (const std::string &name, const NlModule *owner, bool isSink = false);

	Channel resolveChannel (const std::string &name);

	template<typename T>
	void createConnection (const Channel &channel, const Slot<T> &slot);

	template<typename T>
	void emit (const Channel &channel, const T &value, const NlModule *owner);

	template<typename T>
	void emit (const std::string &channelName, const T &value, const NlModule *owner);

private:
	ChannelId _channelsSeq;
	NlSources::Ptr _sources;
	NlSinks::Ptr _sinks;
	std::vector<NlModule::Ptr> _modules;
	std::map<std::string, Channel> _channelNames;
	std::vector<Connection> _connections;
	NlParams _nlParams;
};

template<class DerivedSources>
std::enable_if_t<std::is_base_of<NlSources, DerivedSources>::value, typename DerivedSources::Ptr>
    NlModFlow::sources () {
	return std::dynamic_pointer_cast<DerivedSources> (_sources);
}

template<class DerivedSinks>
std::enable_if_t<std::is_base_of<NlSinks, DerivedSinks>::value, typename DerivedSinks::Ptr>
    NlModFlow::sinks () {
	return std::dynamic_pointer_cast<DerivedSinks> (_sinks);
}


template<class DerivedModule, typename ...Args>
typename DerivedModule::Ptr NlModFlow::loadModule(Args &&...args) {
	auto newModule = std::make_shared<DerivedModule> (
	    std::shared_ptr<NlModFlow> (this), args...);
	_modules.push_back (newModule);
	return newModule;
}

inline Channel::Channel (ChannelId id,
					const std::string &name,
					const std::type_index &type,
					const NlModule *owner,
					bool isSink):
	 _id(id),
	 _name(name),
	 _type (std::make_shared<std::type_index> (type)),
	 _owner(owner),
	 _isSink(isSink)
{}

inline ChannelId Channel::id() const {
	return _id;
}

inline const std::string &Channel::name() const {
	return _name;
}

inline bool Channel::checkOwnership(const NlModule *caller) const {
	if (_isSink)
		return dynamic_cast<const NlSinks *> (caller) != nullptr;
	else
		return caller == _owner;
}

template<typename T>
bool Channel::checkType () const {
	return std::type_index(typeid(T)) == *_type;
}

inline NlModFlow::NlModFlow ():
	 _channelsSeq(0)
{}

inline void NlModFlow::finalize()
{
	for (const NlModule::Ptr &module : _modules) {
		try {
			module->initParams (_nlParams[module->name ()]);
		} catch (...) {}

		if (!module->lateConfiguration ())
			module->configureChannels ();
	}

	_sinks->configureChannels ();
}

template<class DerivedSources, class DerivedSinks>
void NlModFlow::init (const NlParams &nlParams)
{
	_sources = loadModule<DerivedSources> ();
	_sinks = loadModule<DerivedSinks> ();
	_nlParams = nlParams;

	loadModules ();
}

template<typename T>
Channel NlModFlow::createChannel (const std::string &name, const NlModule *owner, bool isSink)
{
	Channel newChannel(_channelsSeq, name, typeid(T), owner);

	_channelNames[name] = newChannel;
	_connections.push_back ({});
	_channelsSeq++;

	return newChannel;
}

inline Channel NlModFlow::resolveChannel(const std::string &name) {
	assert (_channelNames.find (name) != _channelNames.end () && "Channel name does not exist");

	return _channelNames[name];
}

template<typename T>
void NlModFlow::createConnection (const Channel &channel, const Slot<T> &slot)
{
	SerializedSlot serializedSlot(slot);

	_connections[channel.id ()].push_back (serializedSlot);
}

template<typename T>
void NlModFlow::emit (const Channel &channel,
				  const T &value,
				  const NlModule *caller) {
	assert (channel.checkType<T> () && "Channel type mismatch");
	assert (channel.checkOwnership (caller) && "Cannot emit on channels created by different modules");

	for (const SerializedSlot &currentSlot : _connections[channel.id ()]) {
		currentSlot.invoke<T> (value);
	}
}

template<typename T>
void NlModFlow::emit (const std::string &channelName,
				  const T &value,
				  const NlModule *caller) {
	emit (resolveChannel (channelName), value, caller);
}

template<typename T>
SerializedSlot::SerializedSlot (const Slot<T> &slot) {
	serialized = [slot] (const void *arg) {
		slot(*reinterpret_cast<const T *> (arg));
	};
}

template<typename T>
void SerializedSlot::invoke (const T &arg) const {
	serialized(reinterpret_cast<const void *> (&arg));
}

template<typename T>
Channel NlSources::declareSource (const std::string &name) {
	return _modFlow->createChannel<T> (name, this);
}

template<typename T>
void NlSources::callSource (const std::string &channelName, const T &value) {
	emit (channelName, value);
}

template<typename T>
void NlSources::callSource (const Channel &channel, const T &value) {
	emit (channel, value);
}


template<typename T, class ParentClass>
void NlSinks::declareSink (const std::string &name, void (ParentClass::*parentSlot)(T) const, const ParentClass *parent)
{
	Channel channel = _modFlow->createChannel<T> (name, this, true);
	Slot<T> boundSlot = [parent, parentSlot] (T value) {
		const ParentClass *pc = parent;
		(pc->*parentSlot)(value);
	};

	_modFlow->createConnection (channel, boundSlot);
}

template<typename T>
void NlSinks::connectToSink (const std::string &channelName, const std::string &sinkName)
{
	Channel channel = _modFlow->resolveChannel (channelName);
	Channel sink = _modFlow->resolveChannel (sinkName);

	Slot<T> forwardSlot = [this, sink] (const T &value) {
		this->emit (sink, value);
	};

	_modFlow->createConnection (channel, forwardSlot);
}

template<typename T, typename M>
std::enable_if_t<std::is_base_of<NlModule, M>::value>
    NlModule::requestConnection (const std::string &channelName, void (M::*slot)(T))
{
	Channel channel = _modFlow->resolveChannel (channelName);
	Slot<T> boundSlot = std::bind (slot,
							 dynamic_cast<M*> (this),
							 std::placeholders::_1);

	_modFlow->createConnection (channel, boundSlot);
}

template<typename T>
inline Channel NlModule::createChannel (const std::string &name) {
	return _modFlow->createChannel<T> (name, this);
}

inline NlModule::NlModule (const std::shared_ptr<NlModFlow> &modFlow,
					  const std::string &name):
	 _modFlow(modFlow),
	 _name(name)
{
}

inline const std::string &NlModule::name () const {
	return _name;
}

template<typename T>
void NlModule::emit (const Channel &channel, const T &value) {
	_modFlow->emit (channel, value, this);
}

template<typename T>
void NlModule::emit (const std::string &channelName, const T &value) {
	_modFlow->emit (channelName, value, this);
}


}

#endif // NL_MODFLOW_H
