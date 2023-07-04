#ifndef NL_ROS_CONVERSIONS_H
#define NL_ROS_CONVERSIONS_H

#include <nlib/nl_multiarray_ros.h>
#include <std_msgs/Float32MultiArray.h>

#ifdef INCLUDE_TORCH
#include <torch/all.h>
inline
void tensorToMsg (const torch::Tensor &tensor, const std::vector<float> &extraData, std_msgs::Float32MultiArray &outputMsg)
{
	nlib::MultiArray32Manager array(std::vector<int> (tensor.sizes().begin(), tensor.sizes().end()), extraData.size ());

	memcpy (array.data (), extraData.data (), extraData.size () * sizeof (float));
	memcpy (array.data () + extraData.size (), tensor.data_ptr(), tensor.element_size() * tensor.numel ());

	outputMsg = array.msg();
}
#endif

#ifdef INCLUDE_EIGEN
#include <eigen3/Eigen/Core>
namespace nlib {

inline void eigen32ToMsg (const Eigen::MatrixXf &matrix, const std::vector<float> &extraData, std_msgs::Float32MultiArray &msg)
{
	MultiArray32Manager arrayManager(std::vector<int> {matrix.rows(), matrix.cols()}, extraData.size ());

	auto last = std::copy (extraData.begin (), extraData.end (), arrayManager.array ().begin());
	std::copy (matrix.data(), matrix.data() + matrix.size(), last);

	msg = arrayManager.msg ();
}
}
#endif




#endif // NL_ROS_CONVERSIONS_H
