// This is an advanced implementation of the algorithm described in the
// following paper:
//   J. Zhang and S. Singh. LOAM: Lidar Odometry and Mapping in Real-time.
//     Robotics: Science and Systems Conference (RSS). Berkeley, CA, July 2014.

// Modifier: Livox               dev@livoxtech.com

// Copyright 2013, Ji Zhang, Carnegie Mellon University
// Further contributions copyright (c) 2016, Southwest Research Institute
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
//    this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from this
//    software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
#include <omp.h>
#include <math.h>
#include <unistd.h>
#include <Python.h>
#include <Eigen/Core>
#include <opencv/cv.h>
#include <common_lib.h>
#include <nav_msgs/Odometry.h>
#include <opencv2/core/eigen.hpp>
#include <visualization_msgs/Marker.h>
#include <pcl_conversions/pcl_conversions.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
#include <pcl/filters/voxel_grid.h>
#include <pcl/kdtree/kdtree_flann.h>
#include <pcl/io/pcd_io.h>

#include <ros/ros.h>
#include <sensor_msgs/PointCloud2.h>
#include <tf/transform_datatypes.h>
#include <tf/transform_broadcaster.h>
#include <livox_loam_kp/KeyPointPose.h>
#include <geometry_msgs/Vector3.h>
#include "Exp_mat.h"
#include "matplotlibcpp.h"

namespace plt = matplotlibcpp;

// #define USING_CORNER
#define NUM_MATCH_POINTS (5)
#define NUM_MAX_ITERATIONS (15)
#define LASER_FRAME_INTEVAL (0.1)
#define LASER_POINT_COV (0.001)

typedef pcl::PointXYZI PointType;

int kfNum = 0;
int iterCount = 0;

float timeLaserCloudCornerLast = 0;
float timeLaserCloudSurfLast   = 0;
float timeLaserCloudFullRes    = 0;
double timeIMUkpLast = 0;
double timeIMUkpCur  = 0;

bool newLaserCloudCornerLast = false;
bool newLaserCloudSurfLast   = false;
bool newLaserCloudFullRes    = false;

int laserCloudCenWidth  = 10;
int laserCloudCenHeight = 5;
int laserCloudCenDepth  = 10;
const int laserCloudWidth  = 21;
const int laserCloudHeight = 11;
const int laserCloudDepth  = 21;

const int laserCloudNum = laserCloudWidth * laserCloudHeight * laserCloudDepth;//4851
int count_effect_point  = 0;

int laserCloudValidInd[125];

int laserCloudSurroundInd[125];

//corner feature
pcl::PointCloud<PointType>::Ptr laserCloudCornerLast(new pcl::PointCloud<PointType>());
pcl::PointCloud<PointType>::Ptr laserCloudCornerLast_down(new pcl::PointCloud<PointType>());
//surf feature
std::deque<sensor_msgs::PointCloud2> LaserCloudSurfaceBuff;
pcl::PointCloud<PointType>::Ptr laserCloudSurfLast(new pcl::PointCloud<PointType>());
pcl::PointCloud<PointType>::Ptr laserCloudSurf_down(new pcl::PointCloud<PointType>());
pcl::PointCloud<PointType>::Ptr laserCloudOri(new pcl::PointCloud<PointType>());
pcl::PointCloud<PointType>::Ptr coeffSel(new pcl::PointCloud<PointType>());

// pcl::PointCloud<PointType>::Ptr laserCloudSurround(new pcl::PointCloud<PointType>());
// pcl::PointCloud<PointType>::Ptr laserCloudSurround_corner(new pcl::PointCloud<PointType>());

pcl::PointCloud<PointType>::Ptr laserCloudSurround2(new pcl::PointCloud<PointType>());
pcl::PointCloud<PointType>::Ptr laserCloudSurround2_corner(new pcl::PointCloud<PointType>());
//corner feature in map
pcl::PointCloud<PointType>::Ptr laserCloudCornerFromMap(new pcl::PointCloud<PointType>());
//surf feature in map
pcl::PointCloud<PointType>::Ptr laserCloudSurfFromMap(new pcl::PointCloud<PointType>());

std::vector< Eigen::Matrix<float,7,1> > keyframe_pose;
std::vector< Eigen::Matrix4f > pose_map;
std::deque< livox_loam_kp::KeyPointPose > rot_kp_imu_buff;
//all points
pcl::PointCloud<PointType>::Ptr laserCloudFullRes(new pcl::PointCloud<PointType>());
pcl::PointCloud<PointType>::Ptr laserCloudFullRes2(new pcl::PointCloud<PointType>());
pcl::PointCloud<pcl::PointXYZRGB>::Ptr laserCloudFullResColor(new pcl::PointCloud<pcl::PointXYZRGB>());
pcl::PointCloud<pcl::PointXYZRGB>::Ptr laserCloudFullResColor_pcd(new pcl::PointCloud<pcl::PointXYZRGB>());


pcl::PointCloud<PointType>::Ptr laserCloudCornerArray[laserCloudNum];

pcl::PointCloud<PointType>::Ptr laserCloudSurfArray[laserCloudNum];

pcl::PointCloud<PointType>::Ptr laserCloudCornerArray2[laserCloudNum];

pcl::PointCloud<PointType>::Ptr laserCloudSurfArray2[laserCloudNum];

pcl::KdTreeFLANN<PointType>::Ptr kdtreeCornerFromMap(new pcl::KdTreeFLANN<PointType>());
pcl::KdTreeFLANN<PointType>::Ptr kdtreeSurfFromMap(new pcl::KdTreeFLANN<PointType>());

//optimization states 
float transformTobeMapped[6] = {0};
//optimization states after mapping
float transformAftMapped[6] = {0};
//last optimization states
float transformLastMapped[6] = {0};

//estimated rotation and translation;
Eigen::Matrix3d R_global_cur(Eigen::Matrix3d::Identity());
Eigen::Matrix3d R_global_last(Eigen::Matrix3d::Identity());
Eigen::Vector3d T_global_cur(0, 0, 0);
Eigen::Vector3d T_global_last(0, 0, 0);
Eigen::Vector3d V_global_cur(0, 0, 0);
Eigen::Vector3d V_global_last(0, 0, 0);
// Eigen::MatrixXf cov_stat_cur(Eigen::Matrix<float, DIM_OF_STATES, DIM_OF_STATES>::Zero());

//final iteration resdual
float deltaR = 0.0;
float deltaT = 0.0;

double rad2deg(double radians)
{
  return radians * 180.0 / M_PI;
}
double deg2rad(double degrees)
{
  return degrees * M_PI / 180.0;
}

void transformUpdate()
{
    for (int i = 0; i < 6; i++) {
        transformLastMapped[i] = transformAftMapped[i];
        transformAftMapped[i] = transformTobeMapped[i];
    }
}
//lidar coordinate sys to world coordinate sys
void pointAssociateToMap(PointType const * const pi, PointType * const po)
{
    Eigen::Vector3f p_body(pi->x, pi->y, pi->z);
    Eigen::Vector3f p_global(R_global_cur.cast<float>() * p_body + T_global_cur.cast<float>());
    
    po->x = p_global(0);
    po->y = p_global(1);
    po->z = p_global(2);
    po->intensity = pi->intensity;
}

void RGBpointAssociateToMap(PointType const * const pi, pcl::PointXYZRGB * const po)
{
    Eigen::Vector3f p_body(pi->x, pi->y, pi->z);
    Eigen::Vector3f p_global(R_global_cur.cast<float>() * p_body + T_global_cur.cast<float>());
    
    po->x = p_global(0);
    po->y = p_global(1);
    po->z = p_global(2);
    //po->intensity = pi->intensity;

    float intensity = pi->intensity;
    intensity = intensity - std::floor(intensity);

    int reflection_map = intensity*10000;

    //std::cout<<"DEBUG reflection_map "<<reflection_map<<std::endl;

    if (reflection_map < 30)
    {
        int green = (reflection_map * 255 / 30);
        po->r = 0;
        po->g = green & 0xff;
        po->b = 0xff;
    }
    else if (reflection_map < 90)
    {
        int blue = (((90 - reflection_map) * 255) / 60);
        po->r = 0x0;
        po->g = 0xff;
        po->b = blue & 0xff;
    }
    else if (reflection_map < 150)
    {
        int red = ((reflection_map-90) * 255 / 60);
        po->r = red & 0xff;
        po->g = 0xff;
        po->b = 0x0;
    }
    else
    {
        int green = (((255-reflection_map) * 255) / (255-150));
        po->r = 0xff;
        po->g = green & 0xff;
        po->b = 0;
    }
}

// #ifdef USING_CORNER
void laserCloudCornerLastHandler(const sensor_msgs::PointCloud2ConstPtr& laserCloudCornerLast2)
{
    timeLaserCloudCornerLast = laserCloudCornerLast2->header.stamp.toSec();

    laserCloudCornerLast->clear();
    pcl::fromROSMsg(*laserCloudCornerLast2, *laserCloudCornerLast);

    newLaserCloudCornerLast = true;
}
// #endif

void laserCloudSurfLastHandler(const sensor_msgs::PointCloud2ConstPtr& laserCloudSurfLast2)
{
    LaserCloudSurfaceBuff.push_back(*laserCloudSurfLast2);

    timeLaserCloudSurfLast = LaserCloudSurfaceBuff.front().header.stamp.toSec();

    newLaserCloudSurfLast = true;
}

void laserCloudFullResHandler(const sensor_msgs::PointCloud2ConstPtr& laserCloudFullRes2)
{
    laserCloudFullRes->clear();
    laserCloudFullResColor->clear();
    pcl::fromROSMsg(*laserCloudFullRes2, *laserCloudFullRes);

    timeLaserCloudFullRes = laserCloudFullRes2->header.stamp.toSec();

    newLaserCloudFullRes = true;
}

void KeyPointPose6DHandler(const livox_loam_kp::KeyPointPoseConstPtr& KeyPointPose)
{
    rot_kp_imu_buff.push_back(*KeyPointPose);

    timeIMUkpCur = rot_kp_imu_buff.front().header.stamp.toSec();
}

bool sync_packages()
{
    if(rot_kp_imu_buff.size() != LaserCloudSurfaceBuff.size())
    {
        return false;
    }
    // while (fabs(timeIMUkpLast - timeLaserCloudSurfLast) > 0.6 * LASER_FRAME_INTEVAL)
    // {
    //     (timeIMUkpLast > timeLaserCloudSurfLast) ? LaserCloudSurfaceBuff.pop_front() : rot_kp_imu_buff.pop_front();
    //     std::cout<<"`````````````````````````````````````````````````````````````````````````"<<std::endl;

    //     if(rot_kp_imu_buff.empty() || LaserCloudSurfaceBuff.empty()) {return false;}

    //     timeIMUkpLast = rot_kp_imu_buff.front().header.stamp.toSec();
    //     timeLaserCloudSurfLast = LaserCloudSurfaceBuff.front().header.stamp.toSec();
    // }
    // std::cout<<"Sync TIme: "<< timeIMUkpLast << " " << timeLaserCloudSurfLast <<std::endl;
    return true;
}

int main(int argc, char** argv)
{
    // Py_Initialize(); /*初始化python解释器,告诉编译器要用的python编译器*/
	// PyRun_SimpleString("import matplotlib.pyplot as plt"); /*调用python文件*/
    // PyRun_SimpleString("plt.ion()");
	// PyRun_SimpleString("plt.bar([1,2,3],[2,1,3])"); /*调用python文件*/
	// PyRun_SimpleString("plt.show()"); /*调用python文件*/

    // plt::ion();

    ros::init(argc, argv, "laserMapping");
    ros::NodeHandle nh;

    ros::Subscriber subLaserCloudCornerLast = nh.subscribe<sensor_msgs::PointCloud2>
            ("/laser_cloud_sharp", 100, laserCloudCornerLastHandler);

#ifdef USING_CORNER
    ros::Subscriber subLaserCloudFullRes = nh.subscribe<sensor_msgs::PointCloud2>
            ("/livox_cloud", 100, laserCloudFullResHandler);
#else
    ros::Subscriber subLaserCloudFullRes = nh.subscribe<sensor_msgs::PointCloud2>
            ("/livox_undistort", 100, laserCloudFullResHandler);
#endif

    ros::Subscriber subLaserCloudSurfLast = nh.subscribe<sensor_msgs::PointCloud2>
            ("/livox_undistort", 100, laserCloudSurfLastHandler);
    ros::Subscriber KeyPointPose6D = nh.subscribe<livox_loam_kp::KeyPointPose>
            ("/Pose6D_IMUKeyPoints", 100, KeyPointPose6DHandler);

    // ros::Subscriber subIMUOri = nh.subscribe<sensor_msgs::>

    ros::Publisher pubLaserCloudSurround = nh.advertise<sensor_msgs::PointCloud2>
            ("/laser_cloud_surround", 100);
    ros::Publisher pubLaserCloudSurround_corner = nh.advertise<sensor_msgs::PointCloud2>
            ("/laser_cloud_surround_corner", 100);
    ros::Publisher pubLaserCloudFullRes = nh.advertise<sensor_msgs::PointCloud2>
            ("/velodyne_cloud_registered", 100);
    ros::Publisher pubLaserCloudMap = nh.advertise<sensor_msgs::PointCloud2>
            ("/Laser_map", 100);
    ros::Publisher pubSolvedPose6D = nh.advertise<livox_loam_kp::KeyPointPose>
            ("/Pose6D_Solved", 100);
    
    ros::Publisher pubOdomAftMapped = nh.advertise<nav_msgs::Odometry> ("/aft_mapped_to_init", 10);
    nav_msgs::Odometry odomAftMapped;
    odomAftMapped.header.frame_id = "/camera_init";
    odomAftMapped.child_frame_id = "/aft_mapped";
    livox_loam_kp::KeyPointPose Pose6D_Solved;

    std::string map_file_path;
    ros::param::get("~map_file_path",map_file_path);
    double filter_parameter_corner;
    ros::param::get("~filter_parameter_corner",filter_parameter_corner);
    double filter_parameter_surf;
    ros::param::get("~filter_parameter_surf",filter_parameter_surf);

    PointType pointOri, pointSel, coeff;

    cv::Mat matA1(3, 3, CV_32F, cv::Scalar::all(0));
    cv::Mat matD1(1, 3, CV_32F, cv::Scalar::all(0));
    cv::Mat matV1(3, 3, CV_32F, cv::Scalar::all(0));

    bool isDegenerate = false;
    cv::Mat matP(6, 6, CV_32F, cv::Scalar::all(0));
    //VoxelGrid
    pcl::VoxelGrid<PointType> downSizeFilterCorner;
    pcl::VoxelGrid<PointType> downSizeFilterSurf;
    pcl::VoxelGrid<PointType> downSizeFilterMap;

    downSizeFilterCorner.setLeafSize(filter_parameter_corner, filter_parameter_corner, filter_parameter_corner);
    downSizeFilterSurf.setLeafSize(filter_parameter_surf, filter_parameter_surf, filter_parameter_surf);
    downSizeFilterMap.setLeafSize(0.1, 0.1, 0.1);

    for (int i = 0; i < laserCloudNum; i++)
    {
        laserCloudCornerArray[i].reset(new pcl::PointCloud<PointType>());
        laserCloudSurfArray[i].reset(new pcl::PointCloud<PointType>());
        laserCloudCornerArray2[i].reset(new pcl::PointCloud<PointType>());
        laserCloudSurfArray2[i].reset(new pcl::PointCloud<PointType>());
    }
    
    std::vector<double> T1, s_plot, s_plot2, s_plot3;

//------------------------------------------------------------------------------------------------------
    ros::Rate rate(100);
    bool status = ros::ok();
    while (status)
    {
        ros::spinOnce();

        while(!LaserCloudSurfaceBuff.empty() && !rot_kp_imu_buff.empty() && sync_packages()) 
        {
            // std::cout<<"~~~~~~~~~~~"<<LaserCloudSurfaceBuff.size()<<" "<<rot_kp_imu_buff.size()<<" "<<timeIMUkpLast<<" "<<timeLaserCloudSurfLast<<std::endl;
            double t1,t2,t3,t4;
            double match_start, match_time, solve_start, solve_time, pca_time, svd_time;

            match_time = 0;
            solve_time = 0;
            pca_time   = 0;
            svd_time   = 0;

            t1 = omp_get_wtime();

            newLaserCloudCornerLast = false;
            newLaserCloudSurfLast = false;
            newLaserCloudFullRes = false;

            //transformAssociateToMap();
            std::cout<<"DEBUG mapping start "<<std::endl;

            PointType pointOnYAxis;
            pointOnYAxis.x = 0.0;
            pointOnYAxis.y = 10.0;
            pointOnYAxis.z = 0.0;
            
            /** Get the rotations and translations of IMU keypoints in a frame **/
            Eigen::Vector3d gravity, bias_g, bias_a;
            Eigen::Matrix<double, DIM_OF_STATES, DIM_OF_STATES> cov_stat_cur;

            gravity<<VEC_FROM_ARRAY(rot_kp_imu_buff.front().gravity);
            bias_g<<VEC_FROM_ARRAY(rot_kp_imu_buff.front().bias_gyr);
            bias_a<<VEC_FROM_ARRAY(rot_kp_imu_buff.front().bias_acc);
            T_global_cur<<VEC_FROM_ARRAY(rot_kp_imu_buff.front().pos_end);
            V_global_cur<<VEC_FROM_ARRAY(rot_kp_imu_buff.front().vel_end);
            R_global_cur=Eigen::Map<Eigen::Matrix3d>(rot_kp_imu_buff.front().rot_end.data());
            cov_stat_cur=Eigen::Map<Eigen::Matrix<double, DIM_OF_STATES, DIM_OF_STATES> >(rot_kp_imu_buff.front().cov.data());

            Eigen::Matrix3f R_global_f(R_global_cur.cast <float> ());
            Eigen::Vector3f euler_cur = correct_pi(R_global_f.eulerAngles(1, 0, 2));

            transformTobeMapped[0]  = euler_cur(0);
            transformTobeMapped[1]  = euler_cur(1);
            transformTobeMapped[2]  = euler_cur(2);
            transformTobeMapped[3]  = T_global_cur(0);
            transformTobeMapped[4]  = T_global_cur(1);
            transformTobeMapped[5]  = T_global_cur(2);

            std::cout<<"******pre-integrated euler angle: "<<euler_cur[0]<<" " \
                    <<euler_cur[1]<<" "<<euler_cur[2]<<" pos:"<<transformTobeMapped[3]<<" " \
                    <<transformTobeMapped[4]<<" "<<transformTobeMapped[5]<<std::endl;
            
            pointAssociateToMap(&pointOnYAxis, &pointOnYAxis);

            int centerCubeI = int((transformTobeMapped[3] + 25.0) / 50.0) + laserCloudCenWidth;
            int centerCubeJ = int((transformTobeMapped[4] + 25.0) / 50.0) + laserCloudCenHeight;
            int centerCubeK = int((transformTobeMapped[5] + 25.0) / 50.0) + laserCloudCenDepth;

            if (transformTobeMapped[3] + 25.0 < 0) centerCubeI--;
            if (transformTobeMapped[4] + 25.0 < 0) centerCubeJ--;
            if (transformTobeMapped[5] + 25.0 < 0) centerCubeK--;

            while (centerCubeI < 3)
            {
                for (int j = 0; j < laserCloudHeight; j++)
                {
                    for (int k = 0; k < laserCloudDepth; k++)
                    {
                        int i = laserCloudWidth - 1;

                        pcl::PointCloud<PointType>::Ptr laserCloudCubeCornerPointer =
                                laserCloudCornerArray[i + laserCloudWidth * j + laserCloudWidth * laserCloudHeight * k];
                        pcl::PointCloud<PointType>::Ptr laserCloudCubeSurfPointer =
                                laserCloudSurfArray[i + laserCloudWidth * j + laserCloudWidth * laserCloudHeight * k];

                        for (; i >= 1; i--) {
                            laserCloudCornerArray[i + laserCloudWidth * j + laserCloudWidth * laserCloudHeight * k] =
                                    laserCloudCornerArray[i - 1 + laserCloudWidth*j + laserCloudWidth * laserCloudHeight * k];
                            laserCloudSurfArray[i + laserCloudWidth * j + laserCloudWidth * laserCloudHeight * k] =
                                    laserCloudSurfArray[i - 1 + laserCloudWidth * j + laserCloudWidth * laserCloudHeight * k];
                        }

                        laserCloudCornerArray[i + laserCloudWidth * j + laserCloudWidth * laserCloudHeight * k] =
                                laserCloudCubeCornerPointer;
                        laserCloudSurfArray[i + laserCloudWidth * j + laserCloudWidth * laserCloudHeight * k] =
                                laserCloudCubeSurfPointer;
                        laserCloudCubeCornerPointer->clear();
                        laserCloudCubeSurfPointer->clear();
                    }
                }

                centerCubeI++;
                laserCloudCenWidth++;
            }

            while (centerCubeI >= laserCloudWidth - 3) {
                for (int j = 0; j < laserCloudHeight; j++) {
                    for (int k = 0; k < laserCloudDepth; k++) {
                        int i = 0;
                        pcl::PointCloud<PointType>::Ptr laserCloudCubeCornerPointer =
                                laserCloudCornerArray[i + laserCloudWidth * j + laserCloudWidth * laserCloudHeight * k];
                        pcl::PointCloud<PointType>::Ptr laserCloudCubeSurfPointer =
                                laserCloudSurfArray[i + laserCloudWidth * j + laserCloudWidth * laserCloudHeight * k];

                        for (; i < laserCloudWidth - 1; i++)
                        {
                            laserCloudCornerArray[i + laserCloudWidth * j + laserCloudWidth * laserCloudHeight * k] =
                                    laserCloudCornerArray[i + 1 + laserCloudWidth*j + laserCloudWidth * laserCloudHeight * k];
                            laserCloudSurfArray[i + laserCloudWidth * j + laserCloudWidth * laserCloudHeight * k] =
                                    laserCloudSurfArray[i + 1 + laserCloudWidth * j + laserCloudWidth * laserCloudHeight * k];
                        }

                        laserCloudCornerArray[i + laserCloudWidth * j + laserCloudWidth * laserCloudHeight * k] =
                                laserCloudCubeCornerPointer;
                        laserCloudSurfArray[i + laserCloudWidth * j + laserCloudWidth * laserCloudHeight * k] =
                                laserCloudCubeSurfPointer;
                        laserCloudCubeCornerPointer->clear();
                        laserCloudCubeSurfPointer->clear();
                    }
                }

                centerCubeI--;
                laserCloudCenWidth--;
            }

            while (centerCubeJ < 3) {
                for (int i = 0; i < laserCloudWidth; i++) {
                    for (int k = 0; k < laserCloudDepth; k++) {
                        int j = laserCloudHeight - 1;
                        pcl::PointCloud<PointType>::Ptr laserCloudCubeCornerPointer =
                                laserCloudCornerArray[i + laserCloudWidth * j + laserCloudWidth * laserCloudHeight * k];
                        pcl::PointCloud<PointType>::Ptr laserCloudCubeSurfPointer =
                                laserCloudSurfArray[i + laserCloudWidth * j + laserCloudWidth * laserCloudHeight * k];

                        for (; j >= 1; j--) {
                            laserCloudCornerArray[i + laserCloudWidth * j + laserCloudWidth * laserCloudHeight * k] =
                                    laserCloudCornerArray[i + laserCloudWidth*(j - 1) + laserCloudWidth * laserCloudHeight*k];
                            laserCloudSurfArray[i + laserCloudWidth * j + laserCloudWidth * laserCloudHeight * k] =
                                    laserCloudSurfArray[i + laserCloudWidth * (j - 1) + laserCloudWidth * laserCloudHeight*k];
                        }
                        laserCloudCornerArray[i + laserCloudWidth * j + laserCloudWidth * laserCloudHeight * k] =
                                laserCloudCubeCornerPointer;
                        laserCloudSurfArray[i + laserCloudWidth * j + laserCloudWidth * laserCloudHeight * k] =
                                laserCloudCubeSurfPointer;
                        laserCloudCubeCornerPointer->clear();
                        laserCloudCubeSurfPointer->clear();
                    }
                }

                centerCubeJ++;
                laserCloudCenHeight++;
            }

            while (centerCubeJ >= laserCloudHeight - 3) {
                for (int i = 0; i < laserCloudWidth; i++) {
                    for (int k = 0; k < laserCloudDepth; k++) {
                        int j = 0;
                        pcl::PointCloud<PointType>::Ptr laserCloudCubeCornerPointer =
                                laserCloudCornerArray[i + laserCloudWidth * j + laserCloudWidth * laserCloudHeight * k];
                        pcl::PointCloud<PointType>::Ptr laserCloudCubeSurfPointer =
                                laserCloudSurfArray[i + laserCloudWidth * j + laserCloudWidth * laserCloudHeight * k];

                        for (; j < laserCloudHeight - 1; j++) {
                            laserCloudCornerArray[i + laserCloudWidth * j + laserCloudWidth * laserCloudHeight * k] =
                                    laserCloudCornerArray[i + laserCloudWidth*(j + 1) + laserCloudWidth * laserCloudHeight*k];
                            laserCloudSurfArray[i + laserCloudWidth * j + laserCloudWidth * laserCloudHeight * k] =
                                    laserCloudSurfArray[i + laserCloudWidth * (j + 1) + laserCloudWidth * laserCloudHeight*k];
                        }
                        laserCloudCornerArray[i + laserCloudWidth * j + laserCloudWidth * laserCloudHeight * k] =
                                laserCloudCubeCornerPointer;
                        laserCloudSurfArray[i + laserCloudWidth * j + laserCloudWidth * laserCloudHeight * k] =
                                laserCloudCubeSurfPointer;
                        laserCloudCubeCornerPointer->clear();
                        laserCloudCubeSurfPointer->clear();
                    }
                }

                centerCubeJ--;
                laserCloudCenHeight--;
            }

            while (centerCubeK < 3) {
                for (int i = 0; i < laserCloudWidth; i++) {
                    for (int j = 0; j < laserCloudHeight; j++) {
                        int k = laserCloudDepth - 1;
                        pcl::PointCloud<PointType>::Ptr laserCloudCubeCornerPointer =
                                laserCloudCornerArray[i + laserCloudWidth * j + laserCloudWidth * laserCloudHeight * k];
                        pcl::PointCloud<PointType>::Ptr laserCloudCubeSurfPointer =
                                laserCloudSurfArray[i + laserCloudWidth * j + laserCloudWidth * laserCloudHeight * k];

                        for (; k >= 1; k--) {
                            laserCloudCornerArray[i + laserCloudWidth * j + laserCloudWidth * laserCloudHeight * k] =
                                    laserCloudCornerArray[i + laserCloudWidth*j + laserCloudWidth * laserCloudHeight*(k - 1)];
                            laserCloudSurfArray[i + laserCloudWidth * j + laserCloudWidth * laserCloudHeight * k] =
                                    laserCloudSurfArray[i + laserCloudWidth * j + laserCloudWidth * laserCloudHeight*(k - 1)];
                        }
                        laserCloudCornerArray[i + laserCloudWidth * j + laserCloudWidth * laserCloudHeight * k] =
                                laserCloudCubeCornerPointer;
                        laserCloudSurfArray[i + laserCloudWidth * j + laserCloudWidth * laserCloudHeight * k] =
                                laserCloudCubeSurfPointer;
                        laserCloudCubeCornerPointer->clear();
                        laserCloudCubeSurfPointer->clear();
                    }
                }

                centerCubeK++;
                laserCloudCenDepth++;
            }

            while (centerCubeK >= laserCloudDepth - 3)
            {
                for (int i = 0; i < laserCloudWidth; i++)
                {
                    for (int j = 0; j < laserCloudHeight; j++)
                    {
                        int k = 0;
                        pcl::PointCloud<PointType>::Ptr laserCloudCubeCornerPointer =
                                laserCloudCornerArray[i + laserCloudWidth * j + laserCloudWidth * laserCloudHeight * k];
                        pcl::PointCloud<PointType>::Ptr laserCloudCubeSurfPointer =
                                laserCloudSurfArray[i + laserCloudWidth * j + laserCloudWidth * laserCloudHeight * k];
                    
                        for (; k < laserCloudDepth - 1; k++)
                        {
                            laserCloudCornerArray[i + laserCloudWidth * j + laserCloudWidth * laserCloudHeight * k] =
                                    laserCloudCornerArray[i + laserCloudWidth*j + laserCloudWidth * laserCloudHeight*(k + 1)];
                            laserCloudSurfArray[i + laserCloudWidth * j + laserCloudWidth * laserCloudHeight * k] =
                                    laserCloudSurfArray[i + laserCloudWidth * j + laserCloudWidth * laserCloudHeight*(k + 1)];
                        }

                        laserCloudCornerArray[i + laserCloudWidth * j + laserCloudWidth * laserCloudHeight * k] =
                                laserCloudCubeCornerPointer;
                        laserCloudSurfArray[i + laserCloudWidth * j + laserCloudWidth * laserCloudHeight * k] =
                                laserCloudCubeSurfPointer;
                        laserCloudCubeCornerPointer->clear();
                        laserCloudCubeSurfPointer->clear();
                    }
                }

                centerCubeK--;
                laserCloudCenDepth--;
            }

            int laserCloudValidNum    = 0;
            int laserCloudSurroundNum = 0;

            for (int i = centerCubeI - 2; i <= centerCubeI + 2; i++) 
            {
                for (int j = centerCubeJ - 2; j <= centerCubeJ + 2; j++) 
                {
                    for (int k = centerCubeK - 2; k <= centerCubeK + 2; k++) 
                    {
                        if (i >= 0 && i < laserCloudWidth &&
                                j >= 0 && j < laserCloudHeight &&
                                k >= 0 && k < laserCloudDepth) 
                        {

                            float centerX = 50.0 * (i - laserCloudCenWidth);
                            float centerY = 50.0 * (j - laserCloudCenHeight);
                            float centerZ = 50.0 * (k - laserCloudCenDepth);

                            bool isInLaserFOV = false;
                            for (int ii = -1; ii <= 1; ii += 2) 
                            {
                                for (int jj = -1; jj <= 1; jj += 2) 
                                {
                                    for (int kk = -1; kk <= 1; kk += 2) 
                                    {

                                        float cornerX = centerX + 25.0 * ii;
                                        float cornerY = centerY + 25.0 * jj;
                                        float cornerZ = centerZ + 25.0 * kk;

                                        float squaredSide1 = (transformTobeMapped[3] - cornerX)
                                                * (transformTobeMapped[3] - cornerX)
                                                + (transformTobeMapped[4] - cornerY)
                                                * (transformTobeMapped[4] - cornerY)
                                                + (transformTobeMapped[5] - cornerZ)
                                                * (transformTobeMapped[5] - cornerZ);

                                        float squaredSide2 = (pointOnYAxis.x - cornerX) * (pointOnYAxis.x - cornerX)
                                                + (pointOnYAxis.y - cornerY) * (pointOnYAxis.y - cornerY)
                                                + (pointOnYAxis.z - cornerZ) * (pointOnYAxis.z - cornerZ);

                                        float check1 = 100.0 + squaredSide1 - squaredSide2
                                                - 10.0 * sqrt(3.0) * sqrt(squaredSide1);

                                        float check2 = 100.0 + squaredSide1 - squaredSide2
                                                + 10.0 * sqrt(3.0) * sqrt(squaredSide1);

                                        if (check1 < 0 && check2 > 0) {
                                            isInLaserFOV = true;
                                        }
                                    }
                                }
                            }

                            if (isInLaserFOV)
                            {
                                laserCloudValidInd[laserCloudValidNum] = i + laserCloudWidth * j
                                        + laserCloudWidth * laserCloudHeight * k;
                                laserCloudValidNum ++;
                            }
                            laserCloudSurroundInd[laserCloudSurroundNum] = i + laserCloudWidth * j
                                    + laserCloudWidth * laserCloudHeight * k;
                            laserCloudSurroundNum ++;
                        }
                    }
                }
            }

            laserCloudCornerFromMap->clear();
            laserCloudSurfFromMap->clear();
            
            for (int i = 0; i < laserCloudValidNum; i++)
            {
                // *laserCloudCornerFromMap += *laserCloudCornerArray[laserCloudValidInd[i]];
                *laserCloudSurfFromMap += *laserCloudSurfArray[laserCloudValidInd[i]];

            }

#ifdef USING_CORNER
            laserCloudCornerLast_down->clear();
            downSizeFilterCorner.setInputCloud(laserCloudCornerLast);
            downSizeFilterCorner.filter(*laserCloudCornerLast_down);
#endif
            laserCloudSurfLast->clear();
            laserCloudSurf_down->clear();
            pcl::fromROSMsg(LaserCloudSurfaceBuff.front(), *laserCloudSurfLast);
            downSizeFilterSurf.setInputCloud(laserCloudSurfLast);
            downSizeFilterSurf.filter(*laserCloudSurf_down);

            downSizeFilterMap.setInputCloud(laserCloudSurfFromMap);
            downSizeFilterMap.filter(*laserCloudSurfFromMap);

            int laserCloudSurfFromMapNum = laserCloudSurfFromMap->points.size();
            int laserCloudSurf_down_size = laserCloudSurf_down->points.size();

#ifdef USING_CORNER
            std::cout<<"DEBUG MAPPING laserCloudCornerLast_down : "<<laserCloudCornerLast_down->points.size()<<" laserCloudSurf_down : "
            <<laserCloudSurf_down_size<<std::endl;
#endif
            std::cout<<"DEBUG MAPPING laserCloudSurf_down : "<<laserCloudSurf_down_size<<" laserCloudSurf : "
            <<laserCloudSurfLast->points.size()<<std::endl;
            std::cout<<"DEBUG MAPPING laserCloudValidNum : "<<laserCloudValidNum<<" laserCloudSurfFromMapNum : "
            <<laserCloudSurfFromMapNum<<std::endl;

            

            pcl::PointCloud<PointType>::Ptr coeffSel_tmpt
                (new pcl::PointCloud<PointType>(*laserCloudSurf_down));
            pcl::PointCloud<PointType>::Ptr laserCloudSurf_down_updated
                (new pcl::PointCloud<PointType>(*laserCloudSurf_down));

            t2 = omp_get_wtime();

#ifdef USING_CORNER
            if (laserCloudCornerFromMapNum > 10 && laserCloudSurfFromMapNum > 100)
#else
            if (laserCloudSurfFromMapNum > 100)
#endif
            {
#ifdef USING_CORNER
                kdtreeCornerFromMap->setInputCloud(laserCloudCornerFromMap);
#endif
                match_start = omp_get_wtime();

                pcl::KdTreeFLANN<PointType> kdtreeSurfFromMap_tmpt;
                kdtreeSurfFromMap_tmpt.setInputCloud(laserCloudSurfFromMap);
                
                int num_temp = 0;
                
                for (iterCount = 0; iterCount < NUM_MAX_ITERATIONS; iterCount++) 
                {
                    num_temp++;
                    laserCloudOri->clear();
                    coeffSel->clear();
                    std::vector<bool> point_selected(laserCloudSurf_down_size, false);

#ifdef USING_CORNER
                    for (int i = 0; i < laserCloudCornerLast->points.size(); i++) 
                    {
                        pointOri = laserCloudCornerLast->points[i];

                        pointAssociateToMap(&pointOri, &pointSel);
                        //find the closest 5 points
                        kdtreeCornerFromMap->nearestKSearch(pointSel, 5, pointSearchInd, pointSearchSqDis);

                        if (pointSearchSqDis[4] < 1.5)
                        {
                            float cx = 0;
                            float cy = 0;
                            float cz = 0;
                            for (int j = 0; j < 5; j++)
                            {
                                cx += laserCloudCornerFromMap->points[pointSearchInd[j]].x;
                                cy += laserCloudCornerFromMap->points[pointSearchInd[j]].y;
                                cz += laserCloudCornerFromMap->points[pointSearchInd[j]].z;
                            }
                            cx /= 5;
                            cy /= 5;
                            cz /= 5;
                            //mean square error
                            float a11 = 0;
                            float a12 = 0;
                            float a13 = 0;
                            float a22 = 0;
                            float a23 = 0;
                            float a33 = 0;
                            for (int j = 0; j < 5; j++)
                            {
                                float ax = laserCloudCornerFromMap->points[pointSearchInd[j]].x - cx;
                                float ay = laserCloudCornerFromMap->points[pointSearchInd[j]].y - cy;
                                float az = laserCloudCornerFromMap->points[pointSearchInd[j]].z - cz;

                                a11 += ax * ax;
                                a12 += ax * ay;
                                a13 += ax * az;
                                a22 += ay * ay;
                                a23 += ay * az;
                                a33 += az * az;
                            }
                            a11 /= 5;
                            a12 /= 5;
                            a13 /= 5;
                            a22 /= 5;
                            a23 /= 5;
                            a33 /= 5;

                            matA1.at<float>(0, 0) = a11;
                            matA1.at<float>(0, 1) = a12;
                            matA1.at<float>(0, 2) = a13;
                            matA1.at<float>(1, 0) = a12;
                            matA1.at<float>(1, 1) = a22;
                            matA1.at<float>(1, 2) = a23;
                            matA1.at<float>(2, 0) = a13;
                            matA1.at<float>(2, 1) = a23;
                            matA1.at<float>(2, 2) = a33;

                            cv::eigen(matA1, matD1, matV1);

                            if (matD1.at<float>(0, 0) > 3 * matD1.at<float>(0, 1))
                            {
                                float x0 = pointSel.x;
                                float y0 = pointSel.y;
                                float z0 = pointSel.z;
                                float x1 = cx + 0.1 * matV1.at<float>(0, 0);
                                float y1 = cy + 0.1 * matV1.at<float>(0, 1);
                                float z1 = cz + 0.1 * matV1.at<float>(0, 2);
                                float x2 = cx - 0.1 * matV1.at<float>(0, 0);
                                float y2 = cy - 0.1 * matV1.at<float>(0, 1);
                                float z2 = cz - 0.1 * matV1.at<float>(0, 2);

                                //OA = (x0 - x1, y0 - y1, z0 - z1),OB = (x0 - x2, y0 - y2, z0 - z2)，AB = （x1 - x2, y1 - y2, z1 - z2）
                                //cross:
                                //|  i      j      k  |
                                //|x0-x1  y0-y1  z0-z1|
                                //|x0-x2  y0-y2  z0-z2|
                                float a012 = sqrt(((x0 - x1)*(y0 - y2) - (x0 - x2)*(y0 - y1))
                                                    * ((x0 - x1)*(y0 - y2) - (x0 - x2)*(y0 - y1))
                                                    + ((x0 - x1)*(z0 - z2) - (x0 - x2)*(z0 - z1))
                                                    * ((x0 - x1)*(z0 - z2) - (x0 - x2)*(z0 - z1))
                                                    + ((y0 - y1)*(z0 - z2) - (y0 - y2)*(z0 - z1))
                                                    * ((y0 - y1)*(z0 - z2) - (y0 - y2)*(z0 - z1)));

                                float l12 = sqrt((x1 - x2)*(x1 - x2) + (y1 - y2)*(y1 - y2) + (z1 - z2)*(z1 - z2));

                                float la = ((y1 - y2)*((x0 - x1)*(y0 - y2) - (x0 - x2)*(y0 - y1))
                                            + (z1 - z2)*((x0 - x1)*(z0 - z2) - (x0 - x2)*(z0 - z1))) / a012 / l12;

                                float lb = -((x1 - x2)*((x0 - x1)*(y0 - y2) - (x0 - x2)*(y0 - y1))
                                                - (z1 - z2)*((y0 - y1)*(z0 - z2) - (y0 - y2)*(z0 - z1))) / a012 / l12;

                                float lc = -((x1 - x2)*((x0 - x1)*(z0 - z2) - (x0 - x2)*(z0 - z1))
                                                + (y1 - y2)*((y0 - y1)*(z0 - z2) - (y0 - y2)*(z0 - z1))) / a012 / l12;

                                float ld2 = a012 / l12;
                                //if(fabs(ld2) > 1) continue;

                                float s = 1 - 0.9 * fabs(ld2);

                                coeff.x = s * la;
                                coeff.y = s * lb;
                                coeff.z = s * lc;
                                coeff.intensity = s * ld2;

                                if (s > 0.1)
                                {
                                    laserCloudOri->push_back(pointOri);
                                    coeffSel->push_back(coeff);
                                }
                            }
                        }
                    }
#endif
                    // std::cout <<"DEBUG mapping select corner points : " << coeffSel->size() << std::endl;
                    // variables for the points matching

                    omp_set_num_threads(4);
                    #pragma omp parallel for
                    for (int i = 0; i < laserCloudSurf_down_size; i++)
                    {
                        PointType &pointOri_tmpt = laserCloudSurf_down->points[i];
                        PointType &pointSel_tmpt = laserCloudSurf_down_updated->points[i];
                        pointAssociateToMap(&pointOri_tmpt, &pointSel_tmpt);

                        std::vector<int> pointSearchInd;
                        std::vector<float> pointSearchSqDis;
                        kdtreeSurfFromMap_tmpt.nearestKSearch(pointSel_tmpt, NUM_MATCH_POINTS, pointSearchInd, pointSearchSqDis);

                        double svd_start = omp_get_wtime();

                        // Eigen::Vector3f mass_center(0,0,0);
                        // Eigen::Matrix<float,NUM_MATCH_POINTS,3> pointSearched;

                        // for (int j = 0; j < NUM_MATCH_POINTS; j++)
                        // {
                        //     auto &p_searched = laserCloudSurfFromMap->points[pointSearchInd[j]];
                        //     mass_center[0] += p_searched.x;
                        //     mass_center[1] += p_searched.y;
                        //     mass_center[2] += p_searched.z;
                        //     pointSearched.row(j) << p_searched.x, p_searched.y, p_searched.z;
                        // }

                        // mass_center = mass_center / NUM_MATCH_POINTS;

                        // for (int j = 0; j < NUM_MATCH_POINTS; j++)
                        // {
                        //     pointSearched.row(j) -= mass_center;
                        // }

                        // Eigen::JacobiSVD<Eigen::MatrixXf> svd(pointSearched, Eigen::ComputeThinV );
                        // const auto &E = svd.singularValues();
                        // const auto &V = svd.matrixV();
                        // // const auto &U = svd.matrixU();

                        // float res = 0;
                        // if (E[0] > 3.0 * E[2])
                        // {
                        //     auto &p = pointSel_tmpt;
                        //     res = (mass_center - Eigen::Vector3f(p.x, p.y, p.z)).transpose() * V.col(2);

                        //     coeff_tmpt.x = -1 * V(0,2);
                        //     coeff_tmpt.y = -1 * V(1,2);
                        //     coeff_tmpt.z = -1 * V(2,2);
                        //     coeff_tmpt.intensity = res;

                        //     // point_selected[i] = true;
                        //     coeffSel_tmpt->points[i] = coeff_tmpt;

                        //     // static int jjj = 0; jjj ++; if (jjj % 1000 == 0) {std::cout<<"!!!!!!SVD eigens: "<<E<<" V: "<<V <<" res: "<<res<<std::endl;
                        //     //         std::cout<<coeff_tmpt.x<<" "<<coeff_tmpt.y<<" "<<coeff_tmpt.z<<" "<<coeff_tmpt.intensity<<std::endl;}
                        // }

                        svd_time += omp_get_wtime() - svd_start;
                        double pca_start = omp_get_wtime();

                        /// using minimum square method
                        
                        if (*max_element(pointSearchSqDis.begin(), pointSearchSqDis.end()) < 0.5)
                        {
                            cv::Mat matA0(NUM_MATCH_POINTS, 3, CV_32F, cv::Scalar::all(0));
                            cv::Mat matB0(NUM_MATCH_POINTS, 1, CV_32F, cv::Scalar::all(-1));
                            cv::Mat matX0(NUM_MATCH_POINTS, 1, CV_32F, cv::Scalar::all(0));

                            for (int j = 0; j < NUM_MATCH_POINTS; j++)
                            {
                                matA0.at<float>(j, 0) = laserCloudSurfFromMap->points[pointSearchInd[j]].x;
                                matA0.at<float>(j, 1) = laserCloudSurfFromMap->points[pointSearchInd[j]].y;
                                matA0.at<float>(j, 2) = laserCloudSurfFromMap->points[pointSearchInd[j]].z;
                            }

                            //matA0*matX0=matB0
                            //AX+BY+CZ+D = 0 <=> AX+BY+CZ=-D <=> (A/D)X+(B/D)Y+(C/D)Z = -1
                            //(X,Y,Z)<=>mat_a0
                            //A/D, B/D, C/D <=> mat_x0
                
                            cv::solve(matA0, matB0, matX0, cv::DECOMP_QR);  //TODO

                            float pa = matX0.at<float>(0, 0);
                            float pb = matX0.at<float>(1, 0);
                            float pc = matX0.at<float>(2, 0);
                            float pd = 1;

                            //ps is the norm of the plane normal vector
                            //pd is the distance from point to plane
                            float ps = sqrt(pa * pa + pb * pb + pc * pc);
                            pa /= ps;
                            pb /= ps;
                            pc /= ps;
                            pd /= ps;

                            bool planeValid = true;
                            for (int j = 0; j < NUM_MATCH_POINTS; j++)
                            {
                                if (fabs(pa * laserCloudSurfFromMap->points[pointSearchInd[j]].x +
                                         pb * laserCloudSurfFromMap->points[pointSearchInd[j]].y +
                                         pc * laserCloudSurfFromMap->points[pointSearchInd[j]].z + pd) > 0.05)
                                {
                                    planeValid = false;
                                    break;
                                }
                            }

                            if (planeValid) 
                            {
                                //loss fuction
                                float pd2 = pa * pointSel_tmpt.x + pb * pointSel_tmpt.y + pc * pointSel_tmpt.z + pd;
                                //if(fabs(pd2) > 0.1) continue;
                                float s = 1 - 0.9 * fabs(pd2) / sqrt(sqrt(pointSel_tmpt.x * pointSel_tmpt.x + pointSel_tmpt.y * pointSel_tmpt.y + pointSel_tmpt.z * pointSel_tmpt.z));

                                if (s > 0.1)
                                {
                                    point_selected[i] = true;
                                    coeffSel_tmpt->points[i].x = s * pa;
                                    coeffSel_tmpt->points[i].y = s * pb;
                                    coeffSel_tmpt->points[i].z = s * pc;
                                    coeffSel_tmpt->points[i].intensity = s * pd2;
                                }
                            }
                        }

                        pca_time += omp_get_wtime() - pca_start;
                    }

                    for (int i = 0; i < coeffSel_tmpt->points.size(); i++)
                    {
                        if (point_selected[i])
                        {
                            laserCloudOri->push_back(laserCloudSurf_down->points[i]);
                            coeffSel->push_back(coeffSel_tmpt->points[i]);
                        }
                    }

                    // std::cout << "DEBUG mapping select all points : " << coeffSel->size() << "  " << count_effect_point << std::endl;
                    count_effect_point = 0;

                    match_time += omp_get_wtime() - match_start;
                    match_start = omp_get_wtime();
                    solve_start = omp_get_wtime();

                    int laserCloudSelNum = laserCloudOri->points.size();
                    if (laserCloudSelNum < 50) {
                        continue;
                    }

                    /// |c1c3+s1s2s3 c3s1s2-c1s3 c2s1|
                    /// |   c2s3        c2c3      -s2|
                    /// |c1s2s3-c3s1 c1c3s2+s1s3 c1c2|
                    /// AT*A*x = AT*b
                    cv::Mat matA(laserCloudSelNum, 6, CV_32F, cv::Scalar::all(0));
                    cv::Mat matAt(6, laserCloudSelNum, CV_32F, cv::Scalar::all(0));
                    cv::Mat matAtA(6, 6, CV_32F, cv::Scalar::all(0));
                    cv::Mat matB(laserCloudSelNum, 1, CV_32F, cv::Scalar::all(0));
                    cv::Mat matAtB(6, 1, CV_32F, cv::Scalar::all(0));
                    cv::Mat matX(6, 1, CV_32F, cv::Scalar::all(0));

                    float debug_distance = 0;
                    R_global_f = R_global_cur.cast<float> ();

                    for (int i = 0; i < laserCloudSelNum; i++)
                    {
                        pointOri = laserCloudOri->points[i];
                        coeff = coeffSel->points[i];
                        float point_this[3] = {pointOri.x, pointOri.y, pointOri.z};
                        Eigen::Matrix3f point_crossmat;
                        point_crossmat<<SKEW_SYM_MATRX(point_this);
                        Eigen::Vector3f vect_norm(coeff.x, coeff.y, coeff.z);
                        Eigen::Vector3f A(point_crossmat * R_global_f.transpose() * vect_norm);

                        //TODO: the partial derivative
                        matA.at<float>(i, 0) = A(0);
                        matA.at<float>(i, 1) = A(1);
                        matA.at<float>(i, 2) = A(2);
                        matA.at<float>(i, 3) = coeff.x;
                        matA.at<float>(i, 4) = coeff.y;
                        matA.at<float>(i, 5) = coeff.z;
                        matB.at<float>(i, 0) = -coeff.intensity;

                        debug_distance += fabs(coeff.intensity);
                    }
                    cv::transpose(matA, matAt);
                    matAtA = matAt * matA;
                    matAtB = matAt * matB;
                    cv::solve(matAtA, matAtB, matX, cv::DECOMP_QR);

                    //Deterioration judgment
                    if (iterCount == 0)
                    {
                        cv::Mat matE(1, 6, CV_32F, cv::Scalar::all(0));
                        cv::Mat matV(6, 6, CV_32F, cv::Scalar::all(0));
                        cv::Mat matV2(6, 6, CV_32F, cv::Scalar::all(0));

                        cv::eigen(matAtA, matE, matV);
                        matV.copyTo(matV2);

                        isDegenerate = false;
                        float eignThre[6] = {1, 1, 1, 1, 1, 1};
                        for (int i = 5; i >= 0; i--)
                        {
                            if (matE.at<float>(0, i) < eignThre[i])
                            {
                                for (int j = 0; j < 6; j++)
                                {
                                    matV2.at<float>(i, j) = 0;
                                }
                                isDegenerate = true;
                            }
                            else
                            {
                                break;
                            }
                        }
                        matP = matV.inv() * matV2;
                    }

                    if (isDegenerate)
                    {
                        cv::Mat matX2(6, 1, CV_32F, cv::Scalar::all(0));
                        matX.copyTo(matX2);
                        matX = matP * matX2;
                    }

                    Eigen::Vector3d rot_add, t_add;
                    for (int ind = 0; ind < 3; ind ++)
                    {
                        rot_add[ind] = matX.at<float>(ind, 0);
                        t_add[ind]   = matX.at<float>(ind+3, 0);
                    }

                    R_global_cur = R_global_cur * Exp(rot_add);
                    T_global_cur = T_global_cur + t_add;
                    V_global_cur = (T_global_cur - T_global_last) / (timeIMUkpCur - timeIMUkpLast);
                    Eigen::Vector3d euler_cur = correct_pi(R_global_cur.eulerAngles(1, 0, 2));

                    transformTobeMapped[0] = euler_cur(0);
                    transformTobeMapped[1] = euler_cur(1);
                    transformTobeMapped[2] = euler_cur(2);
                    transformTobeMapped[3] = T_global_cur(0);
                    transformTobeMapped[4] = T_global_cur(1);
                    transformTobeMapped[5] = T_global_cur(2);

                    deltaR = sqrt(
                                pow(rad2deg(matX.at<float>(0, 0)), 2) +
                                pow(rad2deg(matX.at<float>(1, 0)), 2) +
                                pow(rad2deg(matX.at<float>(2, 0)), 2));
                    deltaT = sqrt(
                                pow(matX.at<float>(3, 0) * 100, 2) +
                                pow(matX.at<float>(4, 0) * 100, 2) +
                                pow(matX.at<float>(5, 0) * 100, 2));
                    
                    // Eigen::Vector3f pose6d(transformTobeMapped);
                    std::cout<<"transformTobeMapped: "<<transformTobeMapped[0] <<" " \
                             <<transformTobeMapped[1]<<" " <<transformTobeMapped[2]<<" " <<transformTobeMapped[3] <<" " \
                             <<transformTobeMapped[4]<<" " <<transformTobeMapped[5] << " delta R and T: "<<deltaR<<" "<<deltaT<<std::endl;

                    solve_time += omp_get_wtime() - solve_start;
                    
                    if (deltaR < 0.02 && deltaT < 0.03)
                    {
                        break;
                    }
                }

                // std::cout<<"DEBUG num_temp: "<<num_temp << std::endl;
                std::cout<<"current time: "<<timeIMUkpCur<<" iteration count: "<<iterCount+1<<std::endl;
                transformUpdate();
            }

            t3 = omp_get_wtime();

            /*** save results ***/
            set_states(Pose6D_Solved, rot_kp_imu_buff.front().header, gravity, bias_g, bias_a, \
                       T_global_cur, V_global_cur, R_global_cur, cov_stat_cur); std::cout<<"!!!! Sent pose:"<<T_global_cur.transpose()<<std::endl;
            Pose6D_Solved.pose6D.clear();
            pubSolvedPose6D.publish(Pose6D_Solved);

            V_global_last = V_global_cur;
            T_global_last = T_global_cur;
            R_global_last = R_global_cur;
            timeIMUkpLast = timeIMUkpCur;

            LaserCloudSurfaceBuff.pop_front();
            rot_kp_imu_buff.pop_front();
            if (!LaserCloudSurfaceBuff.empty() && !rot_kp_imu_buff.empty())
            {
                timeIMUkpCur  = rot_kp_imu_buff.front().header.stamp.toSec();
                timeLaserCloudSurfLast = LaserCloudSurfaceBuff.front().header.stamp.toSec();
            }

#ifdef USING_CORNER
            for (int i = 0; i < laserCloudCornerLast->points.size(); i++)
            {
                pointAssociateToMap(&laserCloudCornerLast->points[i], &pointSel);

                int cubeI = int((pointSel.x + 25.0) / 50.0) + laserCloudCenWidth;
                int cubeJ = int((pointSel.y + 25.0) / 50.0) + laserCloudCenHeight;
                int cubeK = int((pointSel.z + 25.0) / 50.0) + laserCloudCenDepth;

                if (pointSel.x + 25.0 < 0) cubeI--;
                if (pointSel.y + 25.0 < 0) cubeJ--;
                if (pointSel.z + 25.0 < 0) cubeK--;

                if (cubeI >= 0 && cubeI < laserCloudWidth &&
                        cubeJ >= 0 && cubeJ < laserCloudHeight &&
                        cubeK >= 0 && cubeK < laserCloudDepth)
                {

                    int cubeInd = cubeI + laserCloudWidth * cubeJ + laserCloudWidth * laserCloudHeight * cubeK;
                    laserCloudCornerArray[cubeInd]->push_back(pointSel);
                }
            }
#endif

            for (int i = 0; i < laserCloudSurf_down_size; i++)
            {
                PointType &pointSel = laserCloudSurf_down_updated->points[i];

                int cubeI = int((pointSel.x + 25.0) / 50.0) + laserCloudCenWidth;
                int cubeJ = int((pointSel.y + 25.0) / 50.0) + laserCloudCenHeight;
                int cubeK = int((pointSel.z + 25.0) / 50.0) + laserCloudCenDepth;

                if (pointSel.x + 25.0 < 0) cubeI--;
                if (pointSel.y + 25.0 < 0) cubeJ--;
                if (pointSel.z + 25.0 < 0) cubeK--;

                if (cubeI >= 0 && cubeI < laserCloudWidth &&
                        cubeJ >= 0 && cubeJ < laserCloudHeight &&
                        cubeK >= 0 && cubeK < laserCloudDepth) {
                    int cubeInd = cubeI + laserCloudWidth * cubeJ + laserCloudWidth * laserCloudHeight * cubeK;
                    laserCloudSurfArray[cubeInd]->push_back(pointSel);
                }
            }

            for (int i = 0; i < laserCloudValidNum; i++)
            {
                int ind = laserCloudValidInd[i];

                laserCloudCornerArray2[ind]->clear();
                downSizeFilterCorner.setInputCloud(laserCloudCornerArray[ind]);
                downSizeFilterCorner.filter(*laserCloudCornerArray2[ind]);

                laserCloudSurfArray2[ind]->clear();
                downSizeFilterSurf.setInputCloud(laserCloudSurfArray[ind]);
                downSizeFilterSurf.filter(*laserCloudSurfArray2[ind]);

                pcl::PointCloud<PointType>::Ptr laserCloudTemp = laserCloudCornerArray[ind];
                laserCloudCornerArray[ind] = laserCloudCornerArray2[ind];
                laserCloudCornerArray2[ind] = laserCloudTemp;

                laserCloudTemp = laserCloudSurfArray[ind];
                laserCloudSurfArray[ind] = laserCloudSurfArray2[ind];
                laserCloudSurfArray2[ind] = laserCloudTemp;
            }

            /*** Publish messages:  ***/
            laserCloudSurround2->clear();
            laserCloudSurround2_corner->clear();

            for (int i = 0; i < laserCloudSurroundNum; i++) {
                int ind = laserCloudSurroundInd[i];
                *laserCloudSurround2_corner += *laserCloudCornerArray[ind];
                *laserCloudSurround2 += *laserCloudSurfArray[ind];
            }

            sensor_msgs::PointCloud2 laserCloudSurround3;
            pcl::toROSMsg(*laserCloudSurround2, laserCloudSurround3);
            laserCloudSurround3.header.stamp = ros::Time().fromSec(timeLaserCloudCornerLast);
            laserCloudSurround3.header.frame_id = "/camera_init";
            pubLaserCloudSurround.publish(laserCloudSurround3);

            sensor_msgs::PointCloud2 laserCloudSurround3_corner;
            pcl::toROSMsg(*laserCloudSurround2_corner, laserCloudSurround3_corner);
            laserCloudSurround3_corner.header.stamp = ros::Time().fromSec(timeLaserCloudCornerLast);
            laserCloudSurround3_corner.header.frame_id = "/camera_init";
            pubLaserCloudSurround_corner.publish(laserCloudSurround3_corner);
            
            laserCloudFullRes2->clear();
            // *laserCloudFullRes2 = *laserCloudFullRes;
            *laserCloudFullRes2 = *laserCloudSurf_down;

            int laserCloudFullResNum = laserCloudFullRes2->points.size();

            pcl::PointXYZRGB temp_point;
            // std::cout<<"Points number of NewFrameAddedtoMap: "<<laserCloudFullResNum<<std::endl;

            for (int i = 0; i < laserCloudFullResNum; i++)
            {
                RGBpointAssociateToMap(&laserCloudFullRes2->points[i], &temp_point);
                laserCloudFullResColor->push_back(temp_point);
            }

            sensor_msgs::PointCloud2 laserCloudFullRes3;
            pcl::toROSMsg(*laserCloudFullResColor, laserCloudFullRes3);
            laserCloudFullRes3.header.stamp = ros::Time().fromSec(timeLaserCloudCornerLast);
            laserCloudFullRes3.header.frame_id = "/camera_init";
            pubLaserCloudFullRes.publish(laserCloudFullRes3);

            // sensor_msgs::PointCloud2 laserCloudMap;
            // pcl::toROSMsg(*laserCloudSurfFromMap, laserCloudMap);
            // laserCloudMap.header.stamp = ros::Time().fromSec(timeLaserCloudCornerLast);
            // laserCloudMap.header.frame_id = "/camera_init";
            // pubLaserCloudMap.publish(laserCloudMap);

            *laserCloudFullResColor_pcd += *laserCloudFullResColor;

            geometry_msgs::Quaternion geoQuat = tf::createQuaternionMsgFromRollPitchYaw
                    (transformAftMapped[2], - transformAftMapped[0], - transformAftMapped[1]);

            odomAftMapped.header.stamp = ros::Time().fromSec(timeLaserCloudCornerLast);
            odomAftMapped.pose.pose.orientation.x = -geoQuat.y;
            odomAftMapped.pose.pose.orientation.y = -geoQuat.z;
            odomAftMapped.pose.pose.orientation.z = geoQuat.x;
            odomAftMapped.pose.pose.orientation.w = geoQuat.w;
            odomAftMapped.pose.pose.position.x = transformAftMapped[3];
            odomAftMapped.pose.pose.position.y = transformAftMapped[4];
            odomAftMapped.pose.pose.position.z = transformAftMapped[5];

            pubOdomAftMapped.publish(odomAftMapped);

            static tf::TransformBroadcaster br;
            tf::Transform                   transform;
            tf::Quaternion                  q;
            transform.setOrigin( tf::Vector3( odomAftMapped.pose.pose.position.x,
                                                odomAftMapped.pose.pose.position.y,
                                                odomAftMapped.pose.pose.position.z ) );
            q.setW( odomAftMapped.pose.pose.orientation.w );
            q.setX( odomAftMapped.pose.pose.orientation.x );
            q.setY( odomAftMapped.pose.pose.orientation.y );
            q.setZ( odomAftMapped.pose.pose.orientation.z );
            transform.setRotation( q );
            br.sendTransform( tf::StampedTransform( transform, odomAftMapped.header.stamp, "/camera_init", "/aft_mapped" ) );

            kfNum++;

            if(kfNum >= NUM_MAX_ITERATIONS)
            {
                // std::cout<<"iteration count:"<<kfNum<<std::endl;
                Eigen::Matrix<float,7,1> kf_pose;
                kf_pose << -geoQuat.y,-geoQuat.z,geoQuat.x,geoQuat.w,transformAftMapped[3],transformAftMapped[4],transformAftMapped[5];
                keyframe_pose.push_back(kf_pose);
                kfNum = 0;
            }

            /*** plot variables ***/
            t4 = omp_get_wtime();
            T1.push_back(timeLaserCloudSurfLast);
            s_plot.push_back(omp_get_wtime() - t1);
            s_plot2.push_back(double(deltaR));
            s_plot3.push_back(double(deltaT));

            std::cout<<"mapping time : selection "<<t2-t1 <<" match time: "<<match_time<<"  solve time: "<<solve_time<<" pub time: "<<t4 - t3<<" total: "<<t4-t1<<std::endl;
            // std::cout<<"match time: "<<match_time<<"  solve time: "<<solve_time<<std::endl;
        }
        status = ros::ok();
        rate.sleep();
    }
    //--------------------------save map---------------
    std::string surf_filename(map_file_path + "/surf.pcd");
    std::string corner_filename(map_file_path + "/corner.pcd");
    std::string all_points_filename(map_file_path + "/all_points.pcd");
    std::ofstream keyframe_file(map_file_path + "/key_frame.txt");
    for(auto kf : keyframe_pose){
        keyframe_file << kf[0] << " "<< kf[1] << " "<< kf[2] << " "<< kf[3] << " "
                          << kf[4] << " "<< kf[5] << " "<< kf[6] << " "<< std::endl;
    }
    keyframe_file.close();
    pcl::PointCloud<pcl::PointXYZI> surf_points, corner_points;
    surf_points = *laserCloudSurfFromMap;
    corner_points = *laserCloudCornerFromMap;
    if (surf_points.size() > 0 && corner_points.size() > 0) 
    {
    pcl::PCDWriter pcd_writer;
    std::cout << "saving...";
    pcd_writer.writeBinary(surf_filename, surf_points);
    pcd_writer.writeBinary(corner_filename, corner_points);
    pcd_writer.writeBinary(all_points_filename, *laserCloudFullResColor_pcd);
    }
    else
    {
        if (!T1.empty())
        {
            plt::named_plot("time consumed",T1,s_plot);
            plt::named_plot("R_residual",T1,s_plot2);
            plt::named_plot("T_residual",T1,s_plot3);
            plt::legend();
            plt::show();
            plt::pause(0.5);
        }
        std::cout << "no points saved";
    }
    //--------------------------
    //  loss_output.close();
  return 0;
}
