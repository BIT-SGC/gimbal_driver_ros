// gimbal_ctrl_node.cpp
#include "gimbal_rc_wrapper/GimbalCtrl.h" // 自定义消息
#include <gimbal_drv/gimbal_ctrl.h>
#include <cstdio>  // for fopen, fprintf
#include <memory>
#include <mutex>
#include <ros/ros.h>

class GimbalCtrlNode {
public:
  GimbalCtrlNode() : nh_("~"), sub_(), gimbal_(nullptr) {
    // 从参数服务器获取配置
    std::string target_ip;
    int port;

    nh_.param<std::string>("target_ip", target_ip, "192.168.144.108");
    nh_.param<int>("port", port, 5000);

    ROS_INFO("Attempting to connect to gimbal at %s:%d", target_ip.c_str(),
             port);

    // 尝试构造 GimbalCtrl 对象
    try {
      gimbal_ =
          std::make_unique<GimbalCtrl>(target_ip, static_cast<uint16_t>(port));

      // 设置错误回调（lambda 捕获 this）
      gimbal_->setErrorCallback([this](const std::string &error_msg) {
        std::lock_guard<std::mutex> lock(ros_mutex_);
        ROS_ERROR("Gimbal communication error: %s", error_msg.c_str());
      });

      ROS_INFO("Successfully initialized GimbalCtrl.");

      // 启动时暂不开启录像
      // gimbal_->controlRecording(GimbalCtrl::RecordState::START);
      // if (gimbal_->queryRecordingStatus() == true) {
      //   ROS_INFO("Recording started.");
      // }

    } catch (const std::exception &e) {
      ROS_FATAL("Failed to initialize GimbalCtrl: %s", e.what());
      ROS_FATAL("Shutting down node...");
      ros::shutdown();
      return;
    } catch (...) {
      ROS_FATAL("Unknown error during GimbalCtrl initialization.");
      ros::shutdown();
      return;
    }

    // 创建订阅者
    sub_ =
        nh_.subscribe("/gimbal/control", 10, &GimbalCtrlNode::callback, this);

    ROS_INFO("Gimbal control node is ready. Subscribed to /gimbal/control.");
  }

  ~GimbalCtrlNode() {
    std::lock_guard<std::mutex> lock(ros_mutex_);
    if (gimbal_) {
      // 确认状态
      if (!gimbal_->queryRecordingStatus()) {
        ROS_INFO("Not Recording, exit.");
      } else {
        do {
          gimbal_->controlRecording(GimbalCtrl::RecordState::STOP);
          ROS_WARN("Recording stopping...");
        } while (gimbal_->queryRecordingStatus());
        ROS_INFO("Recording stopped successfully.");
      }
    }
  }

  void callback(const gimbal_rc_wrapper::GimbalCtrl::ConstPtr &msg) {
    std::lock_guard<std::mutex> lock(ros_mutex_);

    if (!gimbal_) {
      ROS_WARN("GimbalCtrl not available, ignoring command.");
      return;
    }

    // 处理视频流源切换（可扩展为调用 shell 命令或发布控制消息）
    switch (msg->stream_src) {
    case 0:
      ROS_DEBUG("Stream source: Visible light (RGB)");
      if (last_stream_src_ != 0) {
        sendStreamSwitchCommand("switch 1");
        last_stream_src_ = 0;
      }
      break;
    case 1:
      ROS_DEBUG("Stream source: Thermal (IR)");
      if (last_stream_src_ != 1) {
        sendStreamSwitchCommand("switch 2");
        last_stream_src_ = 1;
      }
      break;
    case 2:
      ROS_DEBUG("Stream source: Fusion mode (not supported yet)");
      // TODO: 可扩展融合流
      break;
    default:
      ROS_WARN("Unknown stream source ID: %u", msg->stream_src);
      break;
    }

    // 设置录像状态 与解锁按键相关
    switch (msg->record_sta) {
    case 0:
      if (_record_sta != false) {
        do {
          gimbal_->controlRecording(GimbalCtrl::RecordState::STOP);
          ROS_WARN("Recording stopping...");
        } while (gimbal_->queryRecordingStatus());
        _record_sta = false;
        ROS_INFO("Recording stopped successfully.");
      }
      break;
    case 1:
      break;
    case 2:
      if (_record_sta != true) {
        do {
          gimbal_->controlRecording(GimbalCtrl::RecordState::START);
          ROS_WARN("Recording starting...");
        } while (!gimbal_->queryRecordingStatus());
        _record_sta = true;
        ROS_INFO("Recording started successfully.");
      }
      break;
    default:
      break;
    }

    // 以下角度控制需要使能云台
    if (!msg->enable) {
      ROS_DEBUG("Gimbal control disabled. Ignoring command.");
      return;
    }

    float yaw = msg->yaw_angle;
    float pitch = msg->pitch_angle;
    float roll = msg->roll_angle;

    // 角度范围检查（可选）
    bool out_of_range = false;
    if (yaw < -180.0f || yaw > 180.0f) {
      ROS_WARN("Yaw angle out of range: %.2f", yaw);
      out_of_range = true;
    }
    if (pitch < -90.0f || pitch > 90.0f) {
      ROS_WARN("Pitch angle out of range: %.2f", pitch);
      out_of_range = true;
    }
    if (roll < -180.0f || roll > 180.0f) {
      ROS_WARN("Roll angle out of range: %.2f", roll);
      out_of_range = true;
    }
    if (out_of_range)
      return;

    // 控制云台角度（速度设为 10 deg/s）
    bool success = gimbal_->setGimbalAngle(yaw, pitch, roll, 90.0f);

    if (success) {
      ROS_INFO("Set gimbal angle -> yaw: %.2f°, pitch: %.2f°, roll: %.2f°", yaw,
               pitch, roll);
    } else {
      ROS_ERROR("Failed to set gimbal angle: yaw=%.2f, pitch=%.2f, roll=%.2f",
                yaw, pitch, roll);
    }
  }

private:
  ros::NodeHandle nh_;
  ros::Subscriber sub_;
  std::unique_ptr<GimbalCtrl> gimbal_;
  std::mutex ros_mutex_;
  bool _record_sta = false;
  std::string current_stream_pipe_ = "/tmp/stream-control-pipe";
  int last_stream_src_ = -1;

  bool sendStreamSwitchCommand(const std::string &cmd) {
    FILE *pipe = fopen(current_stream_pipe_.c_str(), "w");
    if (!pipe) {
      ROS_WARN("Failed to open stream control pipe: %s",
               current_stream_pipe_.c_str());
      return false;
    }
    if (fprintf(pipe, "%s\n", cmd.c_str()) < 0) {
      ROS_WARN("Failed to write command to pipe: %s", cmd.c_str());
      fclose(pipe);
      return false;
    }
    fclose(pipe);
    ROS_INFO("Sent stream switch command: %s", cmd.c_str());
    return true;
  }
};

int main(int argc, char **argv) {
  ros::init(argc, argv, "gimbal_ctrl_node");

  try {
    GimbalCtrlNode node;
    ros::spin();
  } catch (const std::exception &e) {
    ROS_FATAL("Unhandled exception: %s", e.what());
    return 1;
  } catch (...) {
    ROS_FATAL("Unknown fatal error in node.");
    return 1;
  }

  return 0;
}