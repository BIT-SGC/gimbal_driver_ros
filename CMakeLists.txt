cmake_minimum_required(VERSION 3.0.2)
project(gimbal_driver_ros)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(catkin REQUIRED COMPONENTS
  roscpp
  std_msgs
  message_runtime
  gimbal_rc_wrapper
)

# find_library(GIMBAL_CONTROL_LIB
#   NAMES gimbal_control
#   PATHS /usr/lib/gimbal_drv
#   NO_DEFAULT_PATH
# )

# if(NOT GIMBAL_CONTROL_LIB)
#   message(FATAL_ERROR "FATAL: libgimbal_control.so not found in /usr/lib/gimbal_drv")
# endif()

# 假设消息来自另一个包（如 my_gimbal_control 或 gimbal_msgs）
# 确保该包已安装且消息生成

catkin_package(
  CATKIN_DEPENDS roscpp std_msgs message_runtime
)

include_directories(
  ${catkin_INCLUDE_DIRS}
  # 系统头文件路径（通常不需要显式添加 /usr/include）
)

# 主节点
add_executable(gimbal_driver_node src/gimbal_driver_node.cc)


# 链接：ROS + 系统云台库
target_link_libraries(gimbal_driver_node
  ${catkin_LIBRARIES}
  # ${GIMBAL_CONTROL_LIB}
  gimbal_control
)

# 安装
install(TARGETS gimbal_driver_node
  RUNTIME DESTINATION ${CATKIN_PACKAGE_BIN_DESTINATION}
)

# # 可选：安装 launch 文件
# install(DIRECTORY launch/
#   DESTINATION ${CATKIN_PACKAGE_SHARE_DESTINATION}/launch
#   PATTERN ".svn" EXCLUDE
#   USE_SOURCE_PERMISSIONS
# )