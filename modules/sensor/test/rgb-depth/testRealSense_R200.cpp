/****************************************************************************
 *
 * This file is part of the ViSP software.
 * Copyright (C) 2005 - 2017 by Inria. All rights reserved.
 *
 * This software is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * ("GPL") version 2 as published by the Free Software Foundation.
 * See the file LICENSE.txt at the root directory of this source
 * distribution for additional information about the GNU GPL.
 *
 * For using ViSP with software that can not be combined with the GNU
 * GPL, please contact Inria about acquiring a ViSP Professional
 * Edition License.
 *
 * See http://visp.inria.fr for more information.
 *
 * This software was developed at:
 * Inria Rennes - Bretagne Atlantique
 * Campus Universitaire de Beaulieu
 * 35042 Rennes Cedex
 * France
 *
 * If you have questions regarding the use of this file, please contact
 * Inria at visp@inria.fr
 *
 * This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
 * WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Description:
 * Test Intel RealSense acquisition.
 *
 *****************************************************************************/

/*!
  \example testRealSense_R200.cpp
  Test Intel RealSense R200 acquisition.
*/

#include <iostream>

#include <visp3/sensor/vpRealSense.h>
#include <visp3/gui/vpDisplayX.h>
#include <visp3/gui/vpDisplayGDI.h>
#include <visp3/io/vpImageIo.h>
#include <visp3/core/vpImageConvert.h>

#if defined(VISP_HAVE_REALSENSE) && defined(VISP_HAVE_CPP11_COMPATIBILITY) && ( defined(VISP_HAVE_X11) || defined(VISP_HAVE_GDI) )
#  include <thread>
#  include <atomic>

#ifdef VISP_HAVE_PCL
#  include <pcl/visualization/cloud_viewer.h>
#  include <pcl/visualization/pcl_visualizer.h>
#endif

namespace {

#ifdef VISP_HAVE_PCL
  //Global variables
  pcl::PointCloud<pcl::PointXYZ>::Ptr pointcloud(new pcl::PointCloud<pcl::PointXYZ>());
  pcl::PointCloud<pcl::PointXYZRGB>::Ptr pointcloud_color(new pcl::PointCloud<pcl::PointXYZRGB>());
  std::atomic<bool> cancelled(false), update_pointcloud(false);

  class ViewerWorker {
  public:
    explicit ViewerWorker(const bool color_mode) :
      m_colorMode(color_mode) { }

    bool local_update = false, local_cancelled = false;
    void run() {
      std::string date = vpTime::getDateTime();
      pcl::visualization::PCLVisualizer::Ptr viewer (new pcl::visualization::PCLVisualizer ("3D Viewer " + date));
      pcl::visualization::PointCloudColorHandlerRGBField<pcl::PointXYZRGB> rgb(pointcloud_color);

      viewer->setBackgroundColor (0, 0, 0);
      viewer->initCameraParameters ();
      viewer->setPosition(640+80, 480+80);
      viewer->setCameraPosition(0, 0, -0.25, 0, -1, 0);
      viewer->setSize(640, 480);

      bool init = true;
      while (!local_cancelled) {
        local_update = update_pointcloud;
        update_pointcloud = false;
        local_cancelled = cancelled;

        if (local_update && !local_cancelled) {
          if (init) {
            if (m_colorMode) {
              viewer->addPointCloud<pcl::PointXYZRGB> (pointcloud_color, rgb, "RGB sample cloud");
              viewer->setPointCloudRenderingProperties (pcl::visualization::PCL_VISUALIZER_POINT_SIZE, 1, "RGB sample cloud");
            } else {
              viewer->addPointCloud<pcl::PointXYZ>(pointcloud, "sample cloud");
              viewer->setPointCloudRenderingProperties (pcl::visualization::PCL_VISUALIZER_POINT_SIZE, 1, "sample cloud");
            }
            init = false;
          } else {
            if (m_colorMode) {
              viewer->updatePointCloud<pcl::PointXYZRGB>(pointcloud_color, rgb, "RGB sample cloud");
            } else {
              viewer->updatePointCloud<pcl::PointXYZ>(pointcloud, "sample cloud");
            }
          }
        }

        viewer->spinOnce (10);
      }

      std::cout << "End of point cloud display thread" << std::endl;
    }

  private:
    bool m_colorMode;
  };
#endif //#ifdef VISP_HAVE_PCL

  void test_R200(vpRealSense &rs, const std::map<rs::stream, bool> &enables, const std::map<rs::stream, vpRealSense::vpRsStreamParams> &params,
                 const std::map<rs::option, double> &options, const std::string &title, const bool depth_color_visualization=false,
                 const rs::stream &color_stream=rs::stream::color, const rs::stream &depth_stream=rs::stream::depth,
                 const rs::stream &infrared2_stream=rs::stream::infrared2, bool display_pcl=false, bool pcl_color=false) {
    std::cout << std::endl;

    std::map<rs::stream, bool>::const_iterator it_enable;
    std::map<rs::stream, vpRealSense::vpRsStreamParams>::const_iterator it_param;

    for (it_enable = enables.begin(), it_param = params.begin(); it_enable != enables.end(); ++it_enable) {
      rs.setEnableStream(it_enable->first, it_enable->second);

      if (it_enable->second) {
        it_param = params.find(it_enable->first);

        if (it_param != params.end()) {
          rs.setStreamSettings(it_param->first, it_param->second);
        }
      }
    }

    rs.open();

    vpImage<uint16_t> depth;
    vpImage<unsigned char> I_depth;
    vpImage<vpRGBa> I_depth_color;

    vpImage<vpRGBa> I_color;
    vpImage<uint16_t> infrared, infrared2;
    vpImage<unsigned char> I_infrared, I_infrared2;

    bool direct_infrared_conversion = false;
    for (it_enable = enables.begin(); it_enable != enables.end(); ++it_enable) {
      if (it_enable->second) {
        switch (it_enable->first) {
          case rs::stream::color:
            I_color.init( (unsigned int) rs.getIntrinsics(it_enable->first).height, (unsigned int) rs.getIntrinsics(it_enable->first).width );
            break;

          case rs::stream::depth:
            depth.init( (unsigned int) rs.getIntrinsics(it_enable->first).height, (unsigned int) rs.getIntrinsics(it_enable->first).width );
            I_depth.init( depth.getHeight(), depth.getWidth() );
            I_depth_color.init( depth.getHeight(), depth.getWidth() );
            break;

          case rs::stream::infrared:
            infrared.init( (unsigned int) rs.getIntrinsics(it_enable->first).height, (unsigned int) rs.getIntrinsics(it_enable->first).width );
            I_infrared.init( infrared.getHeight(), infrared.getWidth() );

            it_param = params.find(it_enable->first);
            if (it_param != params.end()) {
              direct_infrared_conversion = it_param->second.m_streamFormat == rs::format::y8;
            }
            break;

          case rs::stream::infrared2:
            infrared2.init( (unsigned int) rs.getIntrinsics(it_enable->first).height, (unsigned int) rs.getIntrinsics(it_enable->first).width );
            I_infrared2.init( infrared.getHeight(), infrared.getWidth() );

            it_param = params.find(it_enable->first);
            if (it_param != params.end()) {
              direct_infrared_conversion = it_param->second.m_streamFormat == rs::format::y8;
            }
            break;

          default:
            break;
        }
      }
    }

  #if defined(VISP_HAVE_X11)
    vpDisplayX dc, dd, di, di2;
  #elif defined(VISP_HAVE_GDI)
    vpDisplayGDI dc, dd, di, di2;
  #endif

    for (it_enable = enables.begin(); it_enable != enables.end(); ++it_enable) {
      if (it_enable->second) {
        switch (it_enable->first) {
          case rs::stream::color:
            dc.init( I_color, 0, 0, "Color frame" );
            break;

          case rs::stream::depth:
            if (depth_color_visualization) {
              dd.init( I_depth_color, (int) I_color.getWidth() + 80, 0, "Depth frame" );
            } else {
              dd.init( I_depth, (int) I_color.getWidth() + 80, 0, "Depth frame" );
            }
            break;

          case rs::stream::infrared:
            di.init( I_infrared, 0, (int) std::max(I_color.getHeight(), I_depth.getHeight()) + 30, "Infrared frame" );
            break;

          case rs::stream::infrared2:
            di2.init( I_infrared2, (int) I_infrared.getWidth(), (int) std::max(I_color.getHeight(), I_depth.getHeight()) + 30, "Infrared2 frame" );
            break;

          default:
            break;
        }
      }
    }

    std::cout << "direct_infrared_conversion=" << direct_infrared_conversion << std::endl;

#ifdef VISP_HAVE_PCL
    ViewerWorker viewer(pcl_color);
    std::thread viewer_thread;

    if (display_pcl) {
      viewer_thread = std::thread(&ViewerWorker::run, &viewer);
    }
#else
    display_pcl = false;
#endif

    rs::device * dev = rs.getHandler();

    //Test stream acquisition during 10 s
    std::vector<double> time_vector;
    double t_begin = vpTime::measureTimeMs();
    while (true) {
      double t = vpTime::measureTimeMs();

      for (std::map<rs::option, double>::const_iterator it = options.begin(); it != options.end(); ++it) {
        dev->set_option(it->first, it->second);
      }

      if (display_pcl) {
#ifdef VISP_HAVE_PCL
        if (direct_infrared_conversion) {
          if (pcl_color) {
            rs.acquire( (unsigned char *) I_color.bitmap, (unsigned char *) depth.bitmap, NULL, pointcloud_color, (unsigned char *) I_infrared.bitmap,
                        (unsigned char *) I_infrared2.bitmap, color_stream, depth_stream, rs::stream::infrared, infrared2_stream );
          } else {
            rs.acquire( (unsigned char *) I_color.bitmap, (unsigned char *) depth.bitmap, NULL, pointcloud, (unsigned char *) I_infrared.bitmap,
                        (unsigned char *) I_infrared2.bitmap, color_stream, depth_stream, rs::stream::infrared, infrared2_stream );
          }
        } else {
          if (pcl_color) {
            rs.acquire( (unsigned char *) I_color.bitmap, (unsigned char *) depth.bitmap, NULL, pointcloud_color, (unsigned char *) infrared.bitmap,
                        (unsigned char *) infrared2.bitmap, color_stream, depth_stream, rs::stream::infrared, infrared2_stream );
          } else {
            rs.acquire( (unsigned char *) I_color.bitmap, (unsigned char *) depth.bitmap, NULL, pointcloud, (unsigned char *) infrared.bitmap,
                        (unsigned char *) infrared2.bitmap, color_stream, depth_stream, rs::stream::infrared, infrared2_stream );
          }

          vpImageConvert::convert(infrared, I_infrared);
          vpImageConvert::convert(infrared2, I_infrared2);
        }

        update_pointcloud = true; //atomic variable
#endif
      } else {
        if (direct_infrared_conversion) {
          rs.acquire( (unsigned char *) I_color.bitmap, (unsigned char *) depth.bitmap, NULL, (unsigned char *) I_infrared.bitmap, (unsigned char *) I_infrared2.bitmap,
                      color_stream, depth_stream, rs::stream::infrared, infrared2_stream );
        } else {
          rs.acquire( (unsigned char *) I_color.bitmap, (unsigned char *) depth.bitmap, NULL, (unsigned char *) infrared.bitmap, (unsigned char *) infrared2.bitmap,
                      color_stream, depth_stream, rs::stream::infrared, infrared2_stream );
          vpImageConvert::convert(infrared, I_infrared);
          vpImageConvert::convert(infrared2, I_infrared2);
        }
      }

      if (depth_color_visualization) {
        vpImageConvert::createDepthHistogram(depth, I_depth_color);
      } else {
        vpImageConvert::convert(depth, I_depth);
      }

      vpDisplay::display(I_color);
      if (depth_color_visualization) {
        vpDisplay::display(I_depth_color);
      } else {
        vpDisplay::display(I_depth);
      }
      vpDisplay::display(I_infrared);
      vpDisplay::display(I_infrared2);

      vpDisplay::flush(I_color);
      if (depth_color_visualization) {
        vpDisplay::flush(I_depth_color);
      } else {
        vpDisplay::flush(I_depth);
      }
      vpDisplay::flush(I_infrared);
      vpDisplay::flush(I_infrared2);

      if ( vpDisplay::getClick(I_color, false) || (depth_color_visualization ? vpDisplay::getClick(I_depth_color, false) : vpDisplay::getClick(I_depth, false) ) ||
           vpDisplay::getClick(I_infrared, false) || vpDisplay::getClick(I_infrared2, false) ) {
        break;
      }

      time_vector.push_back(vpTime::measureTimeMs() - t);
      if (vpTime::measureTimeMs() - t_begin >= 10000) {
        break;
      }
    }

    if (display_pcl) {
#ifdef VISP_HAVE_PCL
      cancelled = true; //atomic variable

      viewer_thread.join();
#endif
    }

    std::cout << title << " - Mean time: "
              << vpMath::getMean(time_vector) << " ms ; Median time: "
              << vpMath::getMedian(time_vector) << " ms" << std::endl;

    rs.close();
  }

} //namespace

int main(int argc, char *argv[]) {
  try {
    vpRealSense rs;

    rs.setEnableStream(rs::stream::color, false);
    rs.open();
    if ( rs_get_device_name((const rs_device *) rs.getHandler(), nullptr) != std::string("Intel RealSense R200") ) {
      std::cout << "This test file is used to test the Intel RealSense R200 only." << std::endl;
      return EXIT_SUCCESS;
    }

    std::cout << "API version: " << rs_get_api_version(nullptr) << std::endl;
    std::cout << "Firmware: " << rs_get_device_firmware_version((const rs_device *) rs.getHandler(), nullptr) << std::endl;
    std::cout << "RealSense sensor characteristics: \n" << rs << std::endl;

    rs.close();

    std::map<rs::stream, bool> enables;
    std::map<rs::stream, vpRealSense::vpRsStreamParams> params;
    std::map<rs::option, double> options;

    enables[rs::stream::color] = false;
    enables[rs::stream::depth] = true;
    enables[rs::stream::infrared] = false;
    enables[rs::stream::infrared2] = false;

    params[rs::stream::depth] = vpRealSense::vpRsStreamParams(640, 480, rs::format::z16, 90);

    options[rs::option::r200_lr_auto_exposure_enabled] = 1;

    test_R200(rs, enables, params, options, "R200_DEPTH_Z16_640x480_90FPS + r200_lr_auto_exposure_enabled", true);


    enables[rs::stream::color] = false;
    enables[rs::stream::depth] = true;
    enables[rs::stream::infrared] = true;
    enables[rs::stream::infrared2] = true;

    params[rs::stream::depth] = vpRealSense::vpRsStreamParams(640, 480, rs::format::z16, 90);
    params[rs::stream::infrared] = vpRealSense::vpRsStreamParams(640, 480, rs::format::y8, 90);
    params[rs::stream::infrared2] = vpRealSense::vpRsStreamParams(640, 480, rs::format::y8, 90);

    options[rs::option::r200_lr_auto_exposure_enabled] = 0;
    options[rs::option::r200_emitter_enabled] = 0;

    test_R200(rs, enables, params, options, "R200_DEPTH_Z16_640x480_90FPS + R200_INFRARED_Y8_640x480_90FPS + R200_INFRARED2_Y8_640x480_90FPS + "
                                            "!r200_lr_auto_exposure_enabled + !r200_emitter_enabled", true);


    enables[rs::stream::color] = false;
    enables[rs::stream::depth] = true;
    enables[rs::stream::infrared] = true;
    enables[rs::stream::infrared2] = true;

    params[rs::stream::depth] = vpRealSense::vpRsStreamParams(628, 468, rs::format::z16, 90);
    params[rs::stream::infrared] = vpRealSense::vpRsStreamParams(640, 480, rs::format::y16, 90);
    params[rs::stream::infrared2] = vpRealSense::vpRsStreamParams(640, 480, rs::format::y16, 90);

    options[rs::option::r200_lr_auto_exposure_enabled] = 1;
    options[rs::option::r200_emitter_enabled] = 1;

    test_R200(rs, enables, params, options, "R200_DEPTH_Z16_628x468_90FPS + R200_INFRARED_Y16_640x480_90FPS + R200_INFRARED2_Y16_640x480_90FPS");


    enables[rs::stream::color] = false;
    enables[rs::stream::depth] = true;
    enables[rs::stream::infrared] = true;
    enables[rs::stream::infrared2] = true;

    params[rs::stream::depth] = vpRealSense::vpRsStreamParams(628, 468, rs::format::z16, 90);
    params[rs::stream::infrared] = vpRealSense::vpRsStreamParams(640, 480, rs::format::y8, 90);
    params[rs::stream::infrared2] = vpRealSense::vpRsStreamParams(640, 480, rs::format::y8, 90);

    options.clear();

    test_R200(rs, enables, params, options, "R200_DEPTH_Z16_628x468_90FPS + R200_INFRARED_Y8_640x480_90FPS + R200_INFRARED2_Y8_640x480_90FPS");


    enables[rs::stream::color] = true;
    enables[rs::stream::depth] = true;
    enables[rs::stream::infrared] = true;
    enables[rs::stream::infrared2] = true;

    params[rs::stream::color] = vpRealSense::vpRsStreamParams(640, 480, rs::format::rgba8, 30);
    params[rs::stream::depth] = vpRealSense::vpRsStreamParams(640, 480, rs::format::z16, 90);
    params[rs::stream::infrared] = vpRealSense::vpRsStreamParams(640, 480, rs::format::y8, 90);
    params[rs::stream::infrared2] = vpRealSense::vpRsStreamParams(640, 480, rs::format::y8, 90);

    test_R200(rs, enables, params, options, "R200_COLOR_RGBA8_640x480_30FPS + R200_DEPTH_Z16_628x468_90FPS + R200_INFRARED_Y8_640x480_90FPS + R200_INFRARED2_Y8_640x480_90FPS", true);


    enables[rs::stream::color] = true;
    enables[rs::stream::depth] = false;
    enables[rs::stream::infrared] = false;
    enables[rs::stream::infrared2] = false;

    params[rs::stream::color] = vpRealSense::vpRsStreamParams(1920, 1080, rs::format::rgba8, 30);

    test_R200(rs, enables, params, options, "R200_COLOR_RGBA8_1920x1080_30FPS");


    enables[rs::stream::color] = true;
    enables[rs::stream::depth] = false;
    enables[rs::stream::infrared] = false;
    enables[rs::stream::infrared2] = false;

    params[rs::stream::color] = vpRealSense::vpRsStreamParams(640, 480, rs::format::rgba8, 60);
    test_R200(rs, enables, params, options, "R200_COLOR_RGBA8_640x480_60FPS");


    enables[rs::stream::color] = true;
    enables[rs::stream::depth] = true;
    enables[rs::stream::infrared] = true;
    enables[rs::stream::infrared2] = true;

    params[rs::stream::color] = vpRealSense::vpRsStreamParams(640, 480, rs::format::rgba8, 60);
    params[rs::stream::depth] = vpRealSense::vpRsStreamParams(640, 480, rs::format::z16, 60);
    params[rs::stream::infrared] = vpRealSense::vpRsStreamParams(640, 480, rs::format::y8, 60);
    params[rs::stream::infrared2] = vpRealSense::vpRsStreamParams(640, 480, rs::format::y8, 60);

    //depth == 0 ; color == 1 ; infrared2 == 3 ; rectified_color == 6 ; color_aligned_to_depth == 7 ; infrared2_aligned_to_depth == 8 ;
    //depth_aligned_to_color == 9 ; depth_aligned_to_rectified_color == 10 ; depth_aligned_to_infrared2 == 11
    //argv[2] <==> color stream
    rs::stream color_stream = argc > 2 ? (rs::stream) atoi(argv[2]) : rs::stream::color_aligned_to_depth;
    std::cout << "\ncolor_stream: " << color_stream << std::endl;
    //argv[3] <==> depth stream
    rs::stream depth_stream = argc > 3 ? (rs::stream) atoi(argv[3]) : rs::stream::depth_aligned_to_rectified_color;
    std::cout << "depth_stream: " << depth_stream << std::endl;
    //argv[4] <==> depth stream
    rs::stream infrared2_stream = argc > 4 ? (rs::stream) atoi(argv[4]) : rs::stream::infrared2_aligned_to_depth;
    std::cout << "infrared2_stream: " << infrared2_stream << std::endl;

    test_R200(rs, enables, params, options, "R200_COLOR_ALIGNED_TO_DEPTH_RGBA8_640x480_60FPS + R200_DEPTH_ALIGNED_TO_RECTIFIED_COLOR_Z16_640x480_60FPS + "
                                   "R200_INFRARED_Y8_640x480_60FPS + R200_INFRARED2_ALIGNED_TO_DEPTH_Y8_640x480_60FPS",
              true, color_stream, depth_stream, infrared2_stream);


    enables[rs::stream::color] = true;
    enables[rs::stream::depth] = true;
    enables[rs::stream::infrared] = true;
    enables[rs::stream::infrared2] = true;

    params[rs::stream::color] = vpRealSense::vpRsStreamParams(640, 480, rs::format::rgba8, 60);
    params[rs::stream::depth] = vpRealSense::vpRsStreamParams(640, 480, rs::format::z16, 60);
    params[rs::stream::infrared] = vpRealSense::vpRsStreamParams(640, 480, rs::format::y8, 60);
    params[rs::stream::infrared2] = vpRealSense::vpRsStreamParams(640, 480, rs::format::y8, 60);

    //Cannot render two pcl::visualization::PCLVisualizer so use an arg option to switch between B&W and color point cloud rendering until a solution is found
    test_R200(rs, enables, params, options, "R200_COLOR_RGBA8_640x480_60FPS + R200_DEPTH_Z16_640x480_60FPS + R200_INFRARED_Y8_640x480_60FPS + R200_INFRARED2_Y8_640x480_60FPS",
              false, rs::stream::color, rs::stream::depth, rs::stream::infrared2, true, (argc > 1 ? argv[1] : false));

  } catch(const vpException &e) {
    std::cerr << "RealSense error " << e.what() << std::endl;
  } catch(const rs::error & e)  {
    std::cerr << "RealSense error calling " << e.get_failed_function() << "(" << e.get_failed_args() << "): " << e.what() << std::endl;
  } catch(const std::exception & e) {
    std::cerr << e.what() << std::endl;
  }

  return EXIT_SUCCESS;
}

#else
int main() {
#if !defined(VISP_HAVE_REALSENSE)
  std::cout << "Install RealSense SDK to make this test working. X11 or GDI are needed also." << std::endl;
#elif !defined(VISP_HAVE_CPP11_COMPATIBILITY)
  std::cout << "Build ViSP with c++11 compiler flag (cmake -DUSE_CPP11=ON) to make this test working" << std::endl;
#elif !defined(VISP_HAVE_X11) && !defined(VISP_HAVE_GDI)
  std::cout << "X11 or GDI are needed!" << std::endl;
#endif
  return EXIT_SUCCESS;
}
#endif