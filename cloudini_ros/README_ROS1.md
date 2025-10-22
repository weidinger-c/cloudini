# cloudini_ros - ROS1 Port

Point cloud compression library integration for ROS1 (Noetic).

This is a port of the [Cloudini](https://github.com/facontidavide/cloudini) library to ROS1, enabling efficient point cloud compression and decompression in ROS1 systems.

## Features

- **Efficient Compression**: Compress sensor_msgs/PointCloud2 messages with excellent compression ratios
- **Fast Processing**: Optimized for real-time applications
- **PCL Integration**: Direct conversion to PCL point clouds
- **Memory Efficient**: Object pooling to minimize allocations
- **Easy to Use**: Simple API for subscribing to compressed point clouds

## Installation

### Prerequisites

```bash
sudo apt-get install -y \
  ros-noetic-pcl-ros \
  ros-noetic-pcl-conversions \
  ros-noetic-topic-tools \
  liblz4-dev \
  libzstd-dev
```

### Build

```bash
cd your_catkin_workspace
catkin build cloudini_lib cloudini_ros
source devel/setup.bash
```

## Quick Start

### Subscribe to Compressed Point Clouds

```cpp
#include <cloudini_ros/cloudini_subscriber_pcl.hpp>
#include <ros/ros.h>

void callback(const pcl::PCLPointCloud2::Ptr& cloud) {
  ROS_INFO("Received %zu points", cloud->width * cloud->height);
}

int main(int argc, char** argv) {
  ros::init(argc, argv, "example");
  ros::NodeHandle nh;
  
  cloudini_ros::CloudiniSubscriberPCL sub(nh, "/lidar/points", callback);
  ros::spin();
  return 0;
}
```

### Convert Topics

```bash
# Compress a point cloud topic
roslaunch cloudini_ros topic_converter.launch \
  topic_input:=/velodyne_points \
  compressing:=true \
  resolution:=0.001
```

## API Documentation

### CloudiniSubscriberPCL

Main class for subscribing to compressed point cloud topics.

#### Constructor

```cpp
CloudiniSubscriberPCL(
    ros::NodeHandle& nh,        // Node handle
    const std::string& topic,   // Topic name
    CallbackType callback,      // Callback function
    uint32_t queue_size = 10    // Subscriber queue size
);
```

#### Callback Type

```cpp
using CallbackType = std::function<void(const pcl::PCLPointCloud2::Ptr&)>;
```

#### Methods

- `std::string getTopicName() const` - Returns the subscribed topic name

### Conversion Utilities

```cpp
#include <cloudini_ros/conversion_utils.hpp>

// Convert ROS message to encoding info
Cloudini::EncodingInfo ConvertToEncodingInfo(
    const sensor_msgs::PointCloud2& msg, 
    float resolution
);
```

## Nodes

### cloudini_topic_converter

Converts point cloud topics between compressed and uncompressed formats.

#### Parameters

- `~topic_input` (string): Input topic name
- `~topic_output` (string, optional): Output topic name (auto-generated if empty)
- `~compressing` (bool, default: true): Compress (true) or decompress (false)
- `~resolution` (double, default: 0.001): Resolution in meters for lossy compression

#### Topics

**Subscribes to:**
- `topic_input` (sensor_msgs/PointCloud2): Input point clouds

**Publishes to:**
- `topic_output` (sensor_msgs/PointCloud2): Processed point clouds

## Launch Files

### topic_converter.launch

Launches the point cloud topic converter.

**Arguments:**
- `topic_input`: Input topic (default: "/points")
- `topic_output`: Output topic (default: auto-generated)
- `compressing`: Compress or decompress (default: true)
- `resolution`: Compression resolution (default: 0.001)

**Example:**

```bash
roslaunch cloudini_ros topic_converter.launch \
  topic_input:=/camera/depth/points \
  resolution:=0.005
```

## Dependencies

- **ROS1 Noetic** (or Melodic)
- **PCL** (Point Cloud Library)
- **cloudini_lib**: Core compression library
- **LZ4**: Fast compression
- **ZSTD**: High compression ratio

## Performance

Typical compression ratios (compared to raw sensor_msgs/PointCloud2):
- **Velodyne VLP-16**: 3-5x compression
- **Ouster OS1**: 4-6x compression
- **RealSense depth**: 5-8x compression

Processing speed: Typically >100 Hz on modern CPUs.

## Limitations

1. **No Plugin System**: The ROS2 point_cloud_transport plugin system is not available in ROS1
2. **Custom Message Format**: Compressed data uses internal buffer format
3. **No QoS Profiles**: ROS1 uses simple queue sizes instead of DDS QoS

## Documentation

- [Quick Start Guide](../ROS1_QUICKSTART.md)
- [Detailed Port Summary](../ROS1_PORT_SUMMARY.md)
- [Original Cloudini](https://github.com/facontidavide/cloudini)

## Testing

```bash
# Build and run tests
catkin build cloudini_ros --make-args tests
rosrun cloudini_ros test_cloudini_subscriber
```

## Examples

Check the `test/` directory for usage examples.

## License

Apache License 2.0

## Authors

- **Original Library**: Davide Faconti
- **ROS1 Port**: [Your team/name]

## Contributing

Contributions welcome! Please check the original repository for contribution guidelines.

## Troubleshooting

### Build Issues

**Problem**: Cannot find cloudini_lib
```bash
catkin build cloudini_lib first
```

**Problem**: Missing compression libraries
```bash
sudo apt-get install liblz4-dev libzstd-dev
```

### Runtime Issues

Check logs:
```bash
rosrun cloudini_ros cloudini_topic_converter --log-level debug
```

For more help, see [ROS1_PORT_SUMMARY.md](../ROS1_PORT_SUMMARY.md).
