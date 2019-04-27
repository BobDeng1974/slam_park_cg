#include <iostream>
#include <fstream>
#include <list>
#include <vector>
#include <chrono>
#include <ctime>
#include <climits>

#include "pose_estimation.h"

#include "tum_data_rgbd.h"

using namespace std;
using namespace cv;

int main ( int argc, char** argv )
{
    cg::TUMDataRGBD tum_data_rgbd("/home/cg/dev_sdb/datasets/TUM/RGBD-SLAM-Dataset/rgbd_dataset_freiburg2_desk/", 1);

    vector<cv::Mat> colorImgs, depthImgs;
    const int count = 10;
    for(int i=0; i<count; ++i) {
        cv::Mat img_color;
        cv::Mat img_depth;
        if (!tum_data_rgbd.get_rgb_depth(img_color, img_depth)) {
            std::cerr << "get_rgb_depth failed!" << std::endl;
            return -1;
        }
        colorImgs.push_back(img_color);
        depthImgs.push_back(img_depth);
    }

    float depth_scale = tum_data_rgbd.depth_scale_;

    Eigen::Matrix3f K;
    tum_data_rgbd.getK(K);

    Eigen::Isometry3d Tcw = Eigen::Isometry3d::Identity();

    vector<Measurement> measurements;
    cv::Mat prev_color, color, depth;
    for ( int index=0; index<count; index++ )
    {
        cout<<"*********** loop "<<index<<" ************"<<endl;
        color = colorImgs[index];
        depth = depthImgs[index];
        if ( color.data==nullptr || depth.data==nullptr )
            continue;
        cv::Mat gray;
        cv::cvtColor ( color, gray, cv::COLOR_BGR2GRAY );
        if ( index ==0 )
        {
            // 对第一帧提取FAST特征点
            vector<cv::KeyPoint> keypoints;
            cv::Ptr<cv::FastFeatureDetector> detector = cv::FastFeatureDetector::create();
            detector->detect ( color, keypoints );
            for ( auto kp:keypoints )
            {
                // 去掉邻近边缘处的点
                if ( kp.pt.x < 20 || kp.pt.y < 20 || ( kp.pt.x+20 ) >color.cols || ( kp.pt.y+20 ) >color.rows )
                    continue;
                ushort d = depth.ptr<ushort> ( cvRound ( kp.pt.y ) ) [ cvRound ( kp.pt.x ) ];
                if ( d==0 )
                    continue;
                Eigen::Vector3d p3d = project2Dto3D ( kp.pt.x, kp.pt.y, d, K, depth_scale );
                float grayscale = float ( gray.ptr<uchar> ( cvRound ( kp.pt.y ) ) [ cvRound ( kp.pt.x ) ] );
                measurements.push_back ( Measurement ( p3d, grayscale ) );
            }
            prev_color = color.clone();
            continue;
        }

        // 使用直接法计算相机运动
        chrono::steady_clock::time_point t1 = chrono::steady_clock::now();
        pose_estimation_direct ( measurements, &gray, K, Tcw );
        chrono::steady_clock::time_point t2 = chrono::steady_clock::now();
        chrono::duration<double> time_used = chrono::duration_cast<chrono::duration<double>> ( t2-t1 );
        cout<<"direct method costs time: "<<time_used.count() <<" seconds."<<endl;
        cout<<"Tcw="<<Tcw.matrix() <<endl;

        // plot the feature points
        cv::Mat img_show ( color.rows*2, color.cols, CV_8UC3 );
        prev_color.copyTo ( img_show ( cv::Rect ( 0,0,color.cols, color.rows ) ) );
        color.copyTo ( img_show ( cv::Rect ( 0,color.rows,color.cols, color.rows ) ) );
        for ( Measurement m:measurements )
        {
            if ( rand() > RAND_MAX/5 )
                continue;
            Eigen::Vector3d p = m.pos_world;
            Eigen::Vector2d pixel_prev = project3Dto2D ( p ( 0,0 ), p ( 1,0 ), p ( 2,0 ), K);
            Eigen::Vector3d p2 = Tcw*m.pos_world;
            Eigen::Vector2d pixel_now = project3Dto2D ( p2 ( 0,0 ), p2 ( 1,0 ), p2 ( 2,0 ), K);
            if ( pixel_now(0,0)<0 || pixel_now(0,0)>=color.cols || pixel_now(1,0)<0 || pixel_now(1,0)>=color.rows )
                continue;

            float b = 255*float ( rand() ) /RAND_MAX;
            float g = 255*float ( rand() ) /RAND_MAX;
            float r = 255*float ( rand() ) /RAND_MAX;
            cv::circle ( img_show, cv::Point2d ( pixel_prev ( 0,0 ), pixel_prev ( 1,0 ) ), 8, cv::Scalar ( b,g,r ), 2 );
            cv::circle ( img_show, cv::Point2d ( pixel_now ( 0,0 ), pixel_now ( 1,0 ) +color.rows ), 8, cv::Scalar ( b,g,r ), 2 );
            cv::line ( img_show, cv::Point2d ( pixel_prev ( 0,0 ), pixel_prev ( 1,0 ) ), cv::Point2d ( pixel_now ( 0,0 ), pixel_now ( 1,0 ) +color.rows ), cv::Scalar ( b,g,r ), 1 );
        }
        cv::imshow ( "result", img_show );
        cv::waitKey ( 0 );
    }
    return 0;
}
