#ifndef NL_MULTIARRAY_ROS_H
#define NL_MULTIARRAY_ROS_H

#include <std_msgs/Float64MultiArray.h>
#include <std_msgs/Float32MultiArray.h>

namespace nlib {

template<class MultiArray>
class MultiArrayManager
{
	using value_type = typename MultiArray::_data_type::value_type;
	using array_type = typename MultiArray::_data_type;

	void create (std::vector<int> sizes, int dataOffset = 0);
	int getIndex (std::vector<int> indexes);

public:
	/**
	 * @brief Create a FloatXXMultiArray given sizes and data offset
	 * @param sizes Sizes of the multi-array
	 * @param dataOffset [optional] extra data size
	 */
	MultiArrayManager (std::vector<int> sizes, int dataOffset = 0);
	MultiArrayManager (const MultiArray &other);

	/**
	 * @brief Get value given indexes
	 * @param indexes
	 * @return array[indexes]
	 */
	value_type get (std::vector<int> indexes);
	/**
	 * @brief set array[indexes] = value
	 * @param indexes
	 * @param value
	 */
	void set (std::vector<int> indexes, value_type value);

	/**
	 * @brief Raw pointer to allocated memory storing data
	 * @return
	 */
	value_type *data();
	/**
	 * @brief Stored data as std::vector
	 */
	array_type &array();

	/**
	 * @brief Get size at dimension i
	 * @param i
	 * @return
	 */
	int size (int i);

	/**
	 * @brief Get FloatXXMultiArray ROS message
	 * @return
	 */
	MultiArray msg ();

private:
	MultiArray _msg;
};

using MultiArray32Manager = MultiArrayManager <std_msgs::Float32MultiArray>;
using MultiArray64Manager = MultiArrayManager <std_msgs::Float64MultiArray>;

template<class MultiArray>
void MultiArrayManager<MultiArray>::create (std::vector<int> sizes, int dataOffset) {
	int dimensions = sizes.size ();
	_msg.layout.dim.resize (dimensions);
	_msg.layout.data_offset = dataOffset;

	for (int i = dimensions-1; i >= 0; i--) {
		_msg.layout.dim[i].size = sizes[i];
		if (i < dimensions - 1)
			_msg.layout.dim[i].stride = sizes[i] * _msg.layout.dim[i+1].stride;
		else
			_msg.layout.dim[i].stride = sizes[i];
	}

	_msg.data.resize (_msg.layout.dim[0].stride + dataOffset);
}

template<class MultiArray>
int MultiArrayManager<MultiArray>::getIndex (std::vector<int> indexes) {
	int numDims = _msg.layout.dim.size ();
	int index = _msg.layout.data_offset;

	for (int i = 0; i < numDims; i++) {
		int currIndex = indexes[i];
		if (i == numDims - 1)
			index += currIndex;
		else
			index += currIndex * _msg.layout.dim[i+1].stride;
	}

	return index;
}

template<class MultiArray>
MultiArrayManager<MultiArray>::MultiArrayManager(std::vector<int> sizes, int dataOffset) {
	create (sizes, dataOffset);
}

template<class MultiArray>
MultiArrayManager<MultiArray>::MultiArrayManager(const MultiArray &other) {
	_msg = other;
}

template<class MultiArray>
typename MultiArrayManager<MultiArray>::value_type
    MultiArrayManager<MultiArray>::get(std::vector<int> indexes) {
	return _msg.data[getIndex (indexes)];
}

template<class MultiArray>
void MultiArrayManager<MultiArray>::set(std::vector<int> indexes, value_type value) {
	_msg.data[getIndex (indexes)] = value;
}

template<class MultiArray>
typename MultiArrayManager<MultiArray>::value_type *MultiArrayManager<MultiArray>::data() {
	return _msg.data.data();
}

template<class MultiArray>
typename MultiArrayManager<MultiArray>::array_type &MultiArrayManager<MultiArray>::array () {
	return _msg.data;
}

template<class MultiArray>
int MultiArrayManager<MultiArray>::size(int i) {
	return _msg.layout.dim[i].size;
}

template<class MultiArray>
MultiArray MultiArrayManager<MultiArray>::msg() {
	return _msg;
}




}

#endif // NL_MULTIARRAY_ROS_H
