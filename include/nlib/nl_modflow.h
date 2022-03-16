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

/**
 * @defgroup modflow NlModFlow: a graph based modular interface
 * @details
 */


/**
 * @brief Defines a channel that each module can define and to which other modules can connect
 * @ingroup modflow
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
	   /// @tparam T Type to check Channel-type with
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
 * @ingroup modflow
 */
class NlModule
{
public:
	/**
	 * @brief Construct the parent NlModule object
	 * @param modFlow Pointer to ModFlow handler, automatically supplied by @ref NlModFlow::loadModule
	 * @param name Unique name of the module
	 */
	NlModule (const std::shared_ptr<NlModFlow> &modFlow,
			const std::string &name);

	/**
	 * @brief Get module name
	 * @return Const reference to module name
	 */
	const std::string &name () const;
	/**
	 * @brief Initialize params from NlParams
	 * @param params NlParams value that is supplied as nlParams[module->name ()] from parent
	 */
	virtual void initParams (const NlParams &params) {};
	/**
	 * @brief Implement this function to create channels
	 *  (see @ref NlModule::createChannel) and configure connections to parent channels @see NlModule::requestConnection
	 */
	virtual void setupNetwork () = 0;

	virtual bool lateConfiguration () const { return false; }
	DEF_SHARED (NlModule)

protected:

	/**
	 * @brief Bind connection from given channel name to a member function @p slot of the module
	 * @see NlModFlow::createConnection
	 * @tparam T Type of connection
	 * @tparam M Derived type that defines @ref slot method. Must be derived from NlModule.
	 * @param channelName Channel name to be resolved
	 * @param slot Member function of signature void M::(T)
	 */

	template<typename T, typename M>
	std::enable_if_t<std::is_base_of<NlModule, M>::value>
	    requestConnection (const std::string &channelName, void (M::*slot)(T));


	/**
	 * @brief Create standard channel of given type @p T and with name @p name, owned by this module
	 * @see NlModFlow::createChannel
	 * @tparam T Channel type
	 * @param name Channel name
	 * @return New channel data
	 */
	template<typename T>
	Channel createChannel (const std::string &name);

	/**
	 * @brief Emit data on channel. All slots connected to the supplied channel will be called in order. @par Complexity
	 * Constant
	 * @see NlModFlow::emit
	 * @tparam T Channel type
	 * @param channel Channel identification
	 * @param value Value to be transmitted
	 */
	template<typename T>
	void emit (const Channel &channel, const T &value);

	/**
	 *  @brief Emit data on named channel. All slots connected to the supplied channel will be called in order. @par Complexity
	 *  Logarithmic in the number of channels.
	 *  @tparam T Channel type
	 *  @param channelName Channel name to be resolved @see createChannel.
	 */
	template<typename T>
	void emit (const std::string &channelName, const T &value);

protected:
	/// @brief Pointer to NlModFlow handler
	std::shared_ptr<NlModFlow> _modFlow;

private:
	std::string _name;
};


/**
 * @brief NlSources are a particular NlModule whose channels are declared by external Parent
 * and data on those channels is emitted from external Parent
 * @ingroup modflow
 */
class NlSources : public NlModule
{
public:
	/**
	 * @brief Construct the parent NlModule object
	 * @param modFlow Pointer to ModFlow handler, automatically supplied by @ref NlModFlow::loadModule
	 */
	NlSources (const std::shared_ptr<NlModFlow> &modFlow):
		 NlModule (modFlow, "sources")
	{}

	/**
	 * @brief initParams is optional and often not used
	 * @param params NlParams intialized with parentNlParams["sources"]
	 */
	virtual void initParams (const NlParams &params) override {};
	/**
	 * @brief Channel configuration done externally by @ref NlSources::callSource;
	 * @see NlSources::callSource
	 */
	void setupNetwork () override {};
	bool lateConfiguration () const override { return true; }

	/**
	 * @brief Declare source channel to be called from external Parent
	 * @param name Channel name
	 * @return Channel identifier data to call sources with constant execution time.
	 */
	template<typename T>
	Channel declareSource (const std::string &name);

	/**
	 * @brief Emit event on channel named @p name.
	 * @tparam T Event type
	 * @param name Channel name.
	 * @param value Event value to be transmitted.
	 */
	template<typename T>
	void callSource (const std::string &name,
				  const T &value);

	/**
	 * @brief Emit event on channel identified by @p channel
	 * @tparam T Event type
	 * @param channel Channel identifier
	 * @param value Event value to be transmitted
	 */
	template<typename T>
	void callSource (const Channel &channel,
				  const T &value);

	DEF_SHARED (NlSources)

private:
	// Sources do not receive connections
	using NlModule::requestConnection;
};
/**
 *
 * @brief NlSinks is a particular @ref NlModule that do not create regular channels
 * nor emit regular events but create sink channels that are connected to parent object's methods
 * @ingroup modflow
 */
class NlSinks : public NlModule
{
public:
	/**
	 * @brief Construct the parent NlModule object
	 * @param modFlow Pointer to ModFlow handler, automatically supplied by @ref NlModFlow::loadModule
	 */
	NlSinks (const std::shared_ptr<NlModFlow> &modFlow):
		 NlModule (modFlow, "sinks")
	{}
	/**
	 * @brief initParams is optional and often not used
	 * @param params NlParams intialized with parentNlParams["sources"]
	 */
	virtual void initParams (const NlParams &params) override {};
	/**
	 * @brief Inherit this method to configure connections between existing channels and sinks. This function is called last
	 * of all @ref NlModule::setupNetwork methods for other modules of the graph.
	 * @see NlSinks::connectToSink
	 */
	virtual void setupNetwork () override = 0;
	bool lateConfiguration () const override { return true; }

	/**
	 * @brief Create special Sink channel and connect it to @p parent's @p slot
	 * @tparam T Channel type
	 * @tparam ParentClass Parent class name
	 * @param name Special sink channel name
	 * @param parent Pointer to Parent class ownig the slot method @p slot
	 */
	template<typename T, class ParentClass>
	void declareSink (const std::string &name,
				   void (ParentClass::*parentSlot)(T) const,
				   const ParentClass *parent);

	DEF_SHARED(NlSinks)

protected:
	/**
	 * @brief Automatically emit @p sinkName signal when an event on @p channelName is received
	 * @param channelName General channel name created by any module.
	 * @param sinkName Sink name to which the general channel is to be connected, owned by a @ref NlSinks
	 */
	template<typename T>
	void connectToSink (const std::string &channelName,
					const std::string &sinkName);

private:
	// Sinks can only connect channels to sinks directly
	using NlModule::createChannel;
	using NlModule::emit;
	using NlModule::requestConnection;
};

/**
 * @brief Serialize a callback function from type void(T) to type
 * void (void *) to be stored in the connections array. For internal use
 * @ingroup modflow
 */
class SerializedSlot
{
	using SerializedFcn = std::function<void(const void *)>;

public:
	/**
	 * @brief Create a generic serialized void(void *) function object
	 * from a void(T) slot that calls the slot by converting
	 * back the void * to T *
	 * @tparam T Function object argument type
	 * @param slot Generic function object of type void(T)
	 *
	 */
	template<typename T>
	SerializedSlot (const Slot<T> &slot);

	/**
	 * @brief Invoke the function by casting &T to a void *
	 * @tparam T Function object argument type
	 * @param arg Argument to be supplied to the slot
	 */
	template<typename T>
	void invoke (const T &arg) const;

	DEF_SHARED (SerializedSlot)
private:
	SerializedFcn serialized;
};

/**
 * @brief This is the main class that handles the call flow between
 * module. You will need to inherit from this class and override @ref loadModules to
 * specify what modules you want to load.
 *
 * The typical intializations step are the following
 * - @ref init Initializes ModFlow by providing the custom classes
 * derived from @ref NlSources and @ref NlSinks. (see @ref init for more details)
 * - @ref sources() -> @ref NlSources::declareSource "declareSource (...)" to declare channels which the sources will emit data to
 * - @ref sinks() ->@ref NlSinks::declareSink "declareSink (...)" to declare channels that will be connected to external slots from sinks
 * - @ref finalize() will configure each loaded module by calling @ref NlModule::initParams, if any,
 * and @ref NlModule::setupNetwork, that each module shall override.
 *
 * After the intialization phase, externally the sources can be triggered by
 * calling @ref sources() -> @ref callSource, which will emit an event on the specified channel
 * Sinks will connect regular channel events to so called sink channels, hold by
 * a @ref NlSinks -derived  class, as specified via @ref NlSinks::connectToSink in the overridden @ref NlSinks::setupNetwork.
 * As an event an a sink-channel is emitted, the callback @p parentSlot defined in the declaration @ref NlSinks::declareSink is called
 * forwarding the regular channel data to the parent object slot.
 * @ingroup modflow
 */
class NlModFlow
{
	using Connection = std::vector<SerializedSlot>;

public:
	NlModFlow ();

	/**
	 * @brief Load sources and sink modules (see @ref loadModule) according to the specified
	 * @p DerivedSources and @p DerivedSinks. It also calls @ref loadModules that shall be implemented
	 * in a child class of @ref NlModFlow specifying the modules to be loaded, via @ref loadModule.
	 * @tparam DerivedSources Child class inheriting from @ref NlSources or NlSources itself.
	 * @tparam DerivedSinks Child class inheriting from @ref NlSinks, which must override @ref NlSinks::setupNetwork "setupNetwork"
	 * specifying the connections between standard channels and sink channels
	 * @param nlParams @ref NlParams object containing all network parameters
	 * @todo Make @p nlParams optional
	 */
	template<class DerivedSources, class DerivedSinks>
	std::enable_if_t<std::is_base_of<NlSources, DerivedSources>::value &&
				  std::is_base_of<NlSinks, DerivedSinks>::value>
	    init (const NlParams &nlParams);
	/**
	 * @brief For each loaded module, in order, call @ref NlModule::initParams "initParams" and @ref NlModule::setupNetwork "setupNetwork",
	 * intializing each module with parameters and the channels configuration.
	 */
	void finalize ();

	/**
	 * @brief Get sources object
	 * @tparam DerivedSources Actual class derived from @ref NlSources
	 * @return Pointer to the sources module, downcasted to the child source class @p DerivedSources
	 */
	template<class DerivedSources>
	std::enable_if_t<std::is_base_of<NlSources, DerivedSources>::value, typename DerivedSources::Ptr>
	    sources ();


	/**
	 * @brief Get sinks object
	 * @tparam DerivedSinks Actual class derived from @ref NlSinks
	 * @return Pointer to the sinks module, downcasted to the child source class @p DerivedSinks
	 */
	template<class DerivedSinks>
	std::enable_if_t<std::is_base_of<NlSinks, DerivedSinks>::value, typename DerivedSinks::Ptr>
	    sinks ();

	DEF_SHARED (NlModFlow)

protected:
	friend class NlModule;
	friend class NlSources;
	friend class NlSinks;

	/**
	 * @brief Create a new std::shared_ptr object of type @p DerivedModule
	 * @tparam DerivedModule Name of a class derived from @ref NlModule
	 * @tparam Args Optional parameter pack arguments for the module constructor
	 * @param args Optional arguments to supply to module constructor
	 * @return Pointer to created module
	 */
	template<class DerivedModule, typename ...Args>
	typename DerivedModule::Ptr
	    loadModule (Args &&...args);
	/**
	 * @brief Method to be overridden to specifiy the modules to be loaded via @ref loadModule
	 */
	virtual void loadModules () = 0;


	/**
	 * @brief Declare a new channel of type @p T. The channel is owned by module @p owner,
	 * that is the only module that can emit events on this channel.
	 * @tparam T Type of the channel
	 * @param name Name of the channel
	 * @param owner Owner module
	 * @param isSink If @p false (default), the channel is regular. If @p true, it refers
	 * to a sink channel, whose slot is external to ModFlow and not belonging to another module
	 * @return New @ref Channel object with the created channel information
	 */
	template<typename T>
	Channel createChannel (const std::string &name, const NlModule *owner, bool isSink = false);

	/**
	 * @brief Get full channel information given its name @par Complexity Logarithmic in the number of channels.
	 * @param name Channel name
	 * @return Channel information
	 */
	Channel resolveChannel (const std::string &name);

	/**
	 * @brief Add @p slot to the connections of the supplied channel.
	 * @note Do not call this method directly. Depending on the channel type:
	 * - If the channel is regular, @p slot must be bound to the module object owning the channel.
	 * Achieve this by calling @ref NlModule::requestConnection from the module.
	 * - If the channel is sink, it is bound to the external calling object.
	 * Achieve this by calling @ref NlSinks::connectToSink
	 * @param channel Channel data
	 * @param slot Function object of type void(T)
	 */
	template<typename T>
	void createConnection (const Channel &channel, const Slot<T> &slot);

	/**
	 * @brief Emit an event on @p channel. This will call the corresponding
	 * slot of each module connected to this channel, supplying @p value.
	 * @note Two sanity checks are performed run-time:
	 * - Verify that value type @p T is compatible with channel typ
	 * - Verify that @p caller is be the owner of the channel
	 * @tparam T Channel type
	 * @param channel Channel to emit the event on
	 * @param value Value to be transmitted
	 * @param caller NlModule that is emitting event
	 */
	template<typename T>
	void emit (const Channel &channel, const T &value, const NlModule *caller);

	/**
	 * @brief Emit an event on a channel itentified by its name via @ref resolveChannel
	 * @see emit
	 */
	template<typename T>
	void emit (const std::string &channelName, const T &value, const NlModule *caller);

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
	/*if (_isSink)
		return dynamic_cast<const NlSinks *> (caller) != nullptr;
	else
		*/
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
			module->setupNetwork ();
	}

	_sinks->setupNetwork ();
}

template<class DerivedSources, class DerivedSinks>
std::enable_if_t<std::is_base_of<NlSources, DerivedSources>::value &&
			  std::is_base_of<NlSinks, DerivedSinks>::value>
    NlModFlow::init (const NlParams &nlParams)
{
	_sources = loadModule<DerivedSources> ();
	_sinks = loadModule<DerivedSinks> ();
	_nlParams = nlParams;

	loadModules ();
}

template<typename T>
Channel NlModFlow::createChannel (const std::string &name,
						    const NlModule *owner,
						    bool isSink)
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
		(parent->*parentSlot)(value);
	};

	_modFlow->createConnection (channel, boundSlot);
}

template<typename T>
void NlSinks::connectToSink (const std::string &channelName, const std::string &sinkName)
{
	Channel channel = _modFlow->resolveChannel (channelName);
	Channel sink = _modFlow->resolveChannel (sinkName);

	assert (channel.checkType<T> () && "Channel type mismatch");

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
