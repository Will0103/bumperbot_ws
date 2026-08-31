#include "rclcpp/rclcpp.hpp"
#include "nav_msgs/msg/odometry.hpp"
#include "nav_msgs/msg/path.hpp"
#include "geometry_msgs/msg/pose_stamped.hpp"

using namespace std::placeholders;

class BumperbotTrajectoryNode : public rclcpp::Node
{
public:
    BumperbotTrajectoryNode() : Node("bumperbot_trajectory")
    {
        sub_odom_ = this->create_subscription<nav_msgs::msg::Odometry>("bumperbot_controller/odom", 10, std::bind(&BumperbotTrajectoryNode::odom_callback, this, _1));
        pub_traj_ = this->create_publisher<nav_msgs::msg::Path>("bumperbot_controller/trajectory", 10);
    }
private:
    void odom_callback(const nav_msgs::msg::Odometry &msg)
    {
        
        auto post_stamped = geometry_msgs::msg::PoseStamped();
        msg_traj_.header.frame_id = msg.header.frame_id;
        msg_traj_.header.stamp = msg.header.stamp;
        
        post_stamped.header = msg_traj_.header;
        post_stamped.pose.position.x = msg.pose.pose.position.x;    
        post_stamped.pose.position.y = msg.pose.pose.position.y;    
        post_stamped.pose.orientation.x = msg.pose.pose.orientation.x;
        post_stamped.pose.orientation.y = msg.pose.pose.orientation.y;
        post_stamped.pose.orientation.z = msg.pose.pose.orientation.z;
        post_stamped.pose.orientation.w = msg.pose.pose.orientation.w;      
        
        msg_traj_.poses.push_back(post_stamped);

        pub_traj_->publish(msg_traj_);
    }

    rclcpp::Subscription<nav_msgs::msg::Odometry>::SharedPtr sub_odom_;
    rclcpp::Publisher<nav_msgs::msg::Path>::SharedPtr pub_traj_;
    nav_msgs::msg::Path msg_traj_;
};

int main(int argc, char **argv)
{
    rclcpp::init(argc, argv);
    auto node = std::make_shared<BumperbotTrajectoryNode>();
    rclcpp::spin(node);
    rclcpp::shutdown();
    return 0;
}