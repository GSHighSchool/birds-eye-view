cmake_minimum_required(VERSION 2.8.3)
project(random_walk)

find_package(PkgConfig REQUIRED)
pkg_check_modules(BULLET bullet)
include_directories(${BULLET_INCLUDE_DIRS})

find_package(catkin REQUIRED COMPONENTS 
  message_generation 
  roscpp 
  rospy 
  std_msgs 
  tf 
  geometry_msgs 
  nav_msgs 
  visualization_msgs 
  message_filters
  sensor_msgs
)


catkin_package(CATKIN_DEPENDS 
  message_runtime 
  roscpp 
  rospy
  std_msgs 
  tf 
  geometry_msgs 
  nav_msgs 
  visualization_msgs
  sensor_msgs
) 
include_directories(${catkin_INCLUDE_DIRS})

include_directories(${CMAKE_CURRENT_BINARY_DIR})
add_executable(random_walk  src/random_walk.cpp)
target_link_libraries(random_walk ${catkin_LIBRARIES})


#add_executable(particle_filter ParticleFilter/ParticleFilter.cpp)
#target_link_libraries(particle_filter ${catkin_LIBRARIES})
#target_link_libraries(particle_filter ${OpenCV_LIBS} ${BULLET_LIBRARIES})
#add_dependencies(particle_filter beginner_tutorials_generate_messages_cpp)

