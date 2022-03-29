#ifndef EXAMPLE_MODFLOW_H
#define EXAMPLE_MODFLOW_H

#include <nlib/nl_modflow.h>

class ExampleModFlow : public nlib::NlModFlow
{
public:
	ExampleModFlow ():
		 NlModFlow ()
	{}

	void loadModules () override;

	DEF_SHARED (ExampleModFlow)
};

class Module1 : public nlib::NlModule
{
	struct Params {
		int integer;
		bool boolean;
	};

public:
	Module1 (nlib::NlModFlow *modFlow):
		 nlib::NlModule (modFlow, "module_1")
	{}

	void initParams (const nlib::NlParams &nlParams) override;
	void setupNetwork () override;

	void processInteger (int value);

	DEF_SHARED (Module1)

private:
	Params _params;
	int _seq;
};

class Module2 : public nlib::NlModule
{
	struct Params {
		std::string stringParam;
	};

public:
	Module2 (nlib::NlModFlow *modFlow):
		 nlib::NlModule (modFlow, "module_2")
	{}

	void initParams (const nlib::NlParams &nlParams) override;
	void setupNetwork () override;

	void processString (const std::string &value);

	DEF_SHARED (Module2)

public:
	Params _params;
};

class Module3 : public nlib::NlModule
{
public:
	Module3  (nlib::NlModFlow  *modFlow):
		 nlib::NlModule (modFlow, "module_3")
	{}

	void initParams (const nlib::NlParams &nlParams) override {}
	void setupNetwork () override;

	void updateInteger (int value);
	void updateString (const std::string &value);

	DEF_SHARED (Module3)

private:
	int _integer;
	std::string _string;
};

#endif // EXAMPLE_MODFLOW_H
























