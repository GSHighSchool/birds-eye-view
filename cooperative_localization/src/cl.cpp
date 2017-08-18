/********************************************************************************************
* cl class
* 
* This class is designed to work with the Parrot Bebop and TurtleBot
* to provide cooperative localization of the drone from the TurtleBot's frame
* of reference using the TurtleBot's pose and ar_track_alvar tag info. Also 
* is responsible for scaling ORB-SLAM.
*
* Authors: Shannon Hood
* Last Edited: 23 January 2017
* 
**/

#include "ros/ros.h"
#include "std_msgs/String.h"
#include "sensor_msgs/Image.h"
#include "std_srvs/Empty.h"
#include "std_msgs/Bool.h"
#include "std_msgs/UInt8.h"
#include <cstdlib>
#include <geometry_msgs/Point.h>
#include <geometry_msgs/PoseWithCovarianceStamped.h>
#include <geometry_msgs/PoseArray.h>
#include "ar_track_alvar_msgs/AlvarMarkers.h"
#include "nav_msgs/Path.h"
#include "nav_msgs/Odometry.h"

#include <vector>
#include <ctime>
#include <sstream>
#include <math.h>
#include <iostream>
#include <fstream>

#include <tf/transform_listener.h>
#include "tf/transform_broadcaster.h"
#include "tf/message_filter.h"
using namespace std;

class cl {
  public:
  cl(ros::NodeHandle& nh) { 
    scaleX = 1;
    scaleY = 7.632; // default scale values calculated from a bag file
    scaleZ = 2.8;   // these are overwritten when the true scale factor is calculated
    first = true;
    seen = false;
    test_flag = true;
    orbSLAMSub = nh.subscribe("/ORB_SLAM/odom", 10000, &cl::orbSLAMCallback, this);
    tagDataSub = nh.subscribe("/ar_pose_marker", 100000, &cl::tagCallback, this);
    bebopOdomSub = nh.subscribe("/bebop/odom", 1, &cl::bebopCallback, this);
    turtlePathPub = nh.advertise<nav_msgs::Path>("/birdseye/cl_ugv_path", 1000);
    dronePathPub = nh.advertise<nav_msgs::Path>("/birdseye/cl_uav_path", 1000);
    orbSLAMPathPub = nh.advertise<nav_msgs::Path>("/birdseye/orbslam_path", 1000);
    //tfServer = new tf::TransformBroadcaster();  
    //tfListener = new tf::TransformListener();
    dronePath.header.frame_id = "map";
    turtlePath.header.frame_id = "map";
    orbSLAMPath.header.frame_id = "map";

    output.open ("3poses.txt");
    output << "Timestamp  TBx  TBy  TBz  CLx  CLy  CLz  ORBx  ORBy  ORBz" << endl;
  }
  ~cl(void) {
      output.close();

   // if(tfServer)
     // delete tfServer; 
  }

  void bebopCallback(const nav_msgs::Odometry::ConstPtr& odom) {
    /*lastBebopOdom.header = odom->header;
    lastBebopOdom.pose = odom->pose;
    lastBebopOdom.twist = odom->twist;*/
  }


  /********************************************************************************:
  * Function: tagCallback
  * Input: Tag pose from ar_track_alvar
  *
  * Transfrom tag data to the TurtleBot's frame of reference.
  * Then using the odom data from the TurtleBot, calculate the pose of the drone
  * using cooperative localization.
  */
  void tagCallback(const ar_track_alvar_msgs::AlvarMarkers::ConstPtr& msg) { 
    listener.waitForTransform("map", "base_link", 
            ros::Time(0), ros::Duration(5.0));
    try { 
      listener.lookupTransform("map", "base_link", ros::Time(0), tfTransform);
    } catch(tf::TransformException &exception) { 
      ROS_ERROR("%s", exception.what());
    }

    //==== Position ====//
    turtlePose.pose.position.x = tfTransform.getOrigin().x();
    turtlePose.pose.position.y = tfTransform.getOrigin().y();
    turtlePose.pose.position.z = tfTransform.getOrigin().z();
    turtlePose.pose.orientation.w = tfTransform.getRotation().w();
    turtlePose.pose.orientation.x = tfTransform.getRotation().x();
    turtlePose.pose.orientation.y = tfTransform.getRotation().y();
    turtlePose.pose.orientation.z = tfTransform.getRotation().z();

    //turtlePlan.push_back(turtlePose);
    turtlePath.poses.push_back(turtlePose);

    if(!turtlePath.poses.empty()){
      turtlePath.header.stamp = turtlePath.poses[0].header.stamp;
    }
    turtlePathPub.publish(turtlePath);
    if(!msg->markers.empty()) {
      tagPose = msg->markers[0].pose;
      dronePose = getDronePose(turtlePose);
      dronePath.poses.push_back(dronePose);
      turtlePose.header.stamp = msg->markers[0].header.stamp;

      if(!dronePath.poses.empty()){
        dronePath.header.stamp = dronePath.poses[0].header.stamp;
      }
      dronePathPub.publish(dronePath);
      seen = true; 
      lastSeen = ros::Time::now().toSec();
    } else {
          if(ros::Time::now().toSec()-lastSeen>2) seen = false;
    }
  }


  geometry_msgs::PoseStamped getDronePose(geometry_msgs::PoseStamped turtlePose) {
    geometry_msgs::PoseStamped dronePose;
    dronePose = turtlePose;

    dronePose.pose.position.x += tagPose.pose.position.x;
    dronePose.pose.position.y += tagPose.pose.position.y;
    dronePose.pose.position.z += tagPose.pose.position.z+.45; //.45 height of tb

    //might need -90*M_PI/180
    dronePose.pose.orientation.w += tagPose.pose.orientation.w;
    dronePose.pose.orientation.x += tagPose.pose.orientation.x;
    dronePose.pose.orientation.y += tagPose.pose.orientation.y;
    dronePose.pose.orientation.z += tagPose.pose.orientation.z;

    nav_msgs::Odometry odom_data;

    odom_data.pose.pose.position.x = dronePose.pose.position.x;
    odom_data.pose.pose.position.y = dronePose.pose.position.y;
    odom_data.pose.pose.position.z = dronePose.pose.position.z;
    odom_data.pose.pose.orientation.x = dronePose.pose.orientation.x;
    odom_data.pose.pose.orientation.y = dronePose.pose.orientation.y;
    odom_data.pose.pose.orientation.z = dronePose.pose.orientation.z;
    odom_data.pose.pose.orientation.w = dronePose.pose.orientation.w;
    lastBebopOdom = odom_data;
    return dronePose;
  }

  void orbSLAMCallback(const nav_msgs::Odometry::ConstPtr& odom) {
    orbSLAMPose.header.stamp = odom->header.stamp;
    if(first && seen) {
      origTurtlePose = turtlePose;
      startHeight = lastBebopOdom.pose.pose.position.z;
      meterAheadTB = odom->pose.pose.position.z-lastBebopOdom.pose.pose.position.y;
      k1ORBSLamOdom.pose = odom->pose;
      k1ORBSLamOdom.twist = odom->twist;
      k1ORBSLamOdom.header = odom->header;
      k1BebopOdom = lastBebopOdom; // make sure they are from the same timestamp
      first = false;
      usleep(1000);
    } 
    //calculate scale factor
    else if(!first && ros::Time::now().toSec()-lastScaled>1 && seen) {
      nav_msgs::Odometry kbebop = lastBebopOdom;
      double bebopX = kbebop.pose.pose.position.x-k1BebopOdom.pose.pose.position.x;
      double orbSLAMX = odom->pose.pose.position.x-k1ORBSLamOdom.pose.pose.position.x;
      double bebopY = kbebop.pose.pose.position.y-k1BebopOdom.pose.pose.position.y;
      double orbSLAMY = odom->pose.pose.position.z-k1ORBSLamOdom.pose.pose.position.z;
      double bebopZ = bebop.pose.pose.position.z-k1BebopOdom.pose.pose.position.z;
      double orbSLAMZ = odom->pose.pose.position.y-k1ORBSLamOdom.pose.pose.position.y;
      if(orbSLAMX != 0 && bebopX != 0) scaleX = bebopX/orbSLAMX;
      if(orbSLAMY != 0 && bebopY != 0) scaleY = bebopY/orbSLAMY;
      if(orbSLAMZ != 0 && bebopZ != 0) scaleZ = bebopZ/orbSLAMZ;
      lastScaled = ros::Time::now().toSec();
      
      if(test_flag) {
      cout << "////////////////ORB-SLAM Scale Factors////////////////" << endl;
      cout << "ScaleX = " << scaleX << endl; //left/right
      cout << "ScaleY = " << scaleY << endl; //forward
      cout << "ScaleZ = " << scaleZ << "\n" << endl; //height
      cout << "DifforbSLAMY = " << odom->pose.pose.position.z-k1ORBSLamOdom.pose.pose.position.z << endl;
      cout << "DiffBebopY = " <<lastBebopOdom.pose.pose.position.y-k1BebopOdom.pose.pose.position.y << "\n"<< endl;
      cout << "orbSLAMPoseX = " << orbSLAMPose.pose.position.x << endl;
      cout << "orbSLAMPoseY = " << orbSLAMPose.pose.position.y << endl;
      cout << "orbSLAMPoseZ = " << orbSLAMPose.pose.position.z << endl;
      cout << "//////////////////////////////////////////////////////\n" << endl;
      }
    }

    if(!first){
      orbSLAMPose.pose.position.x = odom->pose.pose.position.x*abs(scaleX);
      orbSLAMPose.pose.position.y = odom->pose.pose.position.z*abs(scaleY); //orbslam y and z axis swapped
      orbSLAMPose.pose.position.z = odom->pose.pose.position.y*abs(scaleZ);

      orbSLAMPose.pose.position.x = origTurtlePose.pose.position.x+orbSLAMPose.pose.position.x;
      orbSLAMPose.pose.position.y = origTurtlePose.pose.position.y+orbSLAMPose.pose.position.y;
      orbSLAMPose.pose.position.z = origTurtlePose.pose.position.z+orbSLAMPose.pose.position.z;
      orbSLAMPose.pose.orientation.w = origTurtlePose.pose.orientation.w;
      orbSLAMPose.pose.orientation.x = odom->pose.pose.orientation.x;
      orbSLAMPose.pose.orientation.y = odom->pose.pose.orientation.y;
      orbSLAMPose.pose.orientation.z = odom->pose.pose.orientation.z;

      orbSLAMPose.pose.position.y += meterAheadTB;
      orbSLAMPose.pose.position.z += startHeight;
      orbSLAMPath.poses.push_back(orbSLAMPose);

      if(!orbSLAMPath.poses.empty()){
        orbSLAMPath.header.stamp = orbSLAMPath.poses[orbSLAMPath.poses.size()-1].header.stamp;
      }

      orbSLAMPathPub.publish(orbSLAMPath);
      output << turtlePose.header.stamp << "  "
             << turtlePose.pose.position.x << "  " << turtlePose.pose.position.y << "  " << turtlePose.pose.position.z << "  "
             << dronePose.pose.position.x << "  " << dronePose.pose.position.y << "  " << dronePose.pose.position.z << "  "
             << orbSLAMPose.pose.position.x << "  " << orbSLAMPose.pose.position.y << "  " << orbSLAMPose.pose.position.z << endl;
      output.flush();
    }
  }

  
  /*********************************************************************************
  * private cl variables
  */
  private:
    ros::Subscriber tagDataSub;
    ros::Subscriber orbSLAMSub;
    ros::Subscriber bebopOdomSub;

    ros::Publisher turtlePathPub;
    ros::Publisher dronePathPub;
    ros::Publisher orbSLAMPathPub;

    geometry_msgs::PoseStamped turtlePose;
    geometry_msgs::PoseStamped origTurtlePose;
    geometry_msgs::PoseStamped orbSLAMPose;
    geometry_msgs::PoseStamped dronePose;
    geometry_msgs::PoseStamped tagPose;
    nav_msgs::Path dronePath;  
    nav_msgs::Path turtlePath;
    nav_msgs::Path orbSLAMPath;

    nav_msgs::Odometry lastBebopOdom;
    nav_msgs::Odometry k1BebopOdom;
    nav_msgs::Odometry k1ORBSLamOdom;
    tf::TransformListener listener;
    tf::StampedTransform tfTransform;

    ofstream output;

    bool first;
    bool test_flag;
    bool seen;
    double scaleX;
    double scaleY;
    double scaleZ;;
    double lastScaled;  
    double lastSeen;  
    double meterAheadTB;
    double startHeight;
};

int main(int argc, char **argv) {
  //create the node ros functions can be used
  ros::init(argc, argv, "listener");
  ros::NodeHandle n;
 // ros::Rate rate(30);
  
  std::cout<<"CL initializing"<< std::endl;
  int seq = 0;

  cl pilot(n);

  //while(ros::ok()) {
    //rate.sleep();
    ros::spin();
  //}
    
  return 0;
}
