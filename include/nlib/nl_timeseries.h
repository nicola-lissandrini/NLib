#ifndef TIMESERIES_H
#define TIMESERIES_H

#include <chrono>
#include <deque>
#include <type_traits>
#include <vector>
#include <type_traits>
#include <optional>
#include "nl_utils.h"

namespace nlib {

template<class T, class Duration = std::chrono::milliseconds>
class DelayedObject
{
	using DestTimedObject = TimedObject<T, std::chrono::system_clock, Duration>;

public:

	template<typename ...Args>
	DelayedObject (const Duration &delay, Args &&...args):
		  _delay(delay),
		  _obj(args...)
	{}

	DelayedObject () = default;

	T &obj () {
		return _obj;
	}

	const T &obj () const {
		return _obj;
	}

	Duration &delay () {
		return _delay;
	}

	const Duration &delay () const {
		return _delay;
	}

	DestTimedObject operator + (const typename DestTimedObject::Time &time) {
		return DestTimedObject (time + delay (), _obj);
	}

#define DELAYED_DELAYED_COMPARISON(op) \
	bool operator op (const DelayedObject<T, Duration> &rhs) const { \
			return delay () op rhs.time (); \
	}

#define DELAYED_TIME_COMPARISON(op) \
	bool operator op (const Duration &rhs) const { \
			return delay () op rhs; \
	}

	DELAYED_DELAYED_COMPARISON(<)
	DELAYED_DELAYED_COMPARISON(<=)
	DELAYED_DELAYED_COMPARISON(>)
	DELAYED_DELAYED_COMPARISON(>=)
	DELAYED_TIME_COMPARISON(<)
	DELAYED_TIME_COMPARISON(<=)
	DELAYED_TIME_COMPARISON(>)
	DELAYED_TIME_COMPARISON(>=)

private:
	T _obj;
	Duration _delay;
};

template<typename Duration>
std::string durationUnit ();

template<> inline std::string durationUnit<std::chrono::seconds> () { return "s"; }
template<> inline std::string durationUnit<std::chrono::milliseconds> () { return "ms"; }
template<> inline std::string durationUnit<std::chrono::microseconds> () { return "us"; }
template<> inline std::string durationUnit<std::chrono::nanoseconds> () { return "ns"; }

template<typename T, typename Duration>
std::ostream &operator << (std::ostream & os, const DelayedObject<T, Duration> &delayed) {
	os << "[" << delayed.delay ().count () << " " << (durationUnit<Duration> ()) << "] " << delayed.obj ();
	return os;
}

template<typename T, typename Duration = std::chrono::microseconds, typename Clock = std::chrono::system_clock>
class Timeseries
{
public:
	using Sample = DelayedObject<T, Duration>;
	using Time = std::chrono::time_point<Clock, Duration>;
	using DataType = std::vector<Sample>;
	using Precision = Duration;
	using iterator = typename DataType::iterator;
	using const_iterator = typename DataType::const_iterator;
	using Neighbors = std::pair<std::optional<Sample>, std::optional<Sample>>;


	iterator begin () { return _timeseries.begin (); }
	iterator end () { return _timeseries.end (); }

	const_iterator begin () const { return _timeseries.begin (); }
	const_iterator end () const { return _timeseries.end (); }

	template<typename OtherTime>
	void setStartTime (const OtherTime &startTime) {
		_startTime = std::chrono::time_point_cast<Duration> (startTime);
	}

	const Sample &operator[] (int i) const {
		return i >= 0 ? _timeseries[i] :
				   *prev (_timeseries.end (), -i);
	}

	Sample &operator[] (int i) {
		return i >= 0 ? _timeseries[i] :
				   *prev (_timeseries.end (), -i);
	}

	void add (const Sample &x) {
		_timeseries.push_back (x);
	}


	T operator () (const Duration &t) 	{
		return at (t);
	}

	T operator () (const Time &t) {
		return at(t);
	}

	Duration totalDuration () {
		return _timeseries.back ().delay ();
	}

	int size () const {
		return _timeseries.size ();
	}

	template<typename OtherTime>
	T at (const OtherTime &t) const {
		assert (_startTime.has_value () && "Must set starting time to access with time");

		return at (std::chrono::time_point_cast<Duration> (t) - *_startTime);
	}

	T at (const Duration &t) const
	{
		auto [before, after] = neighbors (t);

		assert (before.has_value () && after.has_value () && "Supplied time out of the timeseries limits");

		return interpolation (*before, *after, t);
	}

private:
	T interpolation (const Sample &first, const Sample &second, const Duration &t) const {
		float lambda = (t - first.delay()).count () / (float) (second.delay () - first.delay()).count ();

		T diff = second.obj () - first.obj ();

		return first.obj () + lambda * diff;
	}

	Neighbors neighbors (const Duration &t) const {
		auto closest = std::lower_bound (_timeseries.begin (), _timeseries.end (), t);

		if (closest == _timeseries.begin ())
			return {std::nullopt, *closest};
		if (closest == _timeseries.end ())
			return {*std::prev(closest), std::nullopt};
		return {*std::prev(closest), *closest};
	}

public:
	std::optional<Time> _startTime;
	DataType _timeseries;
};

template<typename T, typename Duration>
std::ostream &operator << (std::ostream &os, const Timeseries<T, Duration> &sig) {
	for (const typename Timeseries<T, Duration>::Sample &curr : sig) {
		os << curr << "\n";
	}

	os << "[ Timeseries " << TYPE(T) << " {" << sig.size () << "} ]\n";
	return os;
}
}

#endif // TIMESERIES_H
