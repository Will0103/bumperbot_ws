// bumperbot_interface_single.cpp
//
// Single-file study version of a ROS 2 ros2_control hardware interface.
// Later, this can be split into:
//   include/bumperbot_firmware/bumperbot_interface.hpp
//   src/bumperbot_interface.cpp
//
// Data flow:
//
//   DiffDriveController
//          |
//          v
//   velocity_commands_
//          |
//       write()
//          |
//       Serial
//          |
//       Arduino
//          |
//     L298N + Motor
//
//   Encoder
//      |
//   Arduino
//      |
//    Serial
//      |
//    read()
//      |
//   velocity_states_ / position_states_
//      |
//   ros2_control

#include <rclcpp/rclcpp.hpp>

#include <hardware_interface/system_interface.hpp>
#include <hardware_interface/types/hardware_interface_type_values.hpp>

#include <rclcpp_lifecycle/state.hpp>
#include <rclcpp_lifecycle/node_interfaces/lifecycle_node_interface.hpp>

#include <pluginlib/class_list_macros.hpp>
#include <libserial/SerialPort.h>

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace template_firmware
{

using CallbackReturn = rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn;

class TemplateInterface : public hardware_interface::SystemInterface
{
public:
  TemplateInterface() = default;

  ~TemplateInterface() override
  {
    if (arduino_.IsOpen())
    {
      try
      {
        arduino_.Close();
      }
      catch (...)
      {
        RCLCPP_ERROR(rclcpp::get_logger("TemplateInterface"), "Failed to close Arduino serial port in destructor.");
      }
    }
  }

  // 1) ros2_control passes HardwareInfo parsed from <ros2_control>.
  CallbackReturn on_init(const hardware_interface::HardwareInfo & hardware_info) override
  {
    if (hardware_interface::SystemInterface::on_init(hardware_info) != CallbackReturn::SUCCESS)
    {
      return CallbackReturn::FAILURE;
    }

    try
    {
      port_ = info_.hardware_parameters.at("port");
    }
    catch (const std::out_of_range &)
    {
      RCLCPP_FATAL(rclcpp::get_logger("TemplateInterface"), "No serial port parameter was provided.");
      return CallbackReturn::FAILURE;
    }

    if (info_.joints.size() != 2)
    {
      RCLCPP_FATAL(rclcpp::get_logger("TemplateInterface"), "Expected exactly 2 wheel joints, but got %zu.", info_.joints.size());
      return CallbackReturn::FAILURE;
    }

    // resize() creates actual elements; reserve() would only reserve capacity.
    velocity_commands_.resize(info_.joints.size(), 0.0);
    position_states_.resize(info_.joints.size(), 0.0);
    velocity_states_.resize(info_.joints.size(), 0.0);

    last_run_ = rclcpp::Clock().now();

    return CallbackReturn::SUCCESS;
  }

  // 2) Tell ros2_control what it may command.
  std::vector<hardware_interface::CommandInterface> export_command_interfaces() override
  {
    std::vector<hardware_interface::CommandInterface> command_interfaces;

    for (size_t i = 0; i < info_.joints.size(); ++i)
    {
      command_interfaces.emplace_back(info_.joints[i].name, hardware_interface::HW_IF_VELOCITY, &velocity_commands_[i]);
    }

    return command_interfaces;
  }

  // 3) Tell ros2_control what states the hardware reports.
  std::vector<hardware_interface::StateInterface> export_state_interfaces() override
  {
    std::vector<hardware_interface::StateInterface> state_interfaces;

    for (size_t i = 0; i < info_.joints.size(); ++i)
    {
      state_interfaces.emplace_back(info_.joints[i].name, hardware_interface::HW_IF_POSITION, &position_states_[i]);

      state_interfaces.emplace_back(info_.joints[i].name, hardware_interface::HW_IF_VELOCITY, &velocity_states_[i]);
    }

    return state_interfaces;
  }

  // 4) Start the hardware.
  CallbackReturn on_activate(const rclcpp_lifecycle::State &) override
  {
    std::fill(velocity_commands_.begin(), velocity_commands_.end(), 0.0);
    std::fill(position_states_.begin(), position_states_.end(), 0.0);
    std::fill(velocity_states_.begin(), velocity_states_.end(), 0.0);

    try
    {
      arduino_.Open(port_);
      arduino_.SetBaudRate(LibSerial::BaudRate::BAUD_115200);
    }
    catch (...)
    {
      RCLCPP_FATAL_STREAM(rclcpp::get_logger("TemplateInterface"), "Failed to open serial port " << port_);
      return CallbackReturn::FAILURE;
    }

    last_run_ = rclcpp::Clock().now();

    RCLCPP_INFO(rclcpp::get_logger("TemplateInterface"), "Hardware activated.");

    return CallbackReturn::SUCCESS;
  }

  // 5) Stop the hardware.
  CallbackReturn on_deactivate(const rclcpp_lifecycle::State &) override
  {
    if (arduino_.IsOpen())
    {
      try
      {
        arduino_.Close();
      }
      catch (...)
      {
        RCLCPP_ERROR_STREAM(rclcpp::get_logger("TemplateInterface"), "Failed to close serial port " << port_);
      }
    }

    RCLCPP_INFO(rclcpp::get_logger("TemplateInterface"), "Hardware deactivated.");

    return CallbackReturn::SUCCESS;
  }

  // 6) Hardware -> ROS 2
  //
  // Expected Arduino line:
  //   rp03.20,ln02.50,
  //
  // r/l = right/left
  // p/n = positive/negative
  //
  // IMPORTANT:
  //   index 0 = right wheel
  //   index 1 = left wheel
  hardware_interface::return_type read(const rclcpp::Time &, const rclcpp::Duration &) override
  {
    if (!arduino_.IsDataAvailable())
    {
      return hardware_interface::return_type::OK;
    }

    const auto now = rclcpp::Clock().now();
    const double dt = (now - last_run_).seconds();

    std::string message;
    arduino_.ReadLine(message);

    std::stringstream ss(message);
    std::string field;

    while (std::getline(ss, field, ','))
    {
      if (field.size() < 3)
      {
        continue;
      }

      const char wheel = field.at(0);
      const char sign_char = field.at(1);

      if ((wheel != 'r' && wheel != 'l') ||
          (sign_char != 'p' && sign_char != 'n'))
      {
        continue;
      }

      const int sign = (sign_char == 'p') ? 1 : -1;

      try
      {
        const double velocity = sign * std::stod(field.substr(2));

        if (wheel == 'r')
        {
          velocity_states_.at(RIGHT_WHEEL_INDEX) = velocity;
          position_states_.at(RIGHT_WHEEL_INDEX) += velocity * dt;
        }
        else
        {
          velocity_states_.at(LEFT_WHEEL_INDEX) = velocity;
          position_states_.at(LEFT_WHEEL_INDEX) += velocity * dt;
        }
      }
      catch (const std::exception &)
      {
        continue;
      }
    }

    last_run_ = now;

    return hardware_interface::return_type::OK;
  }

  // 7) ROS 2 -> Hardware
  //
  // Example:
  //   right = +3.20 rad/s
  //   left  = -2.50 rad/s
  //
  // Sent to Arduino as:
  //   rp03.20,ln02.50,\n
  hardware_interface::return_type write(const rclcpp::Time &, const rclcpp::Duration &) override
  {
    const double right = velocity_commands_.at(RIGHT_WHEEL_INDEX);
    const double left  = velocity_commands_.at(LEFT_WHEEL_INDEX);

    const char right_sign = (right >= 0.0) ? 'p' : 'n';
    const char left_sign  = (left  >= 0.0) ? 'p' : 'n';

    std::stringstream message;

    message << "r" << right_sign << std::fixed << std::setprecision(2) << std::setw(5) << std::setfill('0') << std::abs(right)
            << ",l" << left_sign << std::setw(5) << std::setfill('0') << std::abs(left) << ",\n";

    try
    {
      arduino_.Write(message.str());
    }
    catch (...)
    {
      RCLCPP_ERROR_STREAM(rclcpp::get_logger("TemplateInterface"), "Failed to send: " << message.str() << " through port " << port_);

      return hardware_interface::return_type::ERROR;
    }

    return hardware_interface::return_type::OK;
  }

private:
  // This implementation assumes the <ros2_control> joint order is:
  //   [0] right wheel
  //   [1] left wheel
  static constexpr size_t RIGHT_WHEEL_INDEX = 0;
  static constexpr size_t LEFT_WHEEL_INDEX = 1;

  LibSerial::SerialPort arduino_;
  std::string port_;

  // ROS 2 -> Hardware
  std::vector<double> velocity_commands_;

  // Hardware -> ROS 2
  std::vector<double> position_states_;
  std::vector<double> velocity_states_;

  rclcpp::Time last_run_;
};

}  // namespace template_firmware

// 8) Register this class as a loadable ros2_control plugin.
PLUGINLIB_EXPORT_CLASS(template_firmware::TemplateInterface, hardware_interface::SystemInterface)

// ============================================================
// QUICK NOTES
//
// on_init()
//   -> receive HardwareInfo / initialize hardware data
//
// export_command_interfaces()
//   -> tell ros2_control what it may command
//
// export_state_interfaces()
//   -> tell ros2_control what states the hardware reports
//
// write()
//   -> ROS 2 -> Arduino/hardware
//
// read()
//   -> Arduino/hardware -> ROS 2
//
// on_activate()
//   -> start/open hardware
//
// on_deactivate()
//   -> stop/close hardware
//
// PLUGINLIB_EXPORT_CLASS()
//   -> register class as ros2_control plugin
//
// Core:
//   CommandInterface = ROS 2 -> Hardware
//   StateInterface   = Hardware -> ROS 2
// ============================================================
