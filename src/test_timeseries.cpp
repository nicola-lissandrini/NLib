#include "../include/nlib/nl_timeseries.h"
#include <thread>

using namespace std::literals::chrono_literals;
using namespace std;
using namespace std::chrono;

float interp () {
	using Timeseries = nlib::Timeseries<float, microseconds>;
	using Sample = Timeseries::Sample;

	Timeseries a;

	Sample first(0us, 0);
	Sample second(seconds(1), 100);
	Sample third(2s, 30);

	a.add (first);
	a.add (second);
	a.add (third);

	a.setStartTime (system_clock::now ());
	nlib::Prof prof;
	std::this_thread::sleep_for (500ms);
	prof.tick ();
	return a.at(system_clock::now ());
}

int main ()
{
	float xx =  interp ();

	cout << xx << endl;
}
