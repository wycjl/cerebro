#pragma once

/** DataManager - Inmemory database of data from VINS-estimator

        This attempts to store all the data from VINS-estimator
        in a in-memory database. Note that this has multiple threads.

        The data will be maintained as a std::map with key as timestamp,
        value as custom class DataNode. Other global data will also be kept.
        Callbacks are also handled here.

        Author  : Manohar Kuse <mpkuse@connect.ust.hk>
        Created : 3rd Oct, 2017
        Major Update : Jun, 2018
        Major Update : Oct, 2018

**/


#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <queue>
#include <ostream>
#include <iomanip>
#include <map>
#include <iterator>
#include <ctime>


#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>

//opencv
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/calib3d.hpp>
#include <cv_bridge/cv_bridge.h>

// ros
#include <ros/ros.h>
#include <ros/package.h>

// ros msg
#include <nav_msgs/Odometry.h>
#include <geometry_msgs/Pose.h>
#include <geometry_msgs/PoseWithCovariance.h>
#include <geometry_msgs/PoseStamped.h>
#include <sensor_msgs/PointCloud.h>
#include <geometry_msgs/Point32.h>
#include <sensor_msgs/Image.h>
// #include <nav_msgs/Path.h>
#include <geometry_msgs/Point.h>
#include <visualization_msgs/Marker.h>
#include <visualization_msgs/MarkerArray.h>

// Eigen
#include <Eigen/Core>
#include <Eigen/Dense>
#include <Eigen/Geometry>
using namespace Eigen;
#include <opencv2/core/eigen.hpp>

#include "PinholeCamera.h"
#include "camodocal/camera_models/Camera.h" // this is in include/camodocal. src files in src/utils/camodocal_src
#include "camodocal/camera_models/CameraFactory.h"

#include "DataNode.h"

#include "utils/PoseManipUtils.h"
#include "utils/RawFileIO.h"
#include "utils/SafeQueue.h"
#include "utils/ElapsedTime.h"

class DataManager
{
public:
    DataManager( ros::NodeHandle &nh );
    DataManager(const DataManager &obj);

    // void setCamera( const PinholeCamera& camera );
    // PinholeCamera& getCameraRef() { return camera;}

    void setAbstractCamera( camodocal::CameraPtr abs_camera, short cam_id=0 );
    camodocal::CameraPtr getAbstractCameraRef(short cam_id=0);
    bool isAbstractCameraSet(short cam_id=0);
    vector<short> getAbstractCameraKeys();

    void setCameraRelPose( Matrix4d a_T_b, std::pair<int,int> pair_a_b );
    bool isCameraRelPoseSet( std::pair<int,int> pair_a_b );
    const Matrix4d& getCameraRelPose( std::pair<int,int> pair_a_b );
    vector< std::pair<int,int> > getCameraRelPoseKeys();

    std::map< ros::Time, DataNode* >& getDataMapRef() { return data_map; }

    const ros::Time getPose0Stamp() { return pose_0; }
    bool isPose0Available() { return pose_0_available; }

    const Matrix4d& getIMUCamExtrinsic();
    bool isIMUCamExtrinsicAvailable();
    const ros::Time getIMUCamExtrinsicLastUpdated();


public:
    ////////
    //////// Write Data
    ////////
    // returns string as a json. contains everything including wTc, wX, uv, K, D etc.
    string metaDataAsJson(); // TODO: rewrite this function using the nlohmann/json.hpp.
    string metaDataAsFlatFile(); // a simple list of timestamps and what data is available.


private:
    /////////
    ///////// Global Variables
    /////////
    std::map< int, camodocal::CameraPtr > all_abstract_cameras;
    std::map< std::pair<int,int>,  Matrix4d > cam_relative_poses; //< pair:a,b then a_T_b


    ros::NodeHandle nh; //< Node Handle, TODO Not sure why this will be needed here. consider removing it from here.
    // const std::ofstream &out_stream;

    std::map< ros::Time, DataNode* > data_map;

    bool pose_0_available = false;
    ros::Time pose_0; // time of 1st pose

    Matrix4d imu_T_cam = Matrix4d::Identity();
    bool imu_T_cam_available = false;
    ros::Time imu_T_cam_stamp;

    std::mutex global_vars_mutex;


public:
    /////////
    ///////// Callbacks
    /////////
    void camera_pose_callback( const nav_msgs::Odometry::ConstPtr msg ); ///< w_T_c. pose of camera in the world-cordinate system. All the cameras. only a subset of this will be keyframes
    void keyframe_pose_callback( const nav_msgs::Odometry::ConstPtr msg ); //X /// pose of imu at keyframes. Use it just as a marker, dont use the poses.

    void raw_image_callback( const sensor_msgs::ImageConstPtr& msg );
    void raw_image_callback_1( const sensor_msgs::ImageConstPtr& msg );

    void extrinsic_cam_imu_callback( const nav_msgs::Odometry::ConstPtr msg );
    void ptcld_callback( const sensor_msgs::PointCloud::ConstPtr msg );
    void tracked_feat_callback( const sensor_msgs::PointCloud::ConstPtr msg ); //X


private:

    // callback-buffers
    // std::queue<sensor_msgs::ImageConstPtr> img_buf;
    // std::queue<nav_msgs::OdometryConstPtr> pose_buf;
    // std::queue<nav_msgs::OdometryConstPtr> kf_pose_buf;
    // std::queue<sensor_msgs::PointCloudConstPtr> ptcld_buf;
    // std::queue<sensor_msgs::PointCloudConstPtr> trackedfeat_buf;
    // std::queue<nav_msgs::OdometryConstPtr> extrinsic_cam_imu_buf;

    SafeQueue<sensor_msgs::ImageConstPtr> img_buf;
    SafeQueue<sensor_msgs::ImageConstPtr> img_1_buf;
    SafeQueue<nav_msgs::OdometryConstPtr> pose_buf;
    SafeQueue<nav_msgs::OdometryConstPtr> kf_pose_buf;
    SafeQueue<sensor_msgs::PointCloudConstPtr> ptcld_buf;
    SafeQueue<sensor_msgs::PointCloudConstPtr> trackedfeat_buf;
    SafeQueue<nav_msgs::OdometryConstPtr> extrinsic_cam_imu_buf;


    string print_queue_size(int verbose );


    /////////
    ///////// Threads
    /////////
public:
    // This threads monitors the buffer queues and sync them all in a std::map which is indexed with time and holds all the data
    void data_association_thread( int max_loop_rate_in_hz );
    void data_association_thread_enable() { b_data_association_thread = true; }
    void data_association_thread_disable() { b_data_association_thread = false; }


private:
    atomic<bool> b_data_association_thread;


};
