#ifndef NL_MODFLOW_H
#define NL_MODFLOW_H

#include <functional>
#include <ros/ros.h>
#include <type_traits>
#include <typeindex>
#include <set>
#include "nl_params.h"
#include "nl_utils.h"
#include <cxxabi.h>

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
class Channel;

class Event
{
public:
	DEF_SHARED (Event)

	Event (const NlModule *module,
		  const Channel *channel):
		 _parent(nullptr),
		 _module(module),
		 _channel(channel),
		 _depth(0)
	{}

	Event (const std::shared_ptr<Event> &parent,
		  const NlModule *module,
		  const Channel *channel):
		 _parent(parent),
		 _module(module),
		 _channel(channel),
		 _depth(parent->depth () + 1)
	{}

	std::shared_ptr<Event> branch (const NlModule *module,
							 const Channel *channel) {
		return std::make_shared<Event> (_parent, module, channel);
	}

	bool channelInAncestors (const std::string &name) const;
	bool moduleInAncestors (const std::string &name) const;
	std::string moduleName () const;
	std::string channelName () const;
	int depth () const {
		return _depth;
	}

private:
	std::map<std::string, void * const> _resources;
	const Event::Ptr _parent;
	const NlModule *_module;
	const Channel *_channel;
	const int _depth;
};

template<typename R, typename ...T>
using Slot = std::function<R(const Event::Ptr &, const T &...)>;

/**
 * @defgroup modflow NlModFlow: a graph based modular interface
 * @details
 */


/**
 * @brief Defines a channel that each module can create and to which other modules can connect
 * @ingroup modflow
 */
class Channel
{
	template<typename ...typeids>
	static std::vector<std::type_index> stackTypes (const typeids *...ids);

public:
	/**
	 * @brief Create a new channel given:
	 * @param id  Unique identifier of the channel
	 * @param name Unique name that can resolve to the id from a ModFlow handler
	 * @param ids Channel type(s) identifier: only slots with same type(s) as channel can be connected
	 * @param owner Pointer to owner module: only owner can emit events on channels has itself created
	 * @param isSink Sink channels are connected to Parent methods, external to modflow
	 */
	template<typename ...typeids>
	Channel (ChannelId id,
		    const std::string &name,
		    const NlModule *owner,
		    bool isSink,
		    const typeids *...ids);

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
	    /// @tparam T Type(s) to check Channel-type with
	template<typename ...T>
	bool checkType () const;
	std::vector<std::type_index> types () const;
	std::string ownerName () const;

	    /// @brief Check whether caller is actually the owner of the channel
	bool checkOwnership (const NlModule *caller) const;

	DEF_SHARED(Channel)

private:
	ChannelId _id;
	bool _isSink;
	std::string _name;
	std::vector<std::type_index> _types;
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
	NlModule (NlModFlow *modFlow,
			const std::string &name);

	/**
	 * @brief Get module name
	 * @return Const reference to module name
	 */
	const std::string &name () const;

	Event::Ptr lastEvent () const;

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
	bool isEnabled () const;

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
	template<typename ...T, typename M, typename R>
	std::enable_if_t<std::is_base_of<NlModule, M>::value>
		requestConnection (const std::string &channelName, R (M::*slot)(T...));

	/**
     * @brief Request an enabling channel to enable the module.
	 * Until all enabling channels have been triggered, events emitted on other channels
	 * are discarded.
	 * @param channelName Enabling channel name
	 */
	void requestEnablingChannel (const std::string &channelName);
    void requestEnablingChannel (const Channel &channel);

	/**
	 * @brief Create standard channel of given type @p T and with name @p name, owned by this module
	 * @see NlModFlow::createChannel
	 * @tparam T Channel type
	 * @param name Channel name
	 * @return New channel data
	 */
	template<typename ...T>
	Channel createChannel (const std::string &name);

	/**
	 * @brief Ensure at start up that the parent has declared a sink named @p sinkName with type @p T.
	 * @tparam T Type(s) of the sink to be declared by parent
	 * @param sinkName Name of sink to be declared by parent
	 */
	template<typename ...T>
	Channel requireSink (const std::string &sinkName);

	/**
	 * @brief Emit data on channel. All slots connected to the supplied channel will be called in order. @par Complexity
	 * Constant
	 * @see NlModFlow::emit
	 * @tparam T Channel type
	 * @param channel Channel identification
	 * @param value Value to be transmitted
	 */
	template<typename ...T>
	void emit (const Channel &channel, const T &...value);

	template<typename R, typename ...T>
	R callService (const Channel &channel, const T &...value);

	/**
	 *  @brief Emit data on named channel. All slots connected to the supplied channel will be called in order. @par Complexity
	 *  Logarithmic in the number of channels.
	 *  @tparam T Channel type
	 *  @param channelName Channel name to be resolved @see createChannel.
	 */
	template<typename ...T>
	void emit (const std::string &channelName, const T &...value);

	template<typename R, typename ...T>
	R callService (const std::string &channel, const T &...value);


	const ResourceManager &resources () const;
	ResourceManager &resources ();

private:
	void setEnabled (ChannelId enablingChannelId);

protected:
	/// @brief Pointer to NlModFlow handler
	NlModFlow *const _modFlow;
	Event::Ptr _lastEvent;

private:
	std::set<ChannelId> _disablingChannels;
	std::string _name;
};


/**
 * @brief NlSources are a particular NlModule whose channels are declared by external Parent
 * and data on those channels is emitted from external Parent
 * @ingroup modflow
 */
class NlSources final : public NlModule
{
public:
	/**
	 * @brief Construct the parent NlModule object
	 * @param modFlow Pointer to ModFlow handler, automatically supplied by @ref NlModFlow::loadModule
	 */
	NlSources (NlModFlow *modFlow):
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

	/**
	 * @brief Declare source channel to be called from external Parent
	 * @param name Channel name
	 * @return Channel identifier data to call sources with constant execution time.
	 */
	template<typename ...T>
	Channel declareSource (const std::string &name);

	/**
	 * @brief Emit event on channel named @p name.
	 * @tparam T Event type
	 * @param name Channel name.
	 * @param value Event value to be transmitted.
	 */
	template<typename ...T>
	void callSource (const std::string &name,
				  const T &...value);

	/**
	 * @brief Emit event on channel identified by @p channel
	 * @tparam T Event type
	 * @param channel Channel identifier
	 * @param value Event value to be transmitted
	 */
	template<typename ...T>
	void callSource (const Channel &channel,
				  const T &...value);

	DEF_SHARED (NlSources)

private:
	// Sources cannot receive connections
	using NlModule::requestConnection;
};

/**
 *
 * @brief NlSinks is a particular @ref NlModule that do not create regular channels
 * nor emit regular events but create sink channels that are connected to parent object's methods
 * @ingroup modflow
 */
class NlSinks final : public NlModule
{
public:
	/**
	 * @brief Construct the parent NlModule object
	 * @param modFlow Pointer to ModFlow handler, automatically supplied by @ref NlModFlow::loadModule
	 */
	NlSinks (NlModFlow *modFlow):
		 NlModule (modFlow, "sinks")
	{}

	    // Sink network is set up from parent
	void setupNetwork () override {};
	// Sink do not need parameters
	void initParams (const NlParams &params) override {};

	/**
	 * @brief Create special Sink channel and connect it to @p parent's @p slot
	 * @tparam T Channel type(s)
	 * @tparam ParentClass Parent class name
	 * @param name Special sink channel name
	 * @param parent Pointer to Parent class ownig the slot method @p slot
	 */
	template<typename ...T, class ParentClass>
	void declareSink (const std::string &name,
				   void (ParentClass::*parentSlot)(T...),
				   ParentClass *parent);

	DEF_SHARED(NlSinks)

private:
	// Sinks can only emit to sink channels
	using NlModule::createChannel;
};

/**
 * @brief Serialize a callback function from type void(T) to type
 * void (void *) to be stored in the connections array. For internal use
 * @ingroup modflow
 */
class SerializedSlot
{
	using SerializedFcn = std::function<void *(const Event::Ptr &, const void *)>;

	template<typename R, typename ...T>
	std::enable_if_t<(sizeof... (T) <= 1) &&
					 !std::is_same<R, void>::value>
		serialize (const Slot<R, T...> &slot);

	template<typename R, typename ...T,  std::size_t ...is>
	std::enable_if_t<(sizeof... (T) > 1) &&
					 !std::is_same<R, void>::value>
		serialize (const Slot<R, T...> &slot,
				std::index_sequence<is...>);

	template<typename ...T>
	std::enable_if_t<(sizeof... (T) <= 1)>
		serialize (const Slot<void, T...> &slot);

	template<typename ...T,  std::size_t ...is>
	std::enable_if_t<(sizeof... (T) > 1)>
		serialize (const Slot<void, T...> &slot,
				std::index_sequence<is...>);
public:
	/**
	 * @brief Create a generic serialized void(void *) function object
	 * from a void(T) slot that calls the slot by converting
	 * back the void * to T *
	 * @tparam T Function object argument type(s)
	 * @param slot Generic function object of type void(T)
	 */
	template<typename R, typename ...T>
	SerializedSlot (const Slot<R, T...> &slt,
				 const Channel &channel,
				 const std::string &slotName,
				 std::enable_if_t<(sizeof...(T) <= 1)> * = 0);

	template<typename R, typename ...T>
	SerializedSlot (const Slot<R, T...> &slt,
				 const Channel &channel,
				 const std::string &slotName,
				 std::enable_if_t<(sizeof...(T) > 1)> * = 0);

	/**
	 * @brief Invoke the function by casting T&... to a void *
	 * If multiple parameters are given, they are packed into a tuple
	 * @tparam T Function object argument type
	 * @param arg Argument to be supplied to the slot
	 */
	template<typename R, typename ...T>
	std::enable_if_t<(sizeof... (T) <= 1) &&
					 std::is_same<R, void>::value, R>
	    invoke (const Event::Ptr &event, const T &...arg) const;

	template<typename R, typename ...T>
	std::enable_if_t<(sizeof... (T) > 1) &&
					 std::is_same<R, void>::value, R>
	    invoke (const Event::Ptr &event, const T &...arg) const;

	template<typename R>
	std::enable_if_t<std::is_same<R, void>::value, R>
		invoke (const Event::Ptr &event) const;

	template<typename R, typename ...T>
	std::enable_if_t<(sizeof... (T) <= 1) &&
					 !std::is_same<R, void>::value, R>
		invoke (const Event::Ptr &event, const T &...arg) const;

	template<typename R, typename ...T>
	std::enable_if_t<(sizeof... (T) > 1) &&
					 !std::is_same<R, void>::value, R>
		invoke (const Event::Ptr &event, const T &...arg) const;

	template<typename R>
	std::enable_if_t<!std::is_same<R, void>::value, R>
		invoke (const Event::Ptr &event) const;

	std::string name () const {
		return _name;
	}

	DEF_SHARED (SerializedSlot)

private:
	std::string _name;
	Channel _channel;
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
	 * @brief Load sources and sink modules (see @ref loadModule). It calls @ref loadModules that shall be implemented
	 * in a child class of @ref NlModFlow specifying the modules to be loaded, via @ref loadModule.
	 * specifying the connections between standard channels and sink channels
	 * @param nlParams @ref NlParams object containing all network parameters
	 * @todo Make @p nlParams optional
	 */
	void init (const NlParams &nlParams);
	/**
	 * @brief To be called after declaring sources and sinks. For each loaded module, in order, call @ref NlModule::initParams "initParams" and @ref NlModule::setupNetwork "setupNetwork",
	 * intializing each module with parameters and the channels configuration.
	 */
	void finalize ();

	/**
	 * @brief Get sources object
	 * @return Pointer to the sources module
	 */
	NlSources::Ptr sources ();

	/**
	 * @brief Get sinks object
	 * @return Pointer to the sinks module
	 */
	NlSinks::Ptr sinks ();

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
	 * @return New @ref Channel object with the created channel information
	 */
	template<typename ...T>
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
	 * @param name Connection name
	 * @param slot Function object of type void(T)
	 */
	template<typename ...T, typename R>
	void createConnection (const Channel &channel, const Slot<R, T...> &slot, const std::string &name);

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
	template<typename R, typename ...T>
	std::enable_if_t<std::is_same<R, void>::value, R>
		emit (const Channel &channel, const NlModule *caller, const T &...value);

	template<typename R, typename ...T>
	std::enable_if_t<!std::is_same<R, void>::value, R>
		emit (const Channel &channel, const NlModule *caller, const T &...value);

	/**
	 * @brief Emit an event on a channel itentified by its name via @ref resolveChannel
	 * @see emit
	 */
	template<typename R, typename ...T>
	R emit (const std::string &channelName, const NlModule *caller, const T &...value);


private:
	template<typename R, typename ...T>
	Event::Ptr prepareEmit (const Channel &channel, const NlModule *caller);
	void initDebugConfiguration ();
	bool debugFilters (const Event::Ptr &event);

private:
	struct DebugConfiguration {
		bool enabled;
		bool filterOnly;
		std::vector<std::string> filterOnlyChannels;
		std::vector<std::string> filterExcludeChannels;
		std::vector<std::string> filterOnlyModules;
		std::vector<std::string> filterExcludeModules;
	} _debug;

	ChannelId _channelsSeq;
	NlSources::Ptr _sources;
	NlSinks::Ptr _sinks;
	std::vector<NlModule::Ptr> _modules;
	std::map<std::string, Channel> _channelNames;
	std::vector<Connection> _connections;

protected:
	NlParams _nlParams;
	ResourceManager _resources;
};

inline bool Event::moduleInAncestors  (const std::string &name) const {
	if (_parent == nullptr)
		return _module->name () == name;

	return _module->name () == name || _parent->moduleInAncestors (name);
}

inline bool Event::channelInAncestors  (const std::string &name) const {
	if (_parent == nullptr)
		return _channel->name () == name;

	return _channel->name () == name || _parent->channelInAncestors (name);
}

inline NlSources::Ptr NlModFlow::sources () {
	return _sources;
}

inline NlSinks::Ptr NlModFlow::sinks () {
	return _sinks;
}

template<class DerivedModule, typename ...Args>
typename DerivedModule::Ptr NlModFlow::loadModule(Args &&...args) {
    auto newModule = std::make_shared<DerivedModule> (this, args...);
    _modules.push_back (std::dynamic_pointer_cast<NlModule> (newModule));
	return newModule;
}

template<typename ...typeids>
Channel::Channel (ChannelId id,
			   const std::string &name,
			   const NlModule *owner,
			   bool isSink,
			   const typeids *...ids):
	 _id(id),
	 _name(name),
	 _isSink(isSink),
	 _types(stackTypes (ids...)),
	 _owner(owner)
{}

template<typename ...typeids>
std::vector<std::type_index> Channel::stackTypes (const typeids *...ids) {
	return {std::type_index(*ids) ...};
}

inline ChannelId Channel::id() const {
	return _id;
}

inline const std::string &Channel::name() const {
	return _name;
}

inline std::vector<std::type_index> Channel::types() const {
	return _types;
}

inline std::string Channel::ownerName() const {
	return _owner->name ();
}

inline bool Channel::checkOwnership(const NlModule *caller) const {
	if (_isSink)
		return true; // any module can emit on a sink
	return caller == _owner;
}

template<typename ...T>
bool Channel::checkType () const {
	return stackTypes(&typeid(T)...) == _types;
}

template<typename R, typename ...T>
SerializedSlot::SerializedSlot (const Slot<R, T...> &slt,
						  const Channel &channel,
						  const std::string &slotName,
						  std::enable_if_t<(sizeof...(T) <= 1)> *):
	 _channel(channel)
{
	_name = slotName;
	serialize (slt);
}

template<typename R, typename ...T>
SerializedSlot::SerializedSlot (const Slot<R, T...> &slt,
						  const Channel &channel,
						  const std::string &slotName,
						  std::enable_if_t<(sizeof...(T) > 1)> *):
	 _channel(channel)
{
	_name = slotName;
	serialize (slt, std::make_index_sequence<sizeof...(T)> ());
}

template<typename ...T>
std::enable_if_t<(sizeof...(T) <= 1)>
	SerializedSlot::serialize (const Slot<void, T...> &slt)
{
	serialized = [slt] (const Event::Ptr &event, const void *arg) -> void * {
		slt(event, *reinterpret_cast<const T *> (arg) ...);

		return nullptr;
	};
}

template<typename ...T, size_t ...is>
std::enable_if_t<(sizeof...(T) > 1)>
	SerializedSlot::serialize (const Slot<void, T...> &slt, std::index_sequence<is...>)
{
	serialized = [slt] (const Event::Ptr &event, const void *arg) -> void * {
		auto argTuple = *reinterpret_cast<const std::tuple<T...> *> (arg);
		slt(event, std::get<is> (argTuple) ...);

		return nullptr;
	};
}

template<typename R, typename ...T>
std::enable_if_t<(sizeof... (T) <= 1) &&
				 !std::is_same<R, void>::value>
	SerializedSlot::serialize (const Slot<R, T...> &slt)
{
	serialized = [slt] (const Event::Ptr &event, const void *arg) -> void * {
		R *objData = new R; // will be deleted in invoke ()
		*objData = slt(event, *reinterpret_cast<const T *> (arg) ...);
		return reinterpret_cast<void *> (objData);
	};
}

template<typename R, typename ...T, size_t ...is>
std::enable_if_t<(sizeof... (T) > 1) &&
				 !std::is_same<R, void>::value>
	SerializedSlot::serialize (const Slot<R, T...> &slt, std::index_sequence<is...>)
{
	serialized = [slt] (const Event::Ptr &event, const void *arg) -> R {
		R *objData = new R; // will be deleted in invoke ()
		auto argTuple = *reinterpret_cast<const std::tuple<T...> *> (arg);
		*objData = slt(event, std::get<is> (argTuple) ...);

		return reinterpret_cast<void *> (objData);
	};
}

template<typename R, typename ...T>
std::enable_if_t<(sizeof... (T) <= 1) &&
				 std::is_same<R, void>::value, R>
	SerializedSlot::invoke (const Event::Ptr &event, const T &...arg) const
{
	serialized (event, reinterpret_cast<const void *> (&arg) ...);
}

template<typename R, typename ...T>
std::enable_if_t<(sizeof... (T) > 1) &&
				 std::is_same<R, void>::value, R>
	SerializedSlot::invoke (const Event::Ptr &event, const T &...arg) const
{
	auto data = std::tuple<T...> (arg...);
	serialized (event, reinterpret_cast<const void *> (&data));
}

template<typename R>
std::enable_if_t<std::is_same<R, void>::value, R>
	SerializedSlot::invoke (const Event::Ptr &event) const
{
	serialized (event, nullptr);
}

template<typename R, typename ...T>
std::enable_if_t<(sizeof... (T) <= 1) &&
				 !std::is_same<R, void>::value, R>
	SerializedSlot::invoke (const Event::Ptr &event, const T &...arg) const
{
	void *objDataVoid = serialized (event, reinterpret_cast<const void *> (&arg) ...);
	R *objDataR = reinterpret_cast<R *> (objDataVoid);
	R objDataCopy = *objDataR; // allow deletion of temporary allocation

	delete objDataR;

	return objDataCopy;
}

template<typename R, typename ...T>
std::enable_if_t<(sizeof... (T) > 1) &&
				 !std::is_same<R, void>::value, R>
	SerializedSlot::invoke (const Event::Ptr &event, const T &...arg) const
{
	auto data = std::tuple<T...> (arg...);
	void *objDataVoid = serialized (event, reinterpret_cast<const void *> (&data));
	R *objDataR = reinterpret_cast<R *> (objDataVoid);
	R objDataCopy = *objDataR; // allow deletion of temporary allocation

	delete objDataR;

	return objDataCopy;
}

template<typename R>
std::enable_if_t<!std::is_same<R, void>::value, R>
	SerializedSlot::invoke (const Event::Ptr &event) const
{
	void *objDataVoid = serialized (event, nullptr);
	R *objDataR = reinterpret_cast<R *> (objDataVoid);
	R objDataCopy = *objDataR; // allow deletion of temporary allocation

	delete objDataR;

	return objDataCopy;
}

inline NlModFlow::NlModFlow ():
	 _channelsSeq(0)
{}

inline void NlModFlow::finalize()
{
	for (const NlModule::Ptr &module : _modules) {
		module->initParams (_nlParams[module->name ()]);
		module->setupNetwork ();
	}
}

inline void NlModFlow::init (const NlParams &nlParams)
{
	_nlParams = nlParams;

	initDebugConfiguration ();

	_sources = loadModule<NlSources> ();
	_sinks = loadModule<NlSinks> ();

	loadModules ();
}

template<typename ...T>
Channel NlModFlow::createChannel (const std::string &name,
						    const NlModule *owner,
						    bool isSink)
{
	Channel newChannel(_channelsSeq, name, owner, isSink, &typeid(T)...);

	if (_channelNames.find (name) != _channelNames.end ()) {
		std::cout << "Module " << owner->name () << " creating channel "
				  << name << ": already exists" << std::endl;
		assert (false && "Channel name conflict");
	}

	_channelNames[name] = newChannel;
	_connections.push_back ({});
	_channelsSeq++;

	return newChannel;
}

inline Channel NlModFlow::resolveChannel (const std::string &name) {
	if (_channelNames.find (name) == _channelNames.end ()) {
		if (_debug.enabled)
			std::cout << "Channel " << name << " does not exist" << std::endl;

		assert (false && "Channel name does not exist");
	}

	return _channelNames[name];
}

inline void NlModFlow::initDebugConfiguration () {
    _debug.enabled = _nlParams.get<bool> ("mod_flow/debug/enable", false);

	if (!_debug.enabled)
		return;

	_debug.filterOnlyChannels = _nlParams.get<std::string, std::vector> ("mod_flow/debug/only_channels", std::vector<std::string> ());
	_debug.filterOnlyModules = _nlParams.get<std::string, std::vector> ("mod_flow/debug/only_modules", std::vector<std::string> ());

	_debug.filterExcludeChannels = _nlParams.get<std::string, std::vector> ("mod_flow/debug/exclude_channels", std::vector<std::string> ());
	_debug.filterExcludeModules = _nlParams.get<std::string, std::vector> ("mod_flow/debug/exclude_modules", std::vector<std::string> ());

}

inline const ResourceManager &NlModule::resources () const  { return _modFlow->_resources; }
inline ResourceManager &NlModule::resources ()  { return _modFlow->_resources; }

template<typename ...T>
Channel NlModule::requireSink (const std::string &sinkName)
{
	Channel sink = _modFlow->resolveChannel (sinkName);

	assert (sink.checkType<T...> () && "Channel type mismatch");

	return sink;
}


template<typename ...T, typename R>
void NlModFlow::createConnection (const Channel &channel, const Slot<R, T...> &slot, const std::string &name)
{
	SerializedSlot serializedSlot(slot, channel, name);

	_connections[channel.id ()].push_back (serializedSlot);
}

inline void debugTrackEmit (int depth, const Channel &channel, const NlModule *caller, int connectionsCount) {

	std::string connectionsStr = "(" + std::to_string (connectionsCount) + " connection" + (connectionsCount > 1 ? "s" : "") + ")";
	std::cout << (depth == 0 ? "\n" : "")
			<< "\e[33m[ModFlow] [" << printTime(std::chrono::system_clock::now ()) << "]\e[0m "
			<< std::string (depth, '+' ) << (depth > 0 ? " " : "")
			<< "Module \e[32m" << caller->name ()
			<< "\e[0m emitted \e[93m" << channel.name () << "\e[0m "
			<< (connectionsCount > 0? connectionsStr : "(no connections)")
			<< std::endl;
}

inline std::string truncateArguments (const std::string &fcn) {
	return fcn.substr (0, fcn.find ('('));
}

inline void debugConnection (int depth, const NlModule *caller, const SerializedSlot &slot) {
	std::cout << "\e[33m[ModFlow] [" << printTime(std::chrono::system_clock::now ()) << "]\e[0m "
			<< std::string (depth, '+') << (depth > 0 ? " " : "")
			<< "\e[32m" << caller->name () << "\e[0m"
			<< " calling slot \e[36m" << truncateArguments (slot.name ()) << "\e[0m"
			<< (caller->isEnabled () ? "" : "(not enabled)")
			<< std::endl;
}

inline bool contains (const std::vector<std::string> &list, const std::string &key)
{
	return std::find (list.begin (), list.end (), key) != list.end();
}

inline bool NlModFlow::debugFilters (const Event::Ptr &event) {
	if (!_debug.enabled)
		return false;

	for (const std::string &curr : _debug.filterOnlyChannels) {
		if (!event->channelInAncestors (curr))
			return false;
	}


	for (const std::string &curr : _debug.filterOnlyModules) {
		if (!event->moduleInAncestors (curr))
			return false;
	}

	for (const std::string &curr : _debug.filterExcludeChannels) {
		if (event->channelInAncestors (curr))
			return false;
	}

	for (const std::string &curr : _debug.filterExcludeModules) {
		if (event->moduleInAncestors (curr))
			return false;
	}

	return true;
}

template<typename ...T>
void errorChannelTypeMismatch (const Channel &channel, const NlModule *caller, bool emitting) {
	std::cout << "Type mismatch error on module " << caller->name () << "\n";
	std::cout << "Channel " << channel.name() << " requires {";
	for (auto currType : channel.types ()) {
		std::cout << abi::__cxa_demangle (currType.name (), NULL, NULL, NULL) << ", ";
	}
	std::cout << "}\n" << (emitting?  "while emitting types" : "connecting to slot with types")<< " {";
	std::vector<const char *> gotTypes{typeid(T).name()...};
	for (const char *currTypeName : gotTypes) {
		std::cout << abi::__cxa_demangle (currTypeName, NULL, NULL, NULL) << ", ";
	}
	std::cout << "}" << std::endl;
}


inline void errorOwnership (const Channel &channel, const NlModule *caller) {
	std::cout << "Module " << caller->name () << " cannot emit on channel "
			<< channel.name () << ", owned by " << channel.ownerName () << std::endl;
}


template<typename R, typename ...T>
Event::Ptr NlModFlow::prepareEmit(const Channel &channel,
				  const NlModule *caller)
{
	if (!channel.checkType<T...> ()) {
		errorChannelTypeMismatch<T...> (channel, caller, true);

		assert (false && "Channel type mismatch");
	}
	if (!channel.checkOwnership (caller)) {
		errorOwnership (channel, caller);
		assert (false && "Cannot emit on channels created by different modules");
	}

	Event::Ptr lastEvent = caller->lastEvent ();
	Event::Ptr event;

		// This is the case for a source call
	if (lastEvent == nullptr)
		event = std::make_shared<Event> (caller, &channel);
	else
		event = std::make_shared<Event> (lastEvent, caller, &channel);

	if (debugFilters (event))
		debugTrackEmit (event->depth (), channel, caller, _connections[channel.id ()].size ());

	return event;
}

template<typename R, typename ...T>
std::enable_if_t<!std::is_same<R, void>::value, R>
 NlModFlow::emit (const Channel &channel,
				  const NlModule *caller,
				  const T &...value)
{
	Event::Ptr event = prepareEmit<R, T...> (channel, caller);

	assert ((_connections[channel.id ()].size () == 1) && "Non-void return type only allowed to channels with single connections");

	const SerializedSlot &currentSlot = _connections[channel.id ()].front ();

	if (debugFilters (event))
		debugConnection (event->depth (), caller, currentSlot);

	return currentSlot.invoke<R, T...> (event, value...);
}

template<typename R, typename ...T>
std::enable_if_t<std::is_same<R, void>::value, R>
 NlModFlow::emit (const Channel &channel,
				  const NlModule *caller,
				  const T &...value)
{
	Event::Ptr event = prepareEmit<R, T...> (channel, caller);

	for (const SerializedSlot &currentSlot : _connections[channel.id ()]) {
		if (debugFilters (event))
			debugConnection (event->depth (), caller, currentSlot);

		currentSlot.invoke<R, T...> (event, value...);
	}
}


template<typename R, typename ...T>
R NlModFlow::emit (const std::string &channelName,
				  const NlModule *caller,
				  const T &...value) {
	return emit<R, T...> (resolveChannel (channelName), caller, value...);
}

template<typename ...T>
Channel NlSources::declareSource (const std::string &name) {
	return _modFlow->createChannel<T...> (name, this);
}

template<typename ...T>
void NlSources::callSource (const std::string &channelName, const T &...value) {
	emit<T...> (channelName, value...);
}

template<typename ...T>
void NlSources::callSource (const Channel &channel, const T &...value) {
	emit<T...> (channel, value...);
}


template<typename ...T, class ParentClass>
void NlSinks::declareSink (const std::string &name, void (ParentClass::*parentSlot)(T...), ParentClass *parent)
{
	Channel channel = _modFlow->createChannel<T...> (name, this, true);
	Slot<void, T...> boundSlot = [parent, parentSlot] (const Event::Ptr &, const T &... value) {
		(parent->*parentSlot)(value...);
	};

	_modFlow->createConnection (channel, boundSlot, getFcnName(parentSlot));
}

inline std::string Event::moduleName() const {
	return _module->name ();
}

inline std::string Event::channelName() const {
	return _channel->name ();
}

inline Event::Ptr NlModule::lastEvent() const {
	return _lastEvent;
}

template<typename ...T, typename M, typename R>
std::enable_if_t<std::is_base_of<NlModule, M>::value>
	NlModule::requestConnection (const std::string &channelName, R (M::*slot)(T...))
{
	Channel channel = _modFlow->resolveChannel (channelName);

	if (!channel.checkType<T...> ()) {
		errorChannelTypeMismatch<T...> (channel, this, false);
		assert (false && "Channel type mismatch");
	}

	Slot<R, T...> boundSlot = [this, slot] (const Event::Ptr &event, T ...arg) -> R {
		M *child = dynamic_cast<M*> (this);
		this->_lastEvent = event;
		if (this->isEnabled ())
			return (child->*slot) (arg...);
	};

	_modFlow->createConnection (channel, boundSlot, getFcnName (slot));
}

inline void NlModule::requestEnablingChannel (const Channel &channelId)
{
	Slot<void> boundEnableSlot = [this, channelId] (const Event::Ptr &event) {
        this->_lastEvent = event;
        this->setEnabled (channelId.id ());
    };

    _disablingChannels.insert (channelId.id ());
    _modFlow->createConnection (channelId, boundEnableSlot, "<enabling " + channelId.name () + "> [" + name() + "]");

}

inline void NlModule::requestEnablingChannel (const std::string &channelName)
{
    Channel channelId = _modFlow->resolveChannel (channelName);
    requestEnablingChannel (channelId);
}

inline void NlModule::setEnabled (ChannelId enablingChannelId) {
	// If already erased simply ignore
	_disablingChannels.erase (enablingChannelId);
}

inline bool NlModule::isEnabled () const {
	return _disablingChannels.empty ();
}

template<typename ...T>
inline Channel NlModule::createChannel (const std::string &name) {
	return _modFlow->createChannel<T...> (name, this);
}

inline NlModule::NlModule (NlModFlow *modFlow,
					  const std::string &name):
	 _modFlow(modFlow),
	 _name(name),
	 _lastEvent(nullptr)
{
}

inline const std::string &NlModule::name () const {
	return _name;
}

template<typename ...T>
void NlModule::emit (const Channel &channel, const T &...value) {
	_modFlow->emit<void, T...> (channel, this, value...);
}

template<typename ...T>
void NlModule::emit (const std::string &channelName, const T &...value) {
	_modFlow->emit<void, T...> (channelName, this, value...);
}


template<typename R, typename ...T>
R NlModule::callService (const Channel &channel, const T &...value) {
	return _modFlow->emit<R, T...> (channel, this, value...);
}

template<typename R, typename ...T>
R NlModule::callService (const std::string &channelName, const T &...value) {
	return _modFlow->emit<R, T...> (channelName, this, value...);
}



}

#endif // NL_MODFLOW_H
