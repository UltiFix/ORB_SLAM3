/**
* This file is part of ORB-SLAM3
*
* Copyright (C) 2017-2020 Carlos Campos, Richard Elvira, Juan J. Gómez Rodríguez, José M.M. Montiel and Juan D. Tardós, University of Zaragoza.
* Copyright (C) 2014-2016 Raúl Mur-Artal, José M.M. Montiel and Juan D. Tardós, University of Zaragoza.
*
* ORB-SLAM3 is free software: you can redistribute it and/or modify it under the terms of the GNU General Public
* License as published by the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* ORB-SLAM3 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even
* the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License along with ORB-SLAM3.
* If not, see <http://www.gnu.org/licenses/>.
*/
 
 
#include<iostream>
#include<algorithm>
#include<fstream>
#include<chrono>
#include <string>
#include <time.h>
 
#include<ros/ros.h>
#include <cv_bridge/cv_bridge.h>
#include <message_filters/subscriber.h>
#include <message_filters/time_synchronizer.h>
#include <message_filters/sync_policies/approximate_time.h>
 
#include<opencv2/core/core.hpp>
 
#include"../../../include/System.h"
 
 
#include <geometry_msgs/PoseStamped.h>
#include <tf/tf.h>
#include <tf/transform_datatypes.h>
#include"../../../include/Converter.h"
#include "std_msgs/String.h"
 
using namespace std;
 
 
bool operator ! (const cv::Mat&m) { return m.empty();}
string message1("if");
string message2("else");
 
FILE *ptr;
 
 
 
 
 
const char* ConvertDoubleToString(double value){
    std::stringstream ss ;
    ss << value;
    const char* str = ss.str().c_str();
    return str;
}
 
 
const std::string currentDateTime() {
    time_t     now = time(0);
    struct tm  tstruct;
    char       buf[80];
    tstruct = *localtime(&now);
    // Visit http://en.cppreference.com/w/cpp/chrono/c/strftime
    // for more information about date/time format
    strftime(buf, sizeof(buf), "%Y-%m-%d.%X", &tstruct);
 
    return buf;
}
 
 
class ImageGrabber
{
public:
    ImageGrabber(ORB_SLAM3::System* pSLAM):mpSLAM(pSLAM){}
 
    void GrabRGBD(const sensor_msgs::ImageConstPtr& msgRGB,const sensor_msgs::ImageConstPtr& msgD);
 
    ORB_SLAM3::System* mpSLAM;
};
 
int main(int argc, char **argv)
{
 
      stringstream fs;
      fs << currentDateTime() << "Orbslam" << ".csv" << endl; 
      string fileName1 = fs.str();
      const char * fileName = fileName1.c_str();
 
      ptr = fopen(fileName,"w");
      fprintf(ptr,"0, 1, 2, 3");
 
    ros::init(argc, argv, "RGBD");
    ros::start();
    ros::NodeHandle nh;
 
 
    if(argc != 3)
    {
        cerr << endl << "Usage: rosrun ORB_SLAM3 RGBD path_to_vocabulary path_to_settings" << endl;        
        ros::shutdown();
        return 1;
    }    
 
    // Create SLAM system. It initializes all system threads and gets ready to process frames.
    ORB_SLAM3::System SLAM(argv[1],argv[2],ORB_SLAM3::System::RGBD,true);
 
    ImageGrabber igb(&SLAM);
 
    message_filters::Subscriber<sensor_msgs::Image> rgb_sub(nh, "/camera/color/image_raw", 100);
    message_filters::Subscriber<sensor_msgs::Image> depth_sub(nh, "camera/aligned_depth_to_color/image_raw", 100);
    typedef message_filters::sync_policies::ApproximateTime<sensor_msgs::Image, sensor_msgs::Image> sync_pol;
    message_filters::Synchronizer<sync_pol> sync(sync_pol(10), rgb_sub,depth_sub);
    sync.registerCallback(boost::bind(&ImageGrabber::GrabRGBD,&igb,_1,_2));
 
    ros::spin();
 
    // Stop all threads
    SLAM.Shutdown();
 
    // Save camera trajectory
    SLAM.SaveKeyFrameTrajectoryTUM("KeyFrameTrajectory.txt");
    fclose(ptr);
    ros::shutdown();
 
    return 0;
}
 
void ImageGrabber::GrabRGBD(const sensor_msgs::ImageConstPtr& msgRGB,const sensor_msgs::ImageConstPtr& msgD)
{
    // Copy the ros image message to cv::Mat.
    cv_bridge::CvImageConstPtr cv_ptrRGB;
    try
    {
        cv_ptrRGB = cv_bridge::toCvShare(msgRGB);
    }
    catch (cv_bridge::Exception& e)
    {
        ROS_ERROR("cv_bridge exception: %s", e.what());
        return;
    }
 
    cv_bridge::CvImageConstPtr cv_ptrD;
    try
    {
        cv_ptrD = cv_bridge::toCvShare(msgD);
    }
    catch (cv_bridge::Exception& e)
    {
        ROS_ERROR("cv_bridge exception: %s", e.what());
        return;
    }
    bool test=0;
 
    cv::Mat Tcw = mpSLAM->TrackRGBD(cv_ptrRGB->image,cv_ptrD->image,cv_ptrRGB->header.stamp.toSec());
 
 
 
    geometry_msgs::PoseStamped pose;
    pose.header.stamp = ros::Time::now();
    pose.header.frame_id ="map";
 
 
    test = !Tcw;
    // test if 
    if(test==1){
        tf::Transform new_transform;
        tf::poseTFToMsg(new_transform, pose.pose);
        //pub.publish(pose);
        std::cout << message1 << "\n";
    }
    else{
    cv::Mat Rwc = Tcw.rowRange(0,3).colRange(0,3).t(); // Rotation information
    cv::Mat twc = -Rwc*Tcw.rowRange(0,3).col(3); // translation information
    vector<float> q = ORB_SLAM3::Converter::toQuaternion(Rwc);
 
    tf::Transform new_transform;
    new_transform.setOrigin(tf::Vector3(twc.at<float>(0, 0), twc.at<float>(0, 1), twc.at<float>(0, 2)));
 
    tf::Quaternion quaternion(q[0], q[1], q[2], q[3]);
    new_transform.setRotation(quaternion);
 
    tf::poseTFToMsg(new_transform, pose.pose);
 
    }
 
 
ros::Time stamp = ros::Time::now();
std::stringstream ss;
ss << stamp.sec << "." << stamp.nsec;
string str3 = ss.str();
 
const char * cc = str3.c_str();
 
 
 
fprintf(ptr,"\n");
fprintf(ptr,cc);
fprintf(ptr,",");
fprintf(ptr,ConvertDoubleToString(pose.pose.position.x));
fprintf(ptr,",");
fprintf(ptr,ConvertDoubleToString(pose.pose.position.z));
fprintf(ptr,",");
fprintf(ptr,ConvertDoubleToString(pose.pose.position.y));
fprintf(ptr,","); 
string str1(currentDateTime());
const char * c = str1.c_str();
fprintf(ptr,c);
 
 
}
