#ifndef NL_UTILS_H
#define NL_UTILS_H

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

template<typename T, typename Status, Status ...defaultValue>
class AlgorithmResult
{
	static_assert (sizeof ...(defaultValue) <= 1, "defaultValue must be either 0 or 1 element");

public:
	AlgorithmResult (const T &value): _result(value) { }
	AlgorithmResult (const Status &error): _result(error) {}
	AlgorithmResult () = delete;  // The default initialization of std::variant allocates an object of type T, we don't want that

	bool success () const {
		return std::holds_alternative<T> (_result);
	}

	operator bool () const {
		return success ();
	}

	T value () const {
		return std::get<T> (_result);
	}

	Status status () const {
		if (success() && hasDefault ())
			return defaultStatus();

		return std::get<Status> (_result);
	}

private:
	constexpr bool hasDefault () const {
		return sizeof ...(defaultValue) == 1;
	}

	constexpr Status defaultStatus () const {
		return std::get<0> (_defaultValue);
	}

private:
	std::variant<T, Status> _result;
	constexpr static auto _defaultValue = std::tuple (defaultValue...);
};
#endif //  __cplusplus >= 201703L


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



}

#endif // NL_UTILS_H
