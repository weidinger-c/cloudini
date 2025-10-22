/*
 * Copyright 2025 Davide Faconti
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <cloudini_lib/cloudini.hpp>
#include <cloudini_lib/pcl_conversion.hpp>
#include <cloudini_lib/ros_msg_utils.hpp>
#include <cloudini_ros/conversion_utils.hpp>
#include <pcl/PCLPointCloud2.h>
#include <pcl_conversions/pcl_conversions.h>
#include <ros/ros.h>
#include <ros/serialization.h>
#include <sensor_msgs/PointCloud2.h>

/**
 * @brief Simple ROS1 node that converts PointCloud2 messages
 *        to/from compressed format using Cloudini compression
 */
class CloudiniPointCloudConverter {
 public:
  CloudiniPointCloudConverter(ros::NodeHandle& nh, ros::NodeHandle& pnh)
      : nh_(nh), pnh_(pnh) {
    // Read parameters
    pnh_.param<bool>("compressing", compressing_, true);
    pnh_.param<std::string>("topic_input", topic_input_, "/points");
    pnh_.param<std::string>("topic_output", topic_output_, "");
    pnh_.param<double>("resolution", resolution_, 0.001);

    if (topic_input_.empty()) {
      ROS_ERROR("Input topic is not set");
      throw std::runtime_error("Input topic is not set");
    }

    if (topic_output_.empty()) {
      topic_output_ = topic_input_ + (compressing_ ? "/compressed" : "/decompressed");
      ROS_WARN("Output topic is not set, using default: %s", topic_output_.c_str());
    }

    // Create publisher
    pub_ = nh_.advertise<sensor_msgs::PointCloud2>(topic_output_, 10);

    // Create subscriber
    sub_ = nh_.subscribe(topic_input_, 10, &CloudiniPointCloudConverter::pointCloudCallback, this);

    ROS_INFO("CloudiniPointCloudConverter initialized");
    ROS_INFO("  Compressing: %s", compressing_ ? "true" : "false");
    ROS_INFO("  Input topic: %s", topic_input_.c_str());
    ROS_INFO("  Output topic: %s", topic_output_.c_str());
    ROS_INFO("  Resolution: %.6f", resolution_);
  }

 private:
  void pointCloudCallback(const sensor_msgs::PointCloud2::ConstPtr& msg) {
    if (pub_.getNumSubscribers() == 0) {
      return;
    }

    // STEP 1: Convert ROS1 PointCloud2 to Cloudini's internal format
    Cloudini::EncodingInfo encoding_info;
    encoding_info.width = msg->width;
    encoding_info.height = msg->height;
    encoding_info.point_step = msg->point_step;
    encoding_info.encoding_opt = Cloudini::EncodingOptions::LOSSY;
    encoding_info.compression_opt = Cloudini::CompressionOption::ZSTD;
    
    // Convert field information
    for (const auto& ros_field : msg->fields) {
      Cloudini::PointField field;
      field.name = ros_field.name;
      field.offset = ros_field.offset;
      field.type = static_cast<Cloudini::FieldType>(ros_field.datatype);
      // Apply resolution for FLOAT32 fields (XYZ coordinates)
      if (field.type == Cloudini::FieldType::FLOAT32) {
        field.resolution = resolution_;
      }
      encoding_info.fields.push_back(field);
    }
    
    // Create buffer view of the point cloud data
    Cloudini::ConstBufferView input_data(msg->data.data(), msg->data.size());
    
    if (compressing_) {
      // STEP 2: Compress the point cloud data using Cloudini encoder
      Cloudini::PointcloudEncoder encoder(encoding_info);
      output_raw_message_.clear();
      encoder.encode(input_data, output_raw_message_);
      
      // STEP 3: Create output message with compressed data
      output_message_.header = msg->header;
      output_message_.height = 1;  // Compressed data is unorganized
      output_message_.width = output_raw_message_.size();
      output_message_.point_step = 1;
      output_message_.row_step = output_raw_message_.size();
      output_message_.is_bigendian = false;
      output_message_.is_dense = true;
      
      // Create a single field to hold compressed data
      output_message_.fields.clear();
      sensor_msgs::PointField compressed_field;
      compressed_field.name = "compressed_data";
      compressed_field.offset = 0;
      compressed_field.datatype = sensor_msgs::PointField::UINT8;
      compressed_field.count = 1;
      output_message_.fields.push_back(compressed_field);
      
      // Copy compressed data
      output_message_.data = output_raw_message_;
      
    } else {
      // STEP 2: Decompress the point cloud data
      // First, we need to decode the header from the compressed data to get encoding info
      Cloudini::ConstBufferView compressed_view(msg->data.data(), msg->data.size());
      Cloudini::EncodingInfo decoded_info = Cloudini::DecodeHeader(compressed_view);
      
      // Now decompress using the decoder
      Cloudini::PointcloudDecoder decoder;
      output_raw_message_.clear();
      decoder.decode(decoded_info, compressed_view, output_raw_message_);
      
      // STEP 3: Reconstruct the original PointCloud2 message
      output_message_.header = msg->header;
      output_message_.height = decoded_info.height;
      output_message_.width = decoded_info.width;
      output_message_.point_step = decoded_info.point_step;
      output_message_.row_step = decoded_info.point_step * decoded_info.width;
      output_message_.is_bigendian = false;
      output_message_.is_dense = true;
      
      // Reconstruct field information
      output_message_.fields.clear();
      for (const auto& cloudini_field : decoded_info.fields) {
        sensor_msgs::PointField ros_field;
        ros_field.name = cloudini_field.name;
        ros_field.offset = cloudini_field.offset;
        ros_field.datatype = static_cast<uint8_t>(cloudini_field.type);
        ros_field.count = 1;
        output_message_.fields.push_back(ros_field);
      }
      
      // Copy decompressed data
      output_message_.data = output_raw_message_;
    }
    
    pub_.publish(output_message_);

    // Update statistics
    tot_original_size += msg->data.size();
    tot_compressed_size += output_raw_message_.size();

    static int count = 0;
    if (count % 20 == 0) {
      double average_ratio = static_cast<double>(tot_compressed_size) / tot_original_size;
      tot_compressed_size = 0;
      tot_original_size = 0;
      ROS_INFO("Converted %d messages, average compression ratio: %.2f", count, average_ratio);
    }
    count++;
  }

  ros::NodeHandle nh_;
  ros::NodeHandle pnh_;
  ros::Subscriber sub_;
  ros::Publisher pub_;

  bool compressing_;
  std::string topic_input_;
  std::string topic_output_;
  double resolution_;
  
  // Variables for message handling and statistics
  std::vector<uint8_t> output_raw_message_;
  sensor_msgs::PointCloud2 output_message_;
  size_t tot_original_size = 0;
  size_t tot_compressed_size = 0;
};

int main(int argc, char** argv) {
  ros::init(argc, argv, "cloudini_topic_converter");
  ros::NodeHandle nh;
  ros::NodeHandle pnh("~");

  try {
    CloudiniPointCloudConverter converter(nh, pnh);
    ros::spin();
  } catch (const std::exception& e) {
    ROS_ERROR("Exception in cloudini_topic_converter: %s", e.what());
    return 1;
  }

  return 0;
}
