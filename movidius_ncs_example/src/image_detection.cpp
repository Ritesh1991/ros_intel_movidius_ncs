/*
 * Copyright (c) 2017 Intel Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <dirent.h>
#include <random>
#include <string>
#include <vector>

#include <boost/date_time/posix_time/posix_time.hpp>
#include <cv_bridge/cv_bridge.h>
#include <object_msgs/DetectObject.h>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <ros/ros.h>

#define MOVEWINDOW 1000
#define DEFAULT_PARALLEL_SIZE 1
#define DEFAULT_IMAGE_BASE_PATH "/opt/movidius/ncappzoo/data/images/"
#define DEFAULT_DEMO_MODE 0
#define DEFAULT_PARALLEL_FLAG 1

std::vector<std::string> getImagePath(std::string image_dir)
{
  if (image_dir.back() != '/')
  {
    image_dir += "/";
  }

  std::vector<std::string> files;

  DIR* dir;
  struct dirent* ptr;

  if ((dir = opendir(image_dir.c_str())) == NULL)
  {
    std::cerr << "Open Dir error..." << std::endl;
    exit(1);
  }

  while ((ptr = readdir(dir)) != NULL)
  {
    if (strcmp(ptr->d_name, ".") == 0 || strcmp(ptr->d_name, "..") == 0)
      continue;
    else if (ptr->d_type == DT_REG)
      files.push_back(image_dir + ptr->d_name);
  }
  closedir(dir);

  return files;
}

int main(int argc, char** argv)
{
  ros::init(argc, argv, "movidius_ncs_example_detection");
  ros::NodeHandle n("~");
  ros::ServiceClient client;

  std::string image_base_path = DEFAULT_IMAGE_BASE_PATH;
  if (!n.getParam("image_base_path", image_base_path))
  {
    ROS_WARN("param image_base_path not set, use default");
  }
  ROS_INFO_STREAM("use image_base_path = " << image_base_path);
  std::vector<std::string> image_paths = getImagePath(image_base_path);

  int parallel_size = DEFAULT_PARALLEL_SIZE;
  if (!n.getParam("parallel_size", parallel_size))
  {
    ROS_WARN("param parallel_size not set, use default");
  }
  ROS_INFO_STREAM("use parallel_size = " << parallel_size);

  int demo_mode = DEFAULT_DEMO_MODE;
  if (!n.getParam("demo_mode", demo_mode))
  {
    ROS_WARN("param demo_mode not set, use default");
  }
  ROS_INFO_STREAM("use demo_mode = " << demo_mode);

  int parallel_flag = DEFAULT_PARALLEL_FLAG;
  if (!n.getParam("parallel_flag", parallel_flag))
  {
    ROS_WARN("param parallel_flag not set, use default");
  }
  ROS_INFO_STREAM("use parallel_flag = " << parallel_flag);

  if (parallel_flag == 0)
  {
    client = n.serviceClient<object_msgs::DetectObject>("/movidius_ncs_image_single/detect_object");
  }
  else
  {
    client = n.serviceClient<object_msgs::DetectObject>("/movidius_ncs_image_multiple/detect_object");
  }

  if (demo_mode == 0)
  {
    object_msgs::DetectObject srv;
    srv.request.image_paths = image_paths;
    boost::posix_time::ptime start = boost::posix_time::microsec_clock::local_time();
    if (!client.call(srv))
    {
      ROS_ERROR("failed to call service DetectObject");
      return 1;
    }
    boost::posix_time::ptime end = boost::posix_time::microsec_clock::local_time();
    boost::posix_time::time_duration msdiff = end - start;

    for (unsigned int i = 0; i < srv.response.objects.size(); i++)
    {
      cv_bridge::CvImage cv_image;
      cv_image.image = cv::imread(image_paths[i]);
      cv_image.encoding = "bgr8";
      int width = cv_image.image.cols;
      int height = cv_image.image.rows;

      ROS_INFO("Detection result for image No.%u:", i + 1);
      for (unsigned int j = 0; j < srv.response.objects[i].objects_vector.size(); j++)
      {
        std::stringstream ss;
        ss << srv.response.objects[i].objects_vector[j].object.object_name << ": "
           << srv.response.objects[i].objects_vector[j].object.probability * 100 << "%";

        ROS_INFO("%d: object: %s", j, srv.response.objects[i].objects_vector[j].object.object_name.c_str());
        ROS_INFO("prob: %f", srv.response.objects[i].objects_vector[j].object.probability);
        ROS_INFO("location: (%d, %d, %d, %d)", srv.response.objects[i].objects_vector[j].roi.x_offset,
                 srv.response.objects[i].objects_vector[j].roi.y_offset,
                 srv.response.objects[i].objects_vector[j].roi.width,
                 srv.response.objects[i].objects_vector[j].roi.height);

        int xmin = srv.response.objects[i].objects_vector[j].roi.x_offset;
        int ymin = srv.response.objects[i].objects_vector[j].roi.y_offset;
        int w = srv.response.objects[i].objects_vector[j].roi.width;
        int h = srv.response.objects[i].objects_vector[j].roi.height;

        int xmax = ((xmin + w) < width) ? (xmin + w) : width;
        int ymax = ((ymin + h) < height) ? (ymin + h) : height;

        cv::Point left_top = cv::Point(xmin, ymin);
        cv::Point right_bottom = cv::Point(xmax, ymax);
        cv::rectangle(cv_image.image, left_top, right_bottom, cv::Scalar(0, 255, 0), 1, cv::LINE_8, 0);
        cv::rectangle(cv_image.image, cvPoint(xmin, ymin), cvPoint(xmax, ymin + 20), cv::Scalar(0, 255, 0), -1);
        cv::putText(cv_image.image, ss.str(), cvPoint(xmin + 5, ymin + 20), cv::FONT_HERSHEY_PLAIN, 1,
                    cv::Scalar(0, 0, 255), 1);
      }

      cv::imshow("image_detection", cv_image.image);
      cv::waitKey(0);
    }
    ROS_INFO("inference %lu images during %ld ms", srv.response.objects.size(), msdiff.total_milliseconds());
  }
  else
  {
    while (1)
    {
      object_msgs::DetectObject srv;
      std::vector<int> random_index_list;
      for (int i = 0; i < parallel_size; i++)
      {
        std::random_device rd;
        std::default_random_engine engine(rd());
        std::uniform_int_distribution<> dis(0, image_paths.size() - 1);
        auto dice = std::bind(dis, engine);
        int random_index = dice();
        random_index_list.push_back(random_index);
        srv.request.image_paths.push_back(image_paths[random_index]);
      }

      if (!client.call(srv))
      {
        ROS_ERROR("failed to call service DetectObject");
        exit(1);
      }

      for (unsigned int i = 0; i < srv.response.objects.size(); i++)
      {
        cv_bridge::CvImage cv_image;
        cv_image.image = cv::imread(image_paths[random_index_list[i]]);
        cv_image.encoding = "bgr8";
        int width = cv_image.image.cols;
        int height = cv_image.image.rows;

        for (unsigned int j = 0; j < srv.response.objects[i].objects_vector.size(); j++)
        {
          std::stringstream ss;
          ss << srv.response.objects[i].objects_vector[j].object.object_name << ": "
             << srv.response.objects[i].objects_vector[j].object.probability * 100 << "%";

          int xmin = srv.response.objects[i].objects_vector[j].roi.x_offset;
          int ymin = srv.response.objects[i].objects_vector[j].roi.y_offset;
          int w = srv.response.objects[i].objects_vector[j].roi.width;
          int h = srv.response.objects[i].objects_vector[j].roi.height;

          int xmax = ((xmin + w) < width) ? (xmin + w) : width;
          int ymax = ((ymin + h) < height) ? (ymin + h) : height;

          cv::Point left_top = cv::Point(xmin, ymin);
          cv::Point right_bottom = cv::Point(xmax, ymax);
          cv::rectangle(cv_image.image, left_top, right_bottom, cv::Scalar(0, 255, 0), 1, cv::LINE_8, 0);
          cv::rectangle(cv_image.image, cvPoint(xmin, ymin), cvPoint(xmax, ymin + 20), cv::Scalar(0, 255, 0), -1);

          cv::putText(cv_image.image, ss.str(), cvPoint(xmin + 5, ymin + 20), cv::FONT_HERSHEY_PLAIN, 1,
                      cv::Scalar(0, 0, 255), 1);
        }

        if (parallel_flag == 0)
        {
          cv::imshow("image detection with single device", cv_image.image);
          cv::waitKey(1);
        }
        else
        {
          cv::namedWindow("image detection with multiple devices");
          cv::moveWindow("image detection with multiple devices", MOVEWINDOW, 0);
          cv::imshow("image detection with multiple devices", cv_image.image);
          cv::waitKey(1);
        }
      }
    }
  }

  return 0;
}
