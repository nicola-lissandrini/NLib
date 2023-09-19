#ifndef NL_MATH_H
#define NL_MATH_H

#include <memory>
#include <type_traits>
#include <cmath>

namespace nlib {

template<typename T, typename = void>
struct has_round : std::false_type {};

template<typename T>
struct has_round<T, std::void_t<decltype(std::declval<T> () (std::declval<int>()))>> : std::true_type {};


template<typename T, typename = void>
struct has_square : std::false_type {};

template<typename T>
struct has_square<T, std::void_t<decltype(std::declval<T> () [std::declval<int>()])>> : std::true_type {};


// Dot product of gradient by x0.search rotated by 90 deg, not normalized
template<typename T>
std::enable_if_t<has_round<T>::value, float> parallelScore (const T &v1, const T &v2) {
	return std::abs (v1(1) * v2(0) - v1(0) * v2(1));
}

template<typename T>
std::enable_if_t<has_square<T>::value && !has_round<T>::value, float> parallelScore (const T &v1, const T &v2) {
	return std::abs (v1[1] * v2[0] - v1[0] * v2[1]);
}

template <typename T>
T clamp(const T& value, const T& low, const T& high) {
	return std::max(low, std::min(value, high));
}

template<typename T>
float distanceToSegment (const T &p1, const T &p2, const T &p) {
	T diff = p2 - p1;
	float t = (p - p1).dot(diff) / diff.squaredNorm ();
	t = clamp (t, 0.f, 1.f);

	T projection = p1 + t * diff;
	return (p - projection).norm ();
}

} // nlib

#endif // NL_MATH_H
