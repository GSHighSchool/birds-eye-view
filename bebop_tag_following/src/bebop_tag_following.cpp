/******************************************************************************
* bebopTagFollowing class
* This class is designed to track a tag using ar_track_alvar and move a Parrot 
* Bebop. It also communicates with a TurtleBot about tag detection status.
* 
* Author: Shannon Hood
* Last modified: 13 Feb 2017
*/

#include "bebop_tag_following.h"
using namespace std;

bebopTagFollowing::bebopTagFollowing(ros::NodeHandle& nh) {
  tagDetected = false;

  //publishers & services
  drone_comms = nh.advertise<std_msgs::UInt8>("/drone_comms", 1);
  cmdVel = nh.advertise<geometry_msgs::Twist>("/bebop/cmd_vel", 1);
  takeOff = nh.advertise<std_msgs::Empty>("/bebop/takeoff", 1);
  land = nh.advertise<std_msgs::Empty>("/bebop/land", 1);
  flattrim = nh.serviceClient<std_srvs::Empty>("/bebop/flattrim");

  //subscribers
  tagPose = nh.subscribe("/ar_pose_marker", 5, &bebopTagFollowing::positionCallback, this);
  //turtlebotPose = nh.subscribe("/odom", 100, &bebopTagFollowing::turtlebotCallback, this);
 
  usleep(1000*1000); //wait 1 second to ensure commands are published 
  std_srvs::Empty srv;
  flattrim.call(srv);
  usleep(100);
  initialize();
}

/******************************************************************************
* Function: initialize
* Input: none
*
* Makes the drone take off.
*/
void bebopTagFollowing::initialize() {
  startTime = ros::Time::now().toSec();
  state.x = state.y = state.z = 0;
  
   std_msgs::Empty blank;
   takeOff.publish(blank);
   usleep(100);

  // Let Turtlebot know to start
  std_msgs::UInt8 turtleInfo;
  turtleInfo.data = NO_TAG_FOUND;
  drone_comms.publish(turtleInfo);
  usleep(100);

  std::cout << "{INFO}: Bebop initialization finished" << std::endl;
}

/******************************************************************************
* Function: shutDown
* Input: none
*
* Called on exit. Clear all drone commands and land the drone
*/
void bebopTagFollowing::shutDown() {
  std_msgs::Empty blank;
  hover();
  land.publish(blank);
}

/******************************************************************************
* Function: hover
* Input: none
*
* Makes the drone take hover in one spot by sending 0's as velocities.
*/
void bebopTagFollowing::hover() {
  geometry_msgs::Twist cmdT;
  cmdT.angular.z = 0;
  cmdT.linear.z = 0;
  cmdT.linear.x = 0;
  cmdT.linear.y = 0;
  cmdT.angular.x = cmdT.angular.y = 0;
  cmdVel.publish(cmdT);
}
/**************************************************************************************
* Function: positionCallback
* Input: ar_track_alvar msg
* TODO: find the tag we want to follow and only use that tag
* 
* If the drone is using its bottom camera, then it will move towards the tag and 
* maintain its height.
*/
void bebopTagFollowing::positionCallback(const ar_track_alvar_msgs::AlvarMarkers::ConstPtr& msg) {
  currentTime = ros::Time::now().toSec();
  std_msgs::UInt8 turtleInfo;
  ostringstream s;
  std_msgs::String command;
  if(!msg->markers.empty()) { 
    timeDelta = currentTime-lastSeen;
    lastSeen = currentTime;

    if(!tagDetected) {
      printedWarn = false;
      tagDetected = true;
    }

    // Save velocities
    state.vx = -(state.x + msg->markers[0].pose.pose.position.x) / timeDelta; // state velocity
    state.vy = -(state.y + msg->markers[0].pose.pose.position.y) / timeDelta; // state velocity

    // Save x, y, z pose
    state.x = msg->markers[0].pose.pose.position.x;
    state.y = msg->markers[0].pose.pose.position.y;
    state.z = msg->markers[0].pose.pose.position.z;

    // Get message as quaternion, then get roll, pitch, yaw
    tf::Quaternion q(msg->markers[0].pose.pose.orientation.x, 
                     msg->markers[0].pose.pose.orientation.y, 
                     msg->markers[0].pose.pose.orientation.z, 
                     msg->markers[0].pose.pose.orientation.w);
    tf::Matrix3x3 m(q);
    double t_r, t_p, t_y;
    m.getRPY(t_r, t_p, t_y);

    // Transform x, y, z, and yaw to drones distance from tag
    state.x = -state.x; // Originally, forward is pos and backward is neg.
    state.y = -state.y; // Originally, left is neg and right is pos.
    state.z = -1*(state.z-HEIGHT_OVER_TAG);
    state.yaw = t_y * 180 / M_PI;
    state.yaw = (state.yaw - 90.0);
   /* if(state.yaw<10.0 && state.yaw>-10.0) {
      state.yaw = 0.0;
    } */
    
    ROS_INFO("Tag at: x=%1.2f  y=%1.2f  z=%1.2f  yaw=%1.2f", 
              state.x, state.y, state.z, state.yaw);
    ROS_INFO("Tag vel: vx=%1.3f  vy=%1.3f", state.vx, state.vy);

    // Let Turtlebot know a tag is found
    turtleInfo.data = FOUND_TAG;
    drone_comms.publish(turtleInfo);
    usleep(100);

    geometry_msgs::Twist cmdT;
    cmdT.angular.z = -state.yaw*0.4*M_PI/180;
    cmdT.linear.z = state.z*0.4;
    cmdT.linear.x = state.x*0.07+state.vx;
    cmdT.linear.y = state.y*0.07+state.vy;
    cmdT.angular.x = cmdT.angular.y = 0;
    cmdVel.publish(cmdT);
  }
  
  else { // There are no tags detected
    if(currentTime-lastSeen>1.5 && !printedWarn) {
        tagDetected = false;
        printedWarn = true;
        turtleInfo.data = LOST_TAG;
        drone_comms.publish(turtleInfo);
        usleep(100);

        hover();
        std::cout << "{WARN}: Tag lost by ar_track_alvar" << std::endl;
    }
  }
}

/******************************************************************************
* Function: turtlebotCallback
* Input: Predicted pose of the TurtleBot from the GVG algorithm
* TODO: Use the TurtleBot's pose when the drone loses sight of the tag
* 
* Inform the drone of the TurtleBot's location
*/
void bebopTagFollowing::turtlebotCallback(const nav_msgs::Odometry::ConstPtr& msg) {

}

/******************************************************************************
* Function: main
* Input: none required
*
* Runs this node, allowing the Bebop to start following tags
*/
main(int argc, char **argv) {
  ros::init(argc, argv, "pilot");
  ros::NodeHandle n;

  bebopTagFollowing pilot(n);
//  ros::Rate rate(15);
 // while(ros::ok()) {
 //   ros::spinOnce();
 //   rate.sleep();
 // }
  ros::spin();

  pilot.shutDown();
}