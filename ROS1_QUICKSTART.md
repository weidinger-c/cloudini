# Cloudini ROS1 - Quick Start Guide

## Installation

### 1. Prerequisites

Ensure you have ROS1 (Noetic recommended) installed with development tools:

```bash
sudo apt-get update
sudo apt-get install -y \
  ros-noetic-pcl-ros \
  ros-noetic-pcl-conversions \
  ros-noetic-topic-tools \
  liblz4-dev \
  libzstd-dev
```

### 2. Build the Package

```bash
cd /ros_ws/danais_light_development_docker
catkin build cloudini_lib cloudini_ros

# Or using catkin_make:
catkin_make --only-pkg-with-deps cloudini_lib cloudini_ros

# Source the workspace
source devel/setup.bash
```

## Basic Usage

### Example 1: Subscribe to Compressed Point Clouds

Create a simple subscriber node:

```cpp
// subscriber_example.cpp
#include <cloudini_ros/cloudini_subscriber_pcl.hpp>
#include <ros/ros.h>
#include <pcl/PCLPointCloud2.h>

void cloudCallback(const pcl::PCLPointCloud2::Ptr& cloud) {
  ROS_INFO_STREAM("Received cloud: " 
                  << cloud->width << "x" << cloud->height 
                  << " = " << (cloud->width * cloud->height) << " points");
}

int main(int argc, char** argv) {
  ros::init(argc, argv, "cloudini_subscriber");
  ros::NodeHandle nh;
  
  // Create subscriber for compressed point cloud topic
  cloudini_ros::CloudiniSubscriberPCL subscriber(
      nh,                        // node handle
      "/lidar/points",           // topic name
      cloudCallback,             // callback function
      10                         // queue size
  );
  
  ROS_INFO("Waiting for compressed point clouds on %s", 
           subscriber.getTopicName().c_str());
  
  ros::spin();
  return 0;
}
```

### Example 2: Convert Point Cloud Topics

Use the built-in topic converter:

```bash
# Start the converter node
rosrun cloudini_ros cloudini_topic_converter \
  _topic_input:=/velodyne_points \
  _topic_output:=/velodyne_points/compressed \
  _compressing:=true \
  _resolution:=0.001
```

Parameters:
- `topic_input`: Input topic to read from
- `topic_output`: Output topic to publish to (auto-generated if not specified)
- `compressing`: `true` to compress, `false` to decompress
- `resolution`: Resolution for lossy compression (meters, default 0.001 = 1mm)

## Testing

### Test with Sample Data

1. **Record a rosbag** with point cloud data:
```bash
rosbag record /velodyne_points -O test.bag
```

2. **Run the topic converter**:
```bash
rosrun cloudini_ros cloudini_topic_converter \
  _topic_input:=/velodyne_points \
  _compressing:=true
```

3. **Play back the rosbag** and observe compression:
```bash
rosbag play test.bag
```

4. **View topics**:
```bash
rostopic list
# Should show:
# /velodyne_points
# /velodyne_points/compressed
```

### Measure Compression Ratio

The topic converter logs compression statistics every 20 messages.

## CMake Integration

To use cloudini_ros in your own package:

### package.xml
```xml
<depend>cloudini_ros</depend>
<depend>cloudini_lib</depend>
```

### CMakeLists.txt
```cmake
find_package(catkin REQUIRED COMPONENTS
  roscpp
  cloudini_ros
  cloudini_lib
  pcl_ros
)

catkin_package(
  CATKIN_DEPENDS roscpp cloudini_ros cloudini_lib pcl_ros
)

add_executable(my_node src/my_node.cpp)
target_link_libraries(my_node
  ${catkin_LIBRARIES}
)
```

## Troubleshooting

### Build Errors

**Problem**: `cloudini_lib not found`
```bash
# Solution: Build cloudini_lib first
catkin build cloudini_lib
catkin build cloudini_ros
```

**Problem**: Missing LZ4 or ZSTD
```bash
# Solution: Install compression libraries
sudo apt-get install liblz4-dev libzstd-dev
```

### Runtime Errors

**Problem**: "Failed to decode Cloudini point cloud"
- Check that the message format is compatible
- Verify the input topic is publishing the expected format

**Problem**: No subscribers/publishers
```bash
# Check topics
rostopic list
rostopic info /your/topic

# Check node
rosnode info /your_node_name
```

## Performance Tips

1. **Queue Size**: Adjust based on your sensor rate
   - High rate sensors (100Hz+): Use larger queues (50-100)
   - Low rate sensors (<10Hz): Small queues (5-10) are fine

2. **Resolution**: Trade-off between compression and accuracy
   - LiDAR: 0.001-0.01m (1-10mm)
   - Depth cameras: 0.005-0.02m (5-20mm)
   - Lower values = higher quality but less compression

3. **Memory**: The subscriber uses an object pool (max 4 clouds)
   - Efficient for most use cases
   - No manual memory management needed

## Example Launch File

Create `cloudini_example.launch`:

```xml
<launch>
  <!-- Topic converter node -->
  <node pkg="cloudini_ros" type="cloudini_topic_converter" name="cloudini_converter">
    <param name="topic_input" value="/velodyne_points"/>
    <param name="topic_output" value="/velodyne_points/compressed"/>
    <param name="compressing" value="true"/>
    <param name="resolution" value="0.001"/>
  </node>

  <!-- Your subscriber node -->
  <node pkg="your_package" type="your_subscriber" name="point_cloud_processor"/>
</launch>
```

Run it:
```bash
roslaunch your_package cloudini_example.launch
```

## Next Steps

- Read the full documentation: [ROS1_PORT_SUMMARY.md](./ROS1_PORT_SUMMARY.md)
- Check the original Cloudini documentation: https://github.com/facontidavide/cloudini
- Explore the test files in `cloudini_ros/test/` for more examples

## Support

For issues specific to the ROS1 port, check:
- Build errors in the workspace
- Original Cloudini library issues: https://github.com/facontidavide/cloudini/issues
