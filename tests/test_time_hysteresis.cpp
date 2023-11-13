#include "../include/nlib/nl_utils.h"
#include <chrono>
#include <thread>

template<typename Clock = std::chrono::steady_clock, typename Time = typename Clock::time_point, typename Duration = std::chrono::milliseconds>
class TimeHysteresis {
public:
	TimeHysteresis (const Duration &threshold, const Duration &lockout):
		  _threshold(threshold),
		  _lockout(lockout)
	{
		_lastEventTime = Clock::now ();
		_lastTriggerTime = Clock::now () - lockout;
	}

	bool checkAndUpdate () {
		Time currentTime = Clock::now ();
		Duration sinceEvent = std::chrono::duration_cast<Duration> (currentTime - _lastEventTime);
		Duration sinceTrigger = std::chrono::duration_cast<Duration> (currentTime - _lastTriggerTime);

		if (sinceEvent > _threshold && sinceTrigger > _lockout) {
			_lastEventTime = currentTime;
			_lastTriggerTime = currentTime;

			return true;
		}

		if (sinceEvent > _threshold) {
			_lastEventTime = currentTime;
			std::cout << "Hysteresys out of threshold" << std::endl;
		}

		return false;
	}

	DEF_SHARED(TimeHysteresis)

private:
	Time _lastEventTime;
	Time _lastTriggerTime;
	Duration _threshold;
	Duration _lockout;
};

using namespace std::literals::chrono_literals;
using namespace std;

int main () {
	TimeHysteresis hysteresis (500ms, 1200ms);
	auto start = chrono::steady_clock::now ();

	while (true) {
		if (hysteresis.checkAndUpdate ()) {
			cout << "Event triggered" << endl;
		}

		this_thread::sleep_for(100ms);
		cout << chrono::duration_cast<chrono::milliseconds> (chrono::steady_clock::now () - start).count () << endl;
	}
}


