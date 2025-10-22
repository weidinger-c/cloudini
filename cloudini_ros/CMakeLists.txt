cmake_minimum_required(VERSION 3.16)
project(cloudini_ros)

# Ensure we compile with at least C++20 because cloudini_lib headers use C++17/20 features
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# find dependencies
find_package(catkin REQUIRED COMPONENTS
  roscpp
  sensor_msgs
  rosbag
  rosbag_storage
  pcl_conversions
  pcl_ros
  topic_tools
  cloudini_lib
)

find_package(PCL REQUIRED COMPONENTS common)
find_package(Draco QUIET)
find_package(rclcpp QUIET)

catkin_package(
  INCLUDE_DIRS include
  LIBRARIES cloudini_ros
  CATKIN_DEPENDS 
    roscpp 
    sensor_msgs 
    rosbag 
    rosbag_storage 
    pcl_conversions 
    pcl_ros
    topic_tools
    cloudini_lib
  DEPENDS PCL
)

######### library #########

add_library(cloudini_ros SHARED
  src/conversion_utils.cpp
  src/cloudini_subscriber_pcl.cpp
)

# Redundant but explicit: propagate standard to this target and executables even if linker transitivity fails via catkin
target_compile_features(cloudini_ros PUBLIC cxx_std_20)

target_include_directories(cloudini_ros
  PUBLIC
    ${catkin_INCLUDE_DIRS}
    ${PCL_INCLUDE_DIRS}
    include
)

target_link_libraries(cloudini_ros
  ${catkin_LIBRARIES}
  ${PCL_LIBRARIES}
)

install(TARGETS cloudini_ros
  ARCHIVE DESTINATION ${CATKIN_PACKAGE_LIB_DESTINATION}
  LIBRARY DESTINATION ${CATKIN_PACKAGE_LIB_DESTINATION}
  RUNTIME DESTINATION ${CATKIN_PACKAGE_BIN_DESTINATION}
)

install(
  DIRECTORY include/
  DESTINATION ${CATKIN_PACKAGE_INCLUDE_DESTINATION}
)

######### topic converter #########

add_executable(cloudini_topic_converter
  src/topic_converter_ros1.cpp
)
target_compile_features(cloudini_topic_converter PRIVATE cxx_std_20)

target_include_directories(cloudini_topic_converter
  PUBLIC
    ${catkin_INCLUDE_DIRS}
    include
)

target_link_libraries(cloudini_topic_converter
  cloudini_ros
  ${catkin_LIBRARIES}
)

install(TARGETS cloudini_topic_converter
  RUNTIME DESTINATION ${CATKIN_PACKAGE_BIN_DESTINATION}
)

######### rosbag benchmark & test (ROS2 only) #########
if(rclcpp_FOUND)
  message(STATUS "rclcpp found: enabling rosbag_benchmark and test_cloudini_subscriber (ROS2 build)")
  add_executable(rosbag_benchmark
    test/rosbag_benchmark.cpp
    test/draco_helper.cpp
  )
  target_compile_features(rosbag_benchmark PRIVATE cxx_std_20)
  target_include_directories(rosbag_benchmark
    PUBLIC
      ${catkin_INCLUDE_DIRS}
      include
  )
  target_link_libraries(rosbag_benchmark
    cloudini_ros
    ${catkin_LIBRARIES}
  )
  if(Draco_FOUND)
    target_compile_definitions(rosbag_benchmark PRIVATE DRACO_FOUND)
    target_link_libraries(rosbag_benchmark draco::draco)
  endif()
  install(TARGETS rosbag_benchmark
    RUNTIME DESTINATION ${CATKIN_PACKAGE_BIN_DESTINATION}
  )

  add_executable(test_cloudini_subscriber test/test_cloudini_subscriber.cpp)
  target_compile_features(test_cloudini_subscriber PRIVATE cxx_std_20)
  target_link_libraries(test_cloudini_subscriber
    cloudini_ros
    ${catkin_LIBRARIES}
    ${PCL_LIBRARIES}
  )
  target_include_directories(test_cloudini_subscriber
    PRIVATE
      ${catkin_INCLUDE_DIRS}
      ${PCL_INCLUDE_DIRS}
      include
  )
  install(TARGETS test_cloudini_subscriber
    RUNTIME DESTINATION ${CATKIN_PACKAGE_BIN_DESTINATION}
  )
else()
  message(STATUS "rclcpp NOT found: skipping rosbag_benchmark and test_cloudini_subscriber (ROS1 build)")
endif()

######### launch files #########
install(DIRECTORY launch/
  DESTINATION ${CATKIN_PACKAGE_SHARE_DESTINATION}/launch
  FILES_MATCHING PATTERN "*.launch"
)
