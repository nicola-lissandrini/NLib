#ifndef NL_PARAMS_H
#define NL_PARAMS_H

#include <boost/optional.hpp>
#include <typeinfo>
#include <string>
#include <sstream>
#include <cxxabi.h>
#include <boost/filesystem.hpp>
#include <xmlrpcpp/XmlRpc.h>
#include <iterator>

#include "nl_utils.h"

namespace nlib {

/**
 * @brief Fake container for unified vector and scalar _params.
 * Intended for internal use
 * @param T any type
 */
template<typename T>
class empty_container {
public:
	/// @{
	/// @name Standard container definitions
	/// @see https://en.cppreference.com/w/cpp/named_req/Container
	using value_type = T;
	using reference = T&;
	using const_reference = const T&;
	using wrapper_type = std::array<T, 1>;
	using iterator = typename wrapper_type::iterator;
	using const_iterator = typename wrapper_type::const_iterator;
	using difference_type = typename wrapper_type::difference_type;
	using size_type = typename wrapper_type::size_type;
	/// @}
	empty_container (const T &_value = T());
	empty_container (size_t size);

	operator T ();

	T &operator = (const T &_value);
	bool operator == (const empty_container<T> &b);
	bool operator != (const empty_container<T> &b);

	iterator begin ();
	iterator end ();
	const_iterator cbegin() const;
	const_iterator cend() const;
	size_type size () const;
	size_type max_size () const;
	bool empty () const;

private:
	wrapper_type value;
};

inline char const *xmlRpcErrorStrings[] = {
    "TypeInvalid",
    "TypeBoolean",
    "TypeInt",
    "TypeDouble",
    "TypeString",
    "TypeDateTime",
    "TypeBase64",
    "TypeArray",
    "TypeStruct"
};


template<class T, class U>
boost::optional<T> optional_cast (U &&u) {
	return u ? T(*std::forward<U>(u)) : static_cast<boost::optional<T>> (boost::none);
}

template<typename T, template<typename ...> class container = empty_container>
struct Type;

/**
 * @brief Coveniently handle any type of parameter with error check and debugging info
 */
class NlParams
{
	using c_str = const char *;
	using XmlParamsPtr = std::shared_ptr<XmlRpc::XmlRpcValue>;

	XmlRpc::XmlRpcValue resolveName(const boost::optional<std::string> &name) const;

	void throwErrorType (XmlRpc::XmlRpcValue::Type gotType,
					 const std::string &expected,
					 const boost::optional<std::string> &tagName = boost::none) const;

	void throwErrorEnum (const std::string &value,
					 const boost::optional<std::string> &tagName = boost::none) const;

	void throwErrorResolution (const std::string &name) const;

	template<typename T, template<typename ...> class container>
	container<T> get (XmlRpc::XmlRpcValue &_params,
				   const c_str &name,
				   const boost::optional<int> &index = boost::none) const;

	template<typename T, template<typename ...> class container>
	container<T> get (XmlRpc::XmlRpcValue &_params,
				   const boost::optional<std::string> &name = boost::none,
				   const boost::optional<int> &index = boost::none) const;

	template<typename T>
	T enumFind (const std::string &value,
			  const std::vector<std::string> &values,
			  const boost::optional<std::string> &name) const;

	std::string getFullPath (const std::string &base) const;

public:
	NlParams () = default;
	NlParams (const XmlRpc::XmlRpcValue &params, const std::string &name = "", const NlParams *parent = nullptr);
	NlParams (const NlParams &) = default;
	void setParams (XmlRpc::XmlRpcValue &params);

	NlParams &operator = (XmlRpc::XmlRpcValue &params);
	NlParams &operator = (const NlParams &) = default;
	NlParams operator [] (const std::string &name) const;

	  // Overload for literal and string types
	template<typename T>
	T get (const c_str &name,
		  const boost::optional<T> &defaultValue = boost::none,
		  const boost::optional<int> &index = boost::none) const;

	template<typename T>
	T get (const boost::optional<std::string> &name,
		  const boost::optional<T> &defaultValue = boost::none,
		  const boost::optional<int> &index = boost::none) const;

	  // Overload for vectors of literal and string types
	template<typename T, template<typename ...> class container>
	container<T> get (const c_str &name,
				   const boost::optional<container<T>> &defaultValue = boost::none,
				   const boost::optional<int> &index = boost::none) const;

	template<typename T, template<typename ...> class container>
	container<T> get (const boost::optional<std::string> &name,
				   const boost::optional<container<T>> &deaultValue = boost::none,
				   const boost::optional<int> &index = boost::none) const;

	  // Overload for enums
	template<typename T>
	T get (const c_str &name,
		  const std::vector<c_str> &values,
		  const boost::optional<T> &defaultValue = boost::none,
		  const boost::optional<int> &index = boost::none) const;

	template<typename T>
	T get (const boost::optional<std::string> &name,
		  const std::vector<std::string> &values,
		  const boost::optional<T> &defaultValue = boost::none,
		  const boost::optional<int> &index = boost::none) const;

	  // Overload for vectors of enums
	template<typename T, template<typename ...> class container>
	container<T> get (const c_str &name,
				   const std::vector<c_str> &values,
				   const boost::optional<container<T>> &defaultValue = boost::none,
				   const boost::optional<int> &index = boost::none) const;

	template<typename T, template<typename ...> class container>
	container<T> get (const boost::optional<std::string> &name,
				   const std::vector<std::string> &values,
				   const boost::optional<container<T>> &defaultValue = boost::none,
				   const boost::optional<int> &index = boost::none) const;

	template<typename T, template<typename ...> class container>
	friend class Type;

	DEF_SHARED(NlParams)
private:
	XmlRpc::XmlRpcValue _params;
	std::string _name;
	const NlParams *_parent;
};

inline XmlRpc::XmlRpcValue NlParams::resolveName (const boost::optional<std::string> &name) const
{
	// Need to resort to pointers due to bad programming of external lib XmlRpcValue
	const XmlRpc::XmlRpcValue *param = &_params;

	if (!name.has_value ())
		return param;

	boost::filesystem::path namePath(*name);

	  // relative_path: remove leading '/' if any
	for (const auto &currentName : namePath.relative_path ()) {
		const XmlRpc::XmlRpcValue &currentParam = *param;

		try {
			param = &currentParam[currentName.string()];
		} catch (XmlRpc::XmlRpcException e) {
			throwErrorResolution (getFullPath (*name));
		}
	}

	return *param;
}

inline NlParams NlParams::operator [](const std::string &name) const {
	return {resolveName (name), name, this};
}


template<typename T, template<typename ...> class container>
container<T> NlParams::get (XmlRpc::XmlRpcValue &_params,
					   const c_str &name,
					   const boost::optional<int> &index) const
{
	return get<T, container> (_params, std::string (name), index);
}

template<typename T, template<typename ...> class container>
container<T> NlParams::get (XmlRpc::XmlRpcValue &_params,
					   const boost::optional<std::string> &name,
					   const boost::optional<int> &index) const
{
	XmlRpc::XmlRpcValue param;

	if (index.has_value ()) {
		if (_params.getType () != XmlRpc::XmlRpcValue::TypeArray)
			throwErrorType (_params.getType (), "array");

		param = _params[*index];
	} else
		param = _params;

	Type<T, container> currentType{this};

	if (!currentType.checkType (param.getType ()))
		throwErrorType (param.getType (), typeid(T).name (), name);

	container<T> ret;

	try {
		ret = currentType.convert (param);
#ifdef TORCH_API
	} catch (const torch::Error &e) {
		throw torch::Error ("While processing param " + getFullPath(name?*name:"") + "\n" + e.msg (),e.backtrace (), e.caller ());
	}
#else
		} catch (...) {}
#endif

	return ret;
}

template<typename T>
T NlParams::get (const c_str &name,
			  const boost::optional<T> &defaultValue,
			  const boost::optional<int> &index) const {
	return get<T> (std::string(name), defaultValue, index);
}

template<typename T>
T NlParams::get (const boost::optional<std::string> &name,
			  const boost::optional<T> &defaultValue,
			  const boost::optional<int> &index) const {
	return get<T, empty_container> (name, optional_cast<empty_container<T>> (defaultValue), index);
}

template<typename T, template<typename ...> class container>
container<T> NlParams::get (const c_str &name,
					   const boost::optional<container<T>> &defaultValue,
					   const boost::optional<int> &index) const
{
	return get<T, container> (std::string(name), defaultValue, index);
}

template<typename T, template<typename ...> class container>
container<T> NlParams::get (const boost::optional<std::string> &name,
					   const boost::optional<container<T>> &defaultValue,
					   const boost::optional<int> &index) const
{
	XmlRpc::XmlRpcValue param;

	try {
		param = resolveName (name);
	} catch (const XmlRpc::XmlRpcException &e) {
		if (defaultValue.has_value ())
			return *defaultValue;
		else
			throw e;
	}

	if (param.getType () == XmlRpc::XmlRpcValue::TypeInvalid && defaultValue.has_value ())
		return *defaultValue;

	return get<T, container> (param, name, index);
}

template<typename T>
T NlParams::get (const c_str &name,
			  const std::vector<c_str> &values,
			  const boost::optional<T> &defaultValue,
			  const boost::optional<int> &index) const {
	return get<T> (std::string (name), std::vector<std::string> (values.begin (), values.end ()), defaultValue, index);
}

template<typename T>
T NlParams::get (const boost::optional<std::string> &name,
			  const std::vector<std::string> &values,
			  const boost::optional<T> &defaultValue,
			  const boost::optional<int> &index) const
{
	return get<T, empty_container> (name, values, optional_cast<empty_container<T>> (defaultValue), index);
}

template<typename T>
T NlParams::enumFind (const std::string &value, const std::vector<std::string> &values, const boost::optional<std::string> &name) const
{
	auto selected = std::find (values.begin (), values.end (), value);

	if (selected == values.end ())
		throwErrorEnum (value, name);

	return static_cast<T> (std::distance (values.begin (), selected));
}


template<typename T, template<typename ...> class container>
container<T> NlParams::get (const c_str &name,
					   const std::vector<c_str> &values,
					   const boost::optional<container<T>> &defaultValue,
					   const boost::optional<int> &index) const
{
	return get<T, container> (std::string (name), std::vector<std::string> (values.begin (), values.end ()), defaultValue, index);
}

template<typename T, template<typename ...> class container>
container<T> NlParams::get (const boost::optional<std::string> &name,
					   const std::vector<std::string> &values,
					   const boost::optional<container<T>> &defaultValue,
					   const boost::optional<int> &index) const
{
	if (!resolveName (name).valid () && defaultValue.has_value ())
		return *defaultValue;

	container<std::string> stringValues = get<std::string, container> (name, boost::none, index);
	container<T> enumValues(stringValues.size ());

	typename container<T>::iterator enumValueIt = enumValues.begin ();

	for (const std::string &stringValue : stringValues) {
		*enumValueIt = enumFind<T> (stringValue, values, name);
		enumValueIt++;
	}

	return enumValues;
}

inline void NlParams::throwErrorType (XmlRpc::XmlRpcValue::Type gotType,
							   const std::string &expected,
							   const boost::optional<std::string> &tagName) const
{
	std::string displayTagName = getFullPath (tagName.has_value () ? *tagName: "<unnamed>");
	if (gotType == XmlRpc::XmlRpcValue::TypeInvalid)
		throwErrorResolution (displayTagName);
	std::stringstream msg;

	msg << "Invalid datatype in tag '" << displayTagName << "'. "
	    << "Got " << xmlRpcErrorStrings[(int) gotType] << " for param of type " << expected << ".";

	throw XmlRpc::XmlRpcException (msg.str ());
}

inline void NlParams::throwErrorResolution (const std::string &name) const {
	std::stringstream msg;

	msg << "Parameter '" << name << "' could not be found";

	throw XmlRpc::XmlRpcException (msg.str ());
}

inline std::string NlParams::getFullPath (const std::string &base) const
{
	if (_parent == nullptr)
		return "/" + _name + "/" + base;

	return _parent->getFullPath (_name) + "/" + base;
}

inline void NlParams::throwErrorEnum (const std::string &value, const boost::optional<std::string> &tagName) const
{
	std::stringstream msg;

	msg << "Invalid enum value '" << value << "' in tag '" << (tagName.has_value () ? *tagName: "<unnamed>") << "";

	throw XmlRpc::XmlRpcException (msg.str());
}

inline NlParams::NlParams (const XmlRpc::XmlRpcValue &params, const std::string &name, const NlParams *parent):
	 _params(params),
	 _name(name),
	 _parent(parent)
{
}

inline void NlParams::setParams (XmlRpc::XmlRpcValue &params) {
	_params = params;
}

inline NlParams &NlParams::operator = (XmlRpc::XmlRpcValue &params)
{
	setParams (params);

	return *this;
}

/* Standard types: literals and strings */

template<typename T, template<typename ...> class container>
struct Type {
	static bool checkType (XmlRpc::XmlRpcValue::Type type);
	container<T> convert (XmlRpc::XmlRpcValue &param) const;

	const NlParams *const _parent;
};

template<typename T>
struct Type<T, empty_container> {
	static bool checkType (XmlRpc::XmlRpcValue::Type type);
	empty_container<T> convert (XmlRpc::XmlRpcValue &param) const;

	const NlParams *const _parent;
};

template<>
inline bool Type<int>::checkType (XmlRpc::XmlRpcValue::Type type) {
	return type == XmlRpc::XmlRpcValue::TypeInt;
}

template<>
inline bool Type<bool>::checkType (XmlRpc::XmlRpcValue::Type type) {
	return type == XmlRpc::XmlRpcValue::TypeBoolean;
}

template<>
inline bool Type<std::string>::checkType (XmlRpc::XmlRpcValue::Type type) {
	return type == XmlRpc::XmlRpcValue::TypeString;
}

template<>
inline bool Type<double>::checkType (XmlRpc::XmlRpcValue::Type type) {
	return Type<int>::checkType (type) ||
		  (type == XmlRpc::XmlRpcValue::TypeDouble);
}
template<>
inline bool Type<float>::checkType (XmlRpc::XmlRpcValue::Type type) {
	return Type<double>::checkType (type);
}

template<>
inline bool Type<Range>::checkType (XmlRpc::XmlRpcValue::Type type) {
	return type == XmlRpc::XmlRpcValue::TypeStruct;
}

template<>
inline empty_container<Range> Type<Range>::convert (XmlRpc::XmlRpcValue &param) const {
	Range range;

	range.min = _parent->get<float, empty_container> (param["min"],"min");
	range.max = _parent->get<float, empty_container> (param["max"],"max");
	try {
		range.step = _parent->get<float, empty_container> (param["step"],"step");
	} catch (...) { }

	return range;
}

template<typename T>
empty_container<T> Type<T>::convert (XmlRpc::XmlRpcValue &param) const {
	return static_cast<T> (param);
}

template<>
inline empty_container<double> Type<double>::convert (XmlRpc::XmlRpcValue &param) const {
	switch (param.getType ()) {
	case XmlRpc::XmlRpcValue::TypeInt:
		return static_cast<double> (static_cast<int> (param));
	case XmlRpc::XmlRpcValue::TypeDouble:
		return static_cast<double> (param);
	}
}

template<>
inline empty_container<float> Type<float>::convert (XmlRpc::XmlRpcValue &param) const {
	Type<double> doubleType{_parent};
	return static_cast<float> (doubleType.convert (param));
}

#ifdef TORCH_API
template<>
inline bool Type<torch::Tensor>::checkType (XmlRpc::XmlRpcValue::Type type) {
	return Type<int>::checkType (type)
		  || Type<float>::checkType (type)
		  || (type == XmlRpc::XmlRpcValue::TypeArray);
}

template<>
inline empty_container<torch::Tensor> Type<torch::Tensor>::convert (XmlRpc::XmlRpcValue &param) const {
	switch (param.getType ()) {
	case XmlRpc::XmlRpcValue::TypeInt:
	case XmlRpc::XmlRpcValue::TypeDouble: {
		float value = Type<double> {_parent}.convert (param);
		return torch::tensor ({value}, torch::kFloat);
	}
	case XmlRpc::XmlRpcValue::TypeArray: {
		int size = param.size ();
		torch::Tensor tensor;
		for (int i = 0; i < size; i++) {
			torch::Tensor inner = _parent->get<torch::Tensor, empty_container> (param, boost::none, i);
			if (i == 0) {
				std::vector<int64_t> sizes{size};
				sizes.insert (sizes.end (), inner.sizes ().begin (), inner.sizes().end ());

				tensor = torch::empty (sizes, inner.options ());
			} // if subsequent elements have different number of elements an exception is raised directly by torch

			tensor[i] = inner;
		}
		return tensor.squeeze();
	}
	}
}
#endif // TORCH_API

/* Container types */

template<typename T>
struct Type<T, std::vector> {
	static bool checkType (XmlRpc::XmlRpcValue::Type type);
	std::vector<T> convert (XmlRpc::XmlRpcValue &param) const;

	const NlParams *const _parent;
};

template<typename T>
bool Type<T, std::vector>::checkType (XmlRpc::XmlRpcValue::Type type) {
	return type == XmlRpc::XmlRpcValue::TypeArray;
}

template<typename T>
std::vector<T> Type<T, std::vector>::convert (XmlRpc::XmlRpcValue &param) const
{
	std::vector<T> array(param.size ());

	for (int i = 0; i < param.size (); i++)
		array[i] = _parent->get<T, empty_container> (param, boost::none, i);

	return array;
}

//#ifdef TORCH_API



//#endif









/* empty_container definitions */

template<typename T>
empty_container<T>::empty_container (const T &_value):
	 value({_value})
{}

template<typename T>
empty_container<T>::empty_container (size_t size) {
	assert (size == 1 && "Invalid initialization of empty_container");
}

template<typename T>
empty_container<T>::operator T() {
	return value.front ();
}

template<typename T>
T &empty_container<T>::operator =(const T &_value) {
	value.front() = _value;
	return value.front();
}


template<typename T>
bool empty_container<T>::operator == (const empty_container<T> &b) {
	return value == b.value;
}

template<typename T>
bool empty_container<T>::operator != (const empty_container<T> &b) {
	return value != b.value;
}

template<typename T>
typename empty_container<T>::iterator
    empty_container<T>::begin () {
	return value.begin ();
}

template<typename T>
typename empty_container<T>::iterator
    empty_container<T>::end () {
	return value.end ();
}

template<typename T>
typename empty_container<T>::const_iterator
    empty_container<T>::cbegin () const {
	return value.cbegin ();
}

template<typename T>
typename empty_container<T>::const_iterator
    empty_container<T>::cend () const {
	return value.cend ();
}

template<typename T>
typename empty_container<T>::size_type
    empty_container<T>::size () const {
	return value.size ();
}

template<typename T>
typename empty_container<T>::size_type
    empty_container<T>::max_size () const {
	return value.max_size ();
}

template<typename T>
bool empty_container<T>::empty () const {
	return value.empty ();
}

}
#endif // NL_PARAMS_H
