#ifndef NL_ROS_CONVERSIONS_H
#define NL_ROS_CONVERSIONS_H

#include <nlib/nl_multiarray_ros.h>
#include <torch/all.h>
#include <std_msgs/Float32MultiArray.h>

inline
void tensorToMsg (const torch::Tensor &tensor, const std::vector<float> &extraData, std_msgs::Float32MultiArray &outputMsg)
{
	nlib::MultiArray32Manager array(std::vector<int> (tensor.sizes().begin(), tensor.sizes().end()), extraData.size ());

	memcpy (array.data (), extraData.data (), extraData.size () * sizeof (float));
	memcpy (array.data () + extraData.size (), tensor.data_ptr(), tensor.element_size() * tensor.numel ());

	outputMsg = array.msg();
}

#endif // NL_ROS_CONVERSIONS_H
