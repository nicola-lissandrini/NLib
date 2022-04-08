#ifndef NL_UTILS_H
#define NL_UTILS_H

#include <iostream>
#include <cmath>
#include <chrono>
#define BOOST_STACKTRACE_USE_ADDR2LINE
#include <boost/stacktrace.hpp>
#include <boost/optional.hpp>
#include <map>
#include <sstream>
#include <dlfcn.h>

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

/**
 * @section Shared ptr tools
 */

/// @brief define nested Classname::Ptr symbol for shared_ptr of the class
/// To be used inside class definition
#define DEF_SHARED(classname) using Ptr = std::shared_ptr<classname>;
/// @brief define concat ClassnamePtr symbol for shared_ptr of the class
/// To be used outside class definition
#define DEF_SHARED_CAT(classname) using classname##Ptr = std::shared_ptr<classname>;


/**
 * @defgroup gpt General purpose tools
 */

/// @brief Range struct with convenience methods
/// @param T any literal type that can be converted to floating point
/// @ingroup gpt
template<typename T>
struct RangeBase {
	T min, max;
	boost::optional<T> step;

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
    taken = double(std::chrono::duration_cast<std::chrono::microseconds> (_end - _start).count ())/1e3;\
    if (enable) {\
	    if (ndiv == 1) std::cout << ID.str() << ": taken: " << taken << "ms" << std::endl;\
	    else std::cout << ID.str() << ": total: " << taken << "ms each: " << taken/double(ndiv) << "ms over " << ndiv << " trials" << std::endl; }}

#ifdef DISABLE_PROFILE_OUTPUT
#define PROFILE_N(taken,lambda,ndiv) PROFILE_N_EN(taken,lambda,ndiv,false)
#else
#define PROFILE_N(taken,lambda,ndiv) PROFILE_N_EN(taken,lambda,ndiv,true)
#endif
#define PROFILE(taken, lambda) PROFILE_N(taken,lambda,1)



}

#endif // NL_UTILS_H
