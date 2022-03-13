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
 * @brief Fake container for unified vector and scalar params.
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
	empty_container (const T &_value);
	empty_container ();

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
/**
 * @brief Coveniently handle any type of parameter with error check and debugging info
 */
class NlParams
{
	using c_str = const char *;
	using XmlParamsPtr = std::shared_ptr<XmlRpc::XmlRpcValue>;

	template<typename T, template<typename ...> typename container = empty_container>
	struct Type {
		static bool checkType (XmlRpc::XmlRpcValue::Type type);
		static container<T> convert (XmlRpc::XmlRpcValue &param);
	};

	XmlRpc::XmlRpcValue resolveName(const boost::optional<c_str> &name) const;

	static void throwErrorType (XmlRpc::XmlRpcValue::Type gotType,
						   const std::string &expected,
						   const boost::optional<c_str> &tagName = boost::none);

	static void throwErrorEnum (const std::string &value,
						   const boost::optional<c_str> &tagName = boost::none);

	template<typename T, template<typename ...> typename container>
	static container<T> get (XmlRpc::XmlRpcValue &params,
						const boost::optional<c_str> &name,
						const boost::optional<int> &index = boost::none);
	template<typename T>
	static T enumFind (const std::string &value,
				    const std::vector<c_str> &values,
				    const boost::optional<c_str> &name);

public:
	NlParams () = default;
	NlParams (XmlRpc::XmlRpcValue &_params);
	NlParams (const XmlRpc::XmlRpcValue &&_params);
	NlParams (const NlParams &_nlParams) = default;
	void setParams (XmlRpc::XmlRpcValue &_params);

	NlParams &operator = (XmlRpc::XmlRpcValue &_params);
	NlParams operator [](const std::string &name) const;

	   // Overload for literal and string types
	template<typename T>
	T get (const boost::optional<c_str> &name = boost::none,
		  const boost::optional<T> &defaultValue = boost::none,
		  const boost::optional<int> &index = boost::none) const;

	   // Overload for vectors of literal and string types
	template<typename T, template<typename ...> typename container>
	container<T> get (const boost::optional<c_str> &name = boost::none,
				   const boost::optional<container<T>> &defaultValue = boost::none,
				   const boost::optional<int> &index = boost::none) const;

	   // Overload for enums
	template<typename T>
	T get (const boost::optional<c_str> &name,
		  const std::initializer_list<c_str> &values,
		  const boost::optional<T> &defaultValue = boost::none,
		  const boost::optional<int> &index = boost::none) const;

	   // Overload for vectors of enums
	template<typename T, template<typename ...> typename container>
	container<T> get (const boost::optional<c_str> &name,
				   const std::initializer_list<c_str> &values,
				   const boost::optional<container<T>> &defaultValue = boost::none,
				   const boost::optional<int> &index = boost::none) const;

	DEF_SHARED(NlParams)
private:
	XmlRpc::XmlRpcValue params;
};

inline XmlRpc::XmlRpcValue NlParams::resolveName (const boost::optional<c_str> &name) const
{
	// Need to resort to pointers due to bad programming of external lib XmlRpcValue
	const XmlRpc::XmlRpcValue *param = &params;

	if (!name.has_value ())
		return param;

	boost::filesystem::path namePath(*name);

	   // relative_path: remove leading '/' if any
	for (const auto &currentName : namePath.relative_path ()) {
		const XmlRpc::XmlRpcValue &currentParam = *param;

		param = &currentParam[currentName.string()];
	}

	return *param;
}

inline NlParams NlParams::operator [](const std::string &name) const {
	return resolveName (name.c_str ());
}

template<typename T, template<typename ...> typename container>
container<T> NlParams::get (XmlRpc::XmlRpcValue &params,
					   const boost::optional<c_str> &name,
					   const boost::optional<int> &index)
{
	XmlRpc::XmlRpcValue param;

	if (index.has_value ()) {
		if (params.getType () != XmlRpc::XmlRpcValue::TypeArray)
			throwErrorType (params.getType (), "array");

		param = params[*index];
	} else
		param = params;

	Type<T, container> currentType;

	if (!currentType.checkType (param.getType ()))
		throwErrorType (param.getType (), typeid(T).name (), name);

	return currentType.convert (param);
}

template<typename T>
T NlParams::get (const boost::optional<c_str> &name,
			  const boost::optional<T> &defaultValue,
			  const boost::optional<int> &index) const {
	return get<T, empty_container> (name, optional_cast<empty_container<T>> (defaultValue), index);
}

template<typename T, template<typename ...> typename container>
container<T> NlParams::get (const boost::optional<c_str> &name,
					   const boost::optional<container<T>> &defaultValue,
					   const boost::optional<int> &index) const
{
	XmlRpc::XmlRpcValue param = resolveName (name);

	if (param.getType () == XmlRpc::XmlRpcValue::TypeInvalid && defaultValue.has_value ())
		return *defaultValue;

	return get<T, container> (param, name, index);
}

template<typename T>
T NlParams::get (const boost::optional<c_str> &name,
			  const std::initializer_list<c_str> &values,
			  const boost::optional<T> &defaultValue,
			  const boost::optional<int> &index) const
{
	return get<T, empty_container> (name, values, defaultValue, index);
}

template<typename T>
T NlParams::enumFind (const std::string &value, const std::vector<c_str> &values, const boost::optional<c_str> &name)
{
	auto selected = std::find (values.begin (), values.end (), value);

	if (selected == values.end ())
		throwErrorEnum (value, name);

	return static_cast<T> (std::distance (values.begin (), selected));
}


template<typename T, template<typename ...> typename container>
container<T> NlParams::get (const boost::optional<c_str> &name,
					   const std::initializer_list<c_str> &values,
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
							   const boost::optional<c_str> &tagName)
{
	std::stringstream msg;

	msg << "Invalid datatype in tag '" << (tagName.has_value () ? *tagName: "<unnamed>") << "'. "
	    << "Got " << xmlRpcErrorStrings[(int) gotType] << " for param of type " << expected << ".";

	throw XmlRpc::XmlRpcException (msg.str ());
}

inline void NlParams::throwErrorEnum(const std::string &value, const boost::optional<c_str> &tagName)
{
	std::stringstream msg;

	msg << "Invalid enum value '" << value << "' in tag '" << (tagName.has_value () ? *tagName: "<unnamed>") << "";

	throw XmlRpc::XmlRpcException (msg.str());
}

inline NlParams::NlParams (XmlRpc::XmlRpcValue &_params):
	 params(_params)
{}

inline NlParams::NlParams (const XmlRpc::XmlRpcValue &&_params):
	 params(_params)
{}

inline void NlParams::setParams (XmlRpc::XmlRpcValue &_params) {
	params = _params;
}

inline NlParams &NlParams::operator = (XmlRpc::XmlRpcValue &_params)
{
	setParams (_params);

	return *this;
}

/* Standard types: literals and strings */

template<typename T>
struct NlParams::Type<T, empty_container> {
	static bool checkType (XmlRpc::XmlRpcValue::Type type);
	static empty_container<T> convert (XmlRpc::XmlRpcValue &param);
};

template<>
inline bool NlParams::Type<int>::checkType (XmlRpc::XmlRpcValue::Type type) {
	return type == XmlRpc::XmlRpcValue::TypeInt;
}

template<>
inline bool NlParams::Type<bool>::checkType (XmlRpc::XmlRpcValue::Type type) {
	return type == XmlRpc::XmlRpcValue::TypeBoolean;
}

template<>
inline bool NlParams::Type<std::string>::checkType (XmlRpc::XmlRpcValue::Type type) {
	return type == XmlRpc::XmlRpcValue::TypeString;
}

template<>
inline bool NlParams::Type<double>::checkType (XmlRpc::XmlRpcValue::Type type) {
	return Type<int>::checkType (type) ||
		  (type == XmlRpc::XmlRpcValue::TypeDouble);
}
template<>
inline bool NlParams::Type<float>::checkType (XmlRpc::XmlRpcValue::Type type) {
	return Type<double>::checkType (type);
}

template<typename T>
empty_container<T> NlParams::Type<T>::convert (XmlRpc::XmlRpcValue &param) {
	return static_cast<T> (param);
}

template<>
inline empty_container<double> NlParams::Type<double>::convert (XmlRpc::XmlRpcValue &param) {
	switch (param.getType ()) {
	case XmlRpc::XmlRpcValue::TypeInt:
		return static_cast<double> (static_cast<int> (param));
	case XmlRpc::XmlRpcValue::TypeDouble:
		return static_cast<double> (param);
	}
}

template<>
inline empty_container<float> NlParams::Type<float>::convert (XmlRpc::XmlRpcValue &param) {
	return static_cast<float> (Type<double>::convert(param));
}

/* Container types */

template<typename T>
struct NlParams::Type<T, std::vector> {
	static bool checkType (XmlRpc::XmlRpcValue::Type type);
	std::vector<T> convert (XmlRpc::XmlRpcValue &param);
};

template<typename T>
bool NlParams::Type<T, std::vector>::checkType (XmlRpc::XmlRpcValue::Type type) {
	return type == XmlRpc::XmlRpcValue::TypeArray;
}

template<typename T>
std::vector<T> NlParams::Type<T, std::vector>::convert (XmlRpc::XmlRpcValue &param)
{
	std::vector<T> array(param.size ());

	for (int i = 0; i < param.size (); i++)
		array[i] = get<T, empty_container> (param, boost::none, i);

	return array;
}

/* empty_container definitions */

template<typename T>
empty_container<T>::empty_container (const T &_value):
	 value({_value})
{}

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
