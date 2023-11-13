#ifndef NL_UTILS_H
#define NL_UTILS_H

#include <cstring>
#include <iostream>
#include <cmath>
#include <chrono>
#define BOOST_STACKTRACE_USE_ADDR2LINE
#include <boost/stacktrace.hpp>
#include <optional>
#include <map>
#include <sstream>
#include <dlfcn.h>
#include <iomanip>
#include <any>
#include <list>
#include <variant>
#ifdef INCLUDE_EIGEN
#include <eigen3/Eigen/Core>
#endif

/** @file nlutils.h
 *  @author Nicola Lissandrini
 */

namespace nlib {
/**
 * @defgroup qdt Quick debugging tools
 */

/// @brief Shortcut to cout line
/// @ingroup qdt
#define COUT(str) {std::cout <<  (str) << std::endl;}
/// @brief Print name and value of expression
/// @ingroup qdt
#define COUTN(var) {std::cout << "\e[33m" << #var << "\e[0m" << std::endl << var << std::endl;}
/// @brief Print name and value of an expression that is a time point
/// @ingroup qdt
#define COUTNT(var) {std::cout << "\e[33m" << #var << "\e[0m" << std::endl << nlib::printTime(var) << std::endl;}
/// @brief Print name and shape of expressione
/// @ingroup qdt
#define COUTNS(var) {COUTN(var.sizes());}
/// @brief Print calling function, name and value of expression
/// @ingroup qdt
#define COUTNF(var) {std::cout << "\e[32m" << __PRETTY_FUNCTION__ << "\n\e[33m" << #var << "\e[0m" << std::endl << var << std::endl;}
/// @brief Shortcut for printing name and value and returning the result of expression
/// @ingroup qdt
#define COUT_RET(var) {auto __ret = (var); COUTN(var); return __ret;}
/// @brief Get demangled type of expression
/// @ingroup qdt
#define TYPE(type) (abi::__cxa_demangle(typeid(type).name(), NULL,NULL,NULL))
/// @brief Shortcut for printing boost stacktrace
/// @ingroup qdt
#define STACKTRACE {std::cout << boost::stacktrace::stacktrace() << std::endl;}
/// @brief Print calling function and current file and line
/// @ingroup qdt
#define QUA {std::cout << "\e[33mReached " << __PRETTY_FUNCTION__ << "\e[0m:" << __LINE__ << std::endl; }
#define WAIT_GDB {volatile int __done = 0; while (!__done) sleep(1);}
template<typename T>
std::string getFcnName (T functionAddress) {
	Dl_info info;
	dladdr (reinterpret_cast<void *>(functionAddress), &info);
	return abi::__cxa_demangle(info.dli_sname, NULL, NULL, NULL);
}

#ifdef EIGEN_CORE_H
template <typename Derived>
std::string get_shape(const Eigen::EigenBase<Derived>& x)
{
	std::ostringstream oss;
	oss  << "(" << x.rows() << ", " << x.cols() << ")";
	return oss.str();
}
#endif

/**
 * @section Shared ptr tools
 */

/// @brief define nested Classname::Ptr symbol for shared_ptr of the class
/// To be used inside class definition
#define DEF_SHARED(classname) using Ptr = std::shared_ptr<classname>;
/// @brief define concat ClassnamePtr symbol for shared_ptr of the class
/// To be used outside class definition
#define DEF_SHARED_CAT(classname) using classname##Ptr = std::shared_ptr<classname>;
/// @brief Define base handle for inherited params. To be protected
#define NLIB_PARAMS_BASE  \
template<class _DerivedParams>\
	const _DerivedParams &paramsDerived () const {\
		return *std::dynamic_pointer_cast<_DerivedParams> (_params);\
}
/// @brief Automatic set params. To be public
#define NLIB_PARAMS_SET \
template<class _DerivedParams> \
	void setParams (const _DerivedParams &params) {\
		_params = std::make_shared<_DerivedParams> (params);\
}

/// @brief Define specific handle for inherited params, shorthand for Base::params<Derived::Params>
#define NLIB_PARAMS_INHERIT(Base) \
const Params &params () const { \
		return Base::paramsDerived<Params> (); \
}

#if __cplusplus >= 201703L
namespace traits {

template<typename T, template<typename> class Expression, typename = void>
struct is_valid_expression : std::false_type { };

template<typename T, template<typename> class Expression>
struct is_valid_expression<T, Expression, std::void_t<Expression<T>>> : std::true_type { };

#define DEFINE_EXPRESSION_CHECKER(name, expression) \
template<typename T> \
	using name##_expression = expression; \
	\
	template<typename T> \
	inline constexpr bool name = is_valid_expression<T, name##_expression>::value;

DEFINE_EXPRESSION_CHECKER(has_iterator, typename T::iterator)
DEFINE_EXPRESSION_CHECKER(overloads_ostream_op, decltype(std::declval<std::ostream> () << std::declval<T> ()))

} // namespaec traits
#endif //  __cplusplus >= 201703L


/**
 * @defgroup gpt General purpose tools
 */

/// @brief Range struct with convenience methods
/// @param T any literal type that can be converted to floating point
/// @ingroup gpt
template<typename T>
struct RangeBase {
	T min, max;
	std::optional<T> step;

	int count () const;
	T width () const;
};

template<typename T>
int RangeBase<T>::count() const {
	return step.has_value () ?
			   static_cast<int> (floor ((max - min) / *step)) :
			   -1;
}

template<typename T>
T RangeBase<T>::width() const {
	return max - min;
}

template<typename T>
std::ostream &operator << (std::ostream &os, const RangeBase<T> &range);

using Range = RangeBase<float>;

// Dimensions
#define D_1D 1
#define D_2D 2
#define D_3D 3
#define D_4D 4

class Flag
{
	bool value;
	bool fixed;

public:
	explicit Flag (bool _value = false, bool _fixed = false):
		  value(_value),
		  fixed(_fixed)
	{}

	bool get () const {
		return value;
	}
	void set (bool _value) {
		value = _value;
	}
	bool isFixed () {
		return fixed;
	}
};

template<typename T>
class ReadyFlags
{
	std::map<T, Flag> flags;
	bool updated;

public:
	explicit ReadyFlags ():
		  updated(false)
	{
	}

	void addFlag (T id, bool fixed = false, bool initialValue = false) {
		flags.insert (std::make_pair (id, Flag (initialValue, fixed)));
	}
	void resetFlags () {
		for (typename std::map<T, Flag>::iterator it = flags.begin (); it != flags.end (); it++) {
			if (!it->second.isFixed ())
				it->second.set (false);
		}
	}
	void set (T id) {
		flags[id].set (true);
		updated = true;
	}
	void reset (T id) {
		flags[id].set (false);
		updated = true;
	}
	bool get (T id) const {
		return flags.at (id).get ();
	}
	bool operator[] (T id) const {
		return get (id);
	}
	void setProcessed () {
		updated = false;
		resetFlags ();
	}
	bool isProcessed () const {
		return !updated;
	}
	bool all () const {
		for (typename std::map<T, Flag>::const_iterator it = flags.begin (); it != flags.end (); it++)
			if (!it->second.get ())
				return false;
		return true;
	}
	std::string dump () const {
		std::stringstream ss;

		for (auto f : flags) {
			ss << "'" << f.first << "': " << f.second.get() << std::endl;
		}
		return ss.str();
	}
};

using ReadyFlagsStr = ReadyFlags<std::string>;

#if __cplusplus >= 201703L

class ResourceManager
{
public:
	template<typename T, typename ...Args>
	void create (const std::string &name, Args &&...args);

	template<typename T>
	std::shared_ptr<T> get (const std::string &name);

private:
	std::map<std::string, std::any> _resources;
};

template<typename T, typename ...Args>
void ResourceManager::create (const std::string &name, Args &&...args)
{
	_resources[name] = std::make_shared<T> (args...);
}

template<typename T>
std::shared_ptr<T> ResourceManager::get (const std::string &name) {
	auto resource = _resources[name];

	if (resource.type () != typeid(std::shared_ptr<T>)) {
		std::cout << "Error: Resource " << name << " has type " << resource.type ().name () << ". Got " << typeid(std::shared_ptr<T>).name () << "\nAborting" << std::endl;
		std::abort ();
	}

	return std::any_cast<std::shared_ptr<T>> (_resources[name]);
}
template<typename T, typename Status, const char * const * strings = nullptr, Status ...defaultValue>
class AlgorithmResult
{
	static_assert (sizeof ...(defaultValue) <= 1, "defaultValue must be either 0 or 1 element");

public:
	AlgorithmResult (const T &value): _result(value) { }
	AlgorithmResult (const Status &error): _result(error) {}
	AlgorithmResult (): _result(Status()) {} // if the result needs to be default constructed, we prefer initializing to Status rather than T, which is more likely to be more expensive to construct

	bool success () const {
		return std::holds_alternative<T> (_result);
	}

	operator bool () const {
		return success ();
	}

	T value () const {
		return std::get<T> (_result);
	}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreturn-type"

	Status status () const {
		if (success()) {
			if constexpr (hasDefaultValue())
				return std::get<0> (_defaultValue);
		}  else
			return std::get<Status> (_result);
	}
#pragma GCC diagnostic pop

	std::string toString () const {
		std::stringstream ss;

		if (success ()) {
			ss << value ();

			if constexpr (hasDefaultValue ()) {
				ss << " (status: ";
				if (_strings != nullptr)
					ss << _strings[static_cast<int> (status())];
				else
					ss << static_cast<int> (status());
				ss << ")";
			}
		} else {
			ss << "Status: ";
			if (_strings != nullptr)
				ss << _strings[static_cast<int> (status())];
			else
				ss << static_cast<int> (status());
		}

		return ss.str();
	}

private:
	constexpr static bool hasDefaultValue () {
		return sizeof ...(defaultValue) == 1;
	}

private:
	std::variant<T, Status> _result;
	constexpr static const char * const *_strings = strings;
	constexpr static auto _defaultValue = std::tuple (defaultValue...);
};

template<typename T, typename Status,const char * const * strings = nullptr, Status ...defaultValue>
std::ostream &operator << (std::ostream &os, const AlgorithmResult<T,Status,strings, defaultValue...> &ar) {
	os << ar.toString ();
	return os;
}

template<typename T>
class IteratorRange
{
	static_assert (traits::has_iterator<T>, "No nested type iterator defined in template parameter type");

	using iterator = typename T::iterator;

	iterator _begin, _end;

public:
	IteratorRange (const iterator &begin, const iterator &end):
		  _begin(begin), _end(end) { }
	IteratorRange () = delete;

	typename T::iterator begin () const { return _begin; }
	typename T::iterator end () const { return _end; }
	bool empty () { return _begin == _end; }
};

template<typename DataType, typename LabelType = std::monostate>
class TreeNode
{
	using Children = std::list<TreeNode *>;

	// Only allow creating standalone root nodes
	TreeNode (DataType &&data, const LabelType &label, TreeNode *parent);
	TreeNode (const DataType &data, const LabelType &label, TreeNode *parent);

public:
	// WARNING: this is the constructor that moves the ownership of the data
	// to the node itself, to call it you need to "new TreeNode (std::move(data))"
	TreeNode (DataType &&data, const LabelType &label = LabelType()):
		  TreeNode (std::move(data), label, nullptr)
	{}

	// WARNING: this triggers a deep copy of the data
	TreeNode (const DataType &data,  const LabelType &label = LabelType()):
		  TreeNode (data, label, nullptr)
	{}

	// To call this method: node->addChild (move (data),...)
	TreeNode *addChild (DataType &&data, const LabelType &label = LabelType());
	// This performs a deep copy
	TreeNode *addChild (const DataType &data, const LabelType &label = LabelType());
	TreeNode *nthAncestor (int n);
	TreeNode *nthDescendant (int n);
	TreeNode *parent () { return _parent; }
	const LabelType &label () const { return _label; }
	LabelType &label () { return _label; }
	int childrenCount () const { return _childrenCount; }
	int depth () const { return _depth; }
	bool isRoot () const { return _parent == nullptr; }
	bool isLeaf () const { return _childrenCount == 0; }

	IteratorRange<Children> children () { return {_children.begin (), _children.end ()}; }

	~TreeNode ();

	const DataType &data () const { return _data; }
	DataType &data () { return _data; }

private:
	TreeNode *_parent;
	Children _children;
	DataType _data;
	LabelType _label;
	int _childrenCount;
	int _depth;
};

template<typename DataType, typename LabelType>
TreeNode<DataType, LabelType>::TreeNode (DataType &&data, const LabelType &label, TreeNode *parent):
	  _data(std::move (data)),
	  _label(label),
	  _parent(parent),
	  _childrenCount(0)
{
	if (isRoot ())
		_depth = 0;
	else
		_depth = parent->depth () + 1;
}

template<typename DataType, typename LabelType>
TreeNode<DataType, LabelType>::TreeNode (const DataType &data,  const LabelType &label, TreeNode *parent):
	  TreeNode (DataType {data}, label, parent)
{}

template<typename DataType, typename LabelType>
TreeNode<DataType, LabelType>::~TreeNode () {
	for (TreeNode *child : _children)
		delete child;
}

template<typename DataType, typename LabelType>
TreeNode<DataType, LabelType> *TreeNode<DataType, LabelType>::addChild (DataType &&data,  const LabelType &label) {
	_children.push_back (new TreeNode (std::move (data), label, this));
	_childrenCount++;
	return _children.back ();
}

template<typename DataType, typename LabelType>
TreeNode<DataType, LabelType> *TreeNode<DataType, LabelType>::addChild (const DataType &data,  const LabelType &label) {
	return addChild (DataType {data}, label);
}

template<typename DataType, typename LabelType>
TreeNode<DataType, LabelType> *TreeNode<DataType, LabelType>::nthAncestor (int n) {
	TreeNode *ret = this;

	while (n > 0) {
		if (ret == nullptr)
			return nullptr;
		ret = ret->parent ();
		n--;
	}

	return ret;
}


// Find descendant for nodes that only have 1 child in the whole descendant line
template<typename DataType, typename LabelType>
TreeNode<DataType, LabelType> *TreeNode<DataType, LabelType>::nthDescendant (int n) {
	TreeNode *ret = this;

	while (n > 0) {
		if (ret->childrenCount () != 1)
			return nullptr;
		ret = *ret->children ().begin ();
		n--;
	}

	return ret;
}

template<typename DataType, typename LabelType = std::monostate, typename ExtraDataType = std::monostate>
class Tree
{
public:
	using Node = TreeNode<DataType, LabelType>;
	template<typename ...Args>
	using VisitLambda = std::function<void(Node *, Args ...)>;

	enum Algorithm {
		DEPTH_FIRST_PREORDER,
		DEPTH_FIRST_POSTORDER,
		BREADTH_FIRST
	};

	Tree (DataType &&rootValue, const LabelType &label = LabelType()):
		  _root(new Node (std::move (rootValue), label))
	{}

	Tree (const DataType &rootValue, const LabelType &label = LabelType()):
		  Tree(DataType{rootValue}, label)
	{}

	Node *root () const { return _root; }

	~Tree () { delete _root; }


	template<typename ...Args>
	void traverse (Algorithm algorithm, const VisitLambda<Args...> &visit, Args ...args) {
		traverse<Args...> (algorithm, root(), visit, args...);
	}

	template<typename Fcn = std::function<DataType(Node *)>>
	std::string toJson (const Fcn &printData = [](Node *node) {return node->data ();}) const {
		return toJson (root (), printData);
	}

	template<typename U = DataType>
	std::string toGraphviz (const std::function<U(Node *)> printNode = [](Node *node) -> DataType {return node->data(); }) {
		VisitLambda<std::stringstream &> graphvizVisit = [printNode] (Node *node, std::stringstream &str) {
			if (!node->isRoot ())
				str << printNode (node->parent()) << " -> " << printNode (node) << "; \n";
		};

		std::stringstream str;

		str << "digraph Tree {\n";

		traverse<std::stringstream &> (DEPTH_FIRST_PREORDER, graphvizVisit, str);

		str << "}";

		return str.str ();
	}

	template<typename U = ExtraDataType>
	std::enable_if_t<!std::is_same_v<U, std::monostate>, const ExtraDataType &>
	extraData () const {
		return _extraData;
	}

	template<typename U = ExtraDataType>
	std::enable_if_t<!std::is_same_v<U, std::monostate>, ExtraDataType &>
	extraData () {
		return _extraData;
	}


private:
	template<typename ...Args>
	void traverse (Algorithm algorithm, Node *node, const VisitLambda<Args...> &visit, Args ...args) {
		switch (algorithm) {
		case DEPTH_FIRST_PREORDER:
			depthFirstPreorder<Args...> (node, visit, args...);
			break;
		case DEPTH_FIRST_POSTORDER:
			depthFirstPostorder<Args...> (node, visit, args...);
			break;
		case BREADTH_FIRST:
			breadthFirst<Args...> (node, visit, args...);
			break;
		default:
			break;
		}
	}

	template<typename ...Args>
	static void depthFirstPreorder (Node *node, const VisitLambda<Args...> &visit, Args ...args) {
		visit (node, args...);

		for (Node *child : node->children ())
			depthFirstPreorder<Args...> (child, visit, args...);
	}

	template<typename ...Args>
	static void depthFirstPostorder (Node *node, const VisitLambda<Args...> &visit, Args ...args) {
		for (Node *child : node->children ())
			depthFirstPostorder<Args...> (child, visit, args...);

		visit (node, args...);
	}

	template<typename ...Args>
	static void breadthFirst (Node *node, const VisitLambda<Args...> &visit, Args ...args) {
		for (Node *child : node->children ())
			visit (child, args...);

		for (Node *child : node->children ())
			breadthFirst<Args...> (child, visit, args...);

	}

	template<typename Fcn>
	std::string toJson (Node *node, const Fcn &printData) const {
		std::stringstream ss;

		ss << "{";

		if constexpr (!std::is_same_v<LabelType, std::monostate>)
			ss << "\"label\": " << node->label () << ", ";

		ss << "\"data\": " << printData (node);

		if (!node->isLeaf ()) {
			ss << ", \"children\": [";

			int count = 0;
			for (Node *child : node->children ()) {
				ss << toJson (child, printData);
				if (count < node->childrenCount () - 1)
					ss << ", ";
				count++;
			}
			ss << "]";
		}
		ss << "}";
		return ss.str ();
	}

private:
	ExtraDataType _extraData;
	Node *_root;
};



#endif //  __cplusplus >= 201703L

template<class Duration = std::chrono::milliseconds, class Clock = std::chrono::steady_clock>
class TimeHysteresis
{
	using Time = typename Clock::time_point;

public:
	struct Params {

	};

	TimeHysteresis (int lockTime, int releaseTime):
		  _lockDuration(lockTime),
		  _releaseDuration(releaseTime)
	{}

	void trigger ();

	bool isLocked () const { return _locked; }

private:
	Time _transitionTime;
	bool _locked;
};

template<class Duration, class Clock>
void TimeHysteresis<Duration, Clock>::trigger()
{
	auto currentTime = Clock::now ();
	auto elapsed = currentTime - _transitionTime;

	// Transition from released to locked
	if (!_locked && elapsed >= _releaseDuration) {
		_locked = true;
		_transitionTime = currentTime;
	} else if (_locked && elapsed >= _lockDuration) {
		_locked = false;
		_transitionTime = currentTime;
	}
}


template<class T, class clock, class duration>
class TimedObject
{
public:
	using Time = std::chrono::time_point<clock, duration>;

	template<typename other_clock, typename ...Args>
	TimedObject (const std::chrono::time_point<other_clock> &time, Args ...args):
		  _obj(args ...),
		  _time(std::chrono::time_point_cast<duration> (time))
	{}

	TimedObject () = default;

	// Avoid implicit direct conversion from T to Timed<T>
	TimedObject (const T &other) = delete;

	T &obj() {
		return _obj;
	}

	const T &obj () const {
		return _obj;
	}

	Time &time () {
		return _time;
	}

	const Time &time () const {
		return _time;
	}


#define TIMED_TIMED_COMPARISON(op) \
	bool operator op (const TimedObject<T, clock, duration> &rhs) const { \
			return time () op rhs.time (); \
	}

#define TIMED_TIME_COMPARISON(op) \
	bool operator op (const Time &rhs) const { \
			return time () op rhs; \
	}

	TIMED_TIMED_COMPARISON(<)
	TIMED_TIMED_COMPARISON(<=)
	TIMED_TIMED_COMPARISON(>)
	TIMED_TIMED_COMPARISON(>=)
	TIMED_TIME_COMPARISON(<)
	TIMED_TIME_COMPARISON(<=)
	TIMED_TIME_COMPARISON(>)
	TIMED_TIME_COMPARISON(>=)

private:
	T _obj;
	Time _time;

};

template<class ToType, class FromType, class clock, class duration>
TimedObject<ToType, clock, duration> timed_cast (const TimedObject<FromType, clock, duration> &from) {
	return TimedObject<ToType, clock, duration> (from.time (), static_cast<ToType> (from.obj ()));
}

template<class to_duration = std::chrono::milliseconds, class time_point>
std::string printTime (const time_point &time) {
	using TimeInt = std::chrono::time_point<typename time_point::clock>;
	TimeInt timeInt = std::chrono::time_point_cast<typename TimeInt::duration> (time);
	auto coarse = std::chrono::system_clock::to_time_t(timeInt);
	auto fine = std::chrono::time_point_cast<to_duration>(time);

	char buffer[sizeof "9999-12-31 23:59:59.999"];

	std::snprintf(buffer + std::strftime(buffer, sizeof buffer - 3,
										 "%F %T.", std::localtime(&coarse)),
				  4, "%03lu", fine.time_since_epoch().count() % 1000);
	return buffer;
}

template<class T, class clock, class duration>
std::ostream &operator << (std::ostream &os, const TimedObject<T, clock, duration> &timed) {
	os << "[" << printTime (timed.time()) << "] " << timed.obj ();
	return os;
}

/// @brief [0,2pi) to [-pi, pi)
#define CONVERT_RANGE(v) (fmod ((v)+M_PI,2*M_PI)-M_PI)

inline void quickGDB (int argc, char *argv[]) {
	if (argc == 1)
		return;
	if (std::strcmp(argv[1], "true") == 0) {
		std::cout << "Waiting for gdb to attach..." << std::endl;
		WAIT_GDB;
	}
}

/// @defgroup pt Profiling tools

const std::string secStrings[] = {
	"s", "ms", "us", "ns"
};

inline std::string secView (double x) {
	std::stringstream ss;
	ss << std::fixed << std::setprecision(2);
	for (int i = 0; i < 4; i++) {
		double xp = x * pow(10, i*3);
		if (xp >= 1) {
			ss << std::setw(6) << xp << secStrings[i];
			return ss.str();
		}
	}
	ss << x << secStrings[0];
	return ss.str();
}

/// @brief Compute execution time of @p lambda
/// @param taken: floating point varible to store total time
/// @param lambda: lambda function of type [&](){ ... generic code ...}
/// @param ndiv: number of trials inside the lambda, total time is divided by @p ndiv in printed output
/// @param enable: enable output printing
/// @ingroup pt
#define PROFILE_N_EN(taken, lambda, ndiv, enable) {\
std::stringstream ID;\
	ID << __PRETTY_FUNCTION__ << ":" << __LINE__-2;\
	auto _start = std::chrono::steady_clock::now ();\
	lambda();\
	auto _end = std::chrono::steady_clock::now ();\
	taken = double(std::chrono::duration_cast<std::chrono::microseconds> (_end - _start).count ())/1e6;\
	if (enable) {\
		if (ndiv == 1) std::cout << ID.str() << ": taken: " << secView(taken) << std::endl;\
		else std::cout << ID.str() << ": total: " << secView(taken) << " each: " << secView(taken/double(ndiv)) << " over " << ndiv << " trials" << std::endl; }}

#ifdef DISABLE_PROFILE_OUTPUT
#define PROFILE_N(taken,lambda,ndiv) PROFILE_N_EN(taken,lambda,ndiv,false)
#else
#define PROFILE_N(taken,lambda,ndiv) PROFILE_N_EN(taken,lambda,ndiv,true)
#endif
#define PROFILE(taken, lambda) PROFILE_N(taken,lambda,1)

template<bool _autostop, bool _output>
class Profiler
{
public:
	Profiler (const std::string &id = ""):
		  _id(id)
	{
		start ();
	}

	void start () {
		_start = now();
	}

	double tick (int n = 1) {
		auto end = now ();
		double taken = static_cast<double> (std::chrono::duration_cast<std::chrono::nanoseconds> (end - _start).count()*1e-9);

		if constexpr (_output)
			dump(taken, n);
		_start = now ();
		return taken;
	}

	~Profiler() {
		if constexpr (_autostop && _output) {
			tick ();
		}
	}

private:
	auto now () const {
		return std::chrono::steady_clock::now ();
	}
	void dump (double taken, int n = 1) {
		std::string pre = (_id.empty() ? std::string ("T") : _id + std::string (": t"));
		if (n == 1)
			std::cout << pre << "aken: " << secView(taken) << std::endl;
		else
			std::cout << pre << "otal: " << secView(taken) << " each " << secView (taken / double(n)) << " over " << n << " trials" << std::endl;
	}

private:
	std::string _id;
	std::chrono::time_point<std::chrono::steady_clock> _start;
};

using Autoprof = Profiler<true, true>;
using Prof = Profiler<false, true>;




} // namespace nlib

#endif // NL_UTILS_H
