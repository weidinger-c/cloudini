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

#include "cloudini_ros/cloudini_subscriber_pcl.hpp"

#include <cloudini_lib/cloudini.hpp>
#include <cloudini_lib/pcl_conversion.hpp>
#include <cloudini_lib/ros_msg_utils.hpp>
#include <ros/serialization.h>

namespace cloudini_ros {

using namespace Cloudini;

CloudiniSubscriberPCL::CloudiniSubscriberPCL(
    ros::NodeHandle& nh, const std::string& topic_name, CallbackType callback, uint32_t queue_size)
    : user_callback_(callback), topic_name_(topic_name) {
  
  // Create a generic subscription using ShapeShifter
  subscription_ = nh.subscribe<topic_tools::ShapeShifter>(
      topic_name, queue_size,
      boost::bind(&CloudiniSubscriberPCL::messageCallback, this, _1));

  ROS_INFO("CloudiniSubscriberPCL created for topic: %s", topic_name.c_str());
}

CloudiniSubscriberPCL::~CloudiniSubscriberPCL() {
  // Clean up all objects in the pool
  std::lock_guard<std::mutex> lock(pool_mutex_);
  for (auto* ptr : cloud_pool_) {
    delete ptr;
  }
  cloud_pool_.clear();
}

std::string CloudiniSubscriberPCL::getTopicName() const {
  return topic_name_;
}

pcl::PCLPointCloud2::Ptr CloudiniSubscriberPCL::acquireCloudFromPool() {
  pcl::PCLPointCloud2* raw_ptr = nullptr;

  // Try to get from pool
  std::lock_guard<std::mutex> lock(pool_mutex_);
  if (!cloud_pool_.empty()) {
    raw_ptr = cloud_pool_.back();
    cloud_pool_.pop_back();
  } else {
    // Pool is empty, allocate new
    raw_ptr = new pcl::PCLPointCloud2();
  }

  // Create shared_ptr with custom deleter that returns to pool
  return pcl::PCLPointCloud2::Ptr(raw_ptr, [this](pcl::PCLPointCloud2* ptr) {
    std::lock_guard<std::mutex> lock(pool_mutex_);
    // Return to pool if not full, otherwise delete
    if (cloud_pool_.size() < MAX_POOL_SIZE) {
      // Clear the data but keep allocated memory for reuse
      ptr->data.clear();
      ptr->fields.clear();
      cloud_pool_.push_back(ptr);
    } else {
      delete ptr;
    }
  });
}

void CloudiniSubscriberPCL::messageCallback(const topic_tools::ShapeShifter::ConstPtr& msg) {
  try {
    // STEP 1: Get the serialized buffer from ShapeShifter (zero-copy access)
    const uint32_t serial_size = ros::serialization::serializationLength(*msg);
    std::vector<uint8_t> buffer(serial_size);
    
    ros::serialization::OStream stream(buffer.data(), serial_size);
    ros::serialization::serialize(stream, *msg);
    
    const ConstBufferView raw_msg(buffer.data(), buffer.size());

    // STEP 2: Parse the message to extract compressed data
    // This efficiently parses the compressed message without full deserialization
    const auto pc_info = cloudini_ros::getDeserializedPointCloudMessage(raw_msg);

    // STEP 3: Acquire a PCL cloud from the object pool (avoids allocation)
    auto pcl_cloud = acquireCloudFromPool();

    // STEP 4: Decode directly to PCL format
    // pc_info.data contains the compressed cloudini data
    PCLPointCloudDecode(pc_info.data, *pcl_cloud);

    // STEP 5: Invoke user callback with the decoded PCL cloud
    user_callback_(pcl_cloud);

  } catch (const std::exception& e) {
    ROS_ERROR("Failed to decode Cloudini point cloud: %s", e.what());
  }
}

}  // namespace cloudini_ros
