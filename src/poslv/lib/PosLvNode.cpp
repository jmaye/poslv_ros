/******************************************************************************
 * Copyright (C) 2013 by Jerome Maye                                          *
 * jerome.maye@gmail.com                                                      *
 *                                                                            *
 * This program is free software; you can redistribute it and/or modify       *
 * it under the terms of the Lesser GNU General Public License as published by*
 * the Free Software Foundation; either version 3 of the License, or          *
 * (at your option) any later version.                                        *
 *                                                                            *
 * This program is distributed in the hope that it will be useful,            *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of             *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the              *
 * Lesser GNU General Public License for more details.                        *
 *                                                                            *
 * You should have received a copy of the Lesser GNU General Public License   *
 * along with this program. If not, see <http://www.gnu.org/licenses/>.       *
 ******************************************************************************/

#include "PosLvNode.h"

#include <diagnostic_updater/publisher.h>

#include <boost/shared_ptr.hpp>

#include <libposlv/types/VehicleNavigationSolution.h>
#include <libposlv/types/VehicleNavigationPerformance.h>
#include <libposlv/types/TimeTaggedDMIData.h>
#include <libposlv/types/PrimaryGPSStatus.h>
#include <libposlv/types/SecondaryGPSStatus.h>
#include <libposlv/com/TCPConnectionClient.h>
#include <libposlv/sensor/POSLVComTCP.h>
#include <libposlv/exceptions/IOException.h>
#include <libposlv/exceptions/SystemException.h>
#include <libposlv/base/Timer.h>
#include <libposlv/types/Packet.h>
#include <libposlv/types/Group.h>

#include "poslv/VehicleNavigationSolutionMsg.h"
#include "poslv/VehicleNavigationPerformanceMsg.h"
#include "poslv/TimeTaggedDMIDataMsg.h"

namespace poslv {

/******************************************************************************/
/* Constructors and Destructor                                                */
/******************************************************************************/

  PosLvNode::PosLvNode(const ros::NodeHandle& nh) :
      _nodeHandle(nh),
      _alignStatus(8),
      _navStatus1(-1),
      _navStatus2(-1) {
    getParameters();
    _vehicleNavigationSolutionPublisher =
      _nodeHandle.advertise<poslv::VehicleNavigationSolutionMsg>(
      "vehicle_navigation_solution", _queueDepth);
    _vehicleNavigationPerformancePublisher =
      _nodeHandle.advertise<poslv::VehicleNavigationPerformanceMsg>(
      "vehicle_navigation_performance", _queueDepth);
    _timeTaggedDMIDataPublisher =
      _nodeHandle.advertise<poslv::TimeTaggedDMIDataMsg>(
      "time_tagged_dmi_data", _queueDepth);
    _updater.setHardwareID("none");
    _updater.add("TCP connection", this, &PosLvNode::diagnoseTCPConnection);
    _updater.add("Alignement status", this, &PosLvNode::diagnoseAlignStatus);
    _updater.add("Navigation status (primary GPS)", this,
      &PosLvNode::diagnoseNavStatusPrimary);
    _updater.add("Navigation status (secondary GPS)", this,
      &PosLvNode::diagnoseNavStatusSecondary);
    _vnsFreq.reset(new diagnostic_updater::HeaderlessTopicDiagnostic(
      "vehicle_navigation_solution", _updater,
      diagnostic_updater::FrequencyStatusParam(&_vnsMinFreq, &_vnsMaxFreq,
      0.1, 10)));
    _vnpFreq.reset(new diagnostic_updater::HeaderlessTopicDiagnostic(
      "vehicle_navigation_performance", _updater,
      diagnostic_updater::FrequencyStatusParam(&_vnpMinFreq, &_vnpMaxFreq,
      0.1, 10)));
    _dmiFreq.reset(new diagnostic_updater::HeaderlessTopicDiagnostic(
      "time_tagged_dmi_data", _updater,
      diagnostic_updater::FrequencyStatusParam(&_dmiMinFreq, &_dmiMaxFreq,
      0.1, 10)));
    _updater.force_update();
  }

  PosLvNode::~PosLvNode() {
  }

/******************************************************************************/
/* Methods                                                                    */
/******************************************************************************/

  void PosLvNode::publishVehicleNavigationSolution(const ros::Time& timestamp,
      const VehicleNavigationSolution& vns) {
    boost::shared_ptr<poslv::VehicleNavigationSolutionMsg> vnsMsg(
      new poslv::VehicleNavigationSolutionMsg);
    vnsMsg->header.stamp = timestamp;
    vnsMsg->header.frame_id = _frameId;
    vnsMsg->TimeDistance.Time1 = vns.mTimeDistance.mTime1;
    vnsMsg->TimeDistance.Time2 = vns.mTimeDistance.mTime2;
    vnsMsg->TimeDistance.DistanceTag = vns.mTimeDistance.mDistanceTag;
    vnsMsg->TimeDistance.TimeType = vns.mTimeDistance.mTimeType;
    vnsMsg->TimeDistance.DistanceType = vns.mTimeDistance.mDistanceType;
    vnsMsg->Latitude = vns.mLatitude;
    vnsMsg->Longitude = vns.mLongitude;
    vnsMsg->Altitude = vns.mAltitude;
    vnsMsg->NorthVelocity = vns.mNorthVelocity;
    vnsMsg->EastVelocity = vns.mEastVelocity;
    vnsMsg->DownVelocity = vns.mDownVelocity;
    vnsMsg->Roll = vns.mRoll;
    vnsMsg->Pitch = vns.mPitch;
    vnsMsg->Heading = vns.mHeading;
    vnsMsg->WanderAngle = vns.mWanderAngle;
    vnsMsg->TrackAngle = vns.mTrackAngle;
    vnsMsg->Speed = vns.mSpeed;
    vnsMsg->AngularRateLong = vns.mAngularRateLong;
    vnsMsg->AngularRateTrans = vns.mAngularRateTrans;
    vnsMsg->AngularRateDown = vns.mAngularRateDown;
    vnsMsg->AccLong = vns.mAccLong;
    vnsMsg->AccTrans = vns.mAccTrans;
    vnsMsg->AccDown = vns.mAccDown;
    vnsMsg->AlignementStatus = vns.mAlignementStatus;
    _vehicleNavigationSolutionPublisher.publish(vnsMsg);
    _alignStatus = vns.mAlignementStatus;
    _vnsFreq->tick();
  }

  void PosLvNode::publishVehicleNavigationPerformance(
      const ros::Time& timestamp, const VehicleNavigationPerformance& vnp) {
    boost::shared_ptr<poslv::VehicleNavigationPerformanceMsg> vnpMsg(
      new poslv::VehicleNavigationPerformanceMsg);
    vnpMsg->header.stamp = timestamp;
    vnpMsg->header.frame_id = _frameId;
    vnpMsg->TimeDistance.Time1 = vnp.mTimeDistance.mTime1;
    vnpMsg->TimeDistance.Time2 = vnp.mTimeDistance.mTime2;
    vnpMsg->TimeDistance.DistanceTag = vnp.mTimeDistance.mDistanceTag;
    vnpMsg->TimeDistance.TimeType = vnp.mTimeDistance.mTimeType;
    vnpMsg->TimeDistance.DistanceType = vnp.mTimeDistance.mDistanceType;
    vnpMsg->NorthPositionRMSError = vnp.mNorthPositionRMSError;
    vnpMsg->EastPositionRMSError = vnp.mEastPositionRMSError;
    vnpMsg->DownPositionRMSError = vnp.mDownPositionRMSError;
    vnpMsg->NorthVelocityRMSError = vnp.mNorthVelocityRMSError;
    vnpMsg->EastVelocityRMSError = vnp.mEastVelocityRMSError;
    vnpMsg->DownVelocityRMSError = vnp.mDownVelocityRMSError;
    vnpMsg->RollRMSError = vnp.mRollRMSError;
    vnpMsg->PitchRMSError = vnp.mPitchRMSError;
    vnpMsg->HeadingRMSError = vnp.mHeadingRMSError;
    vnpMsg->ErrorEllipsoidSemiMajor = vnp.mErrorEllipsoidSemiMajor;
    vnpMsg->ErrorEllipsoidSemiMinor = vnp.mErrorEllipsoidSemiMinor;
    vnpMsg->ErrorEllipsoidOrientation = vnp.mErrorEllipsoidOrientation;
    _vehicleNavigationPerformancePublisher.publish(vnpMsg);
    _vnpFreq->tick();
  }

  void PosLvNode::publishTimeTaggedDMIData(
      const ros::Time& timestamp, const TimeTaggedDMIData& dmi) {
    boost::shared_ptr<poslv::TimeTaggedDMIDataMsg> dmiMsg(
      new poslv::TimeTaggedDMIDataMsg);
    dmiMsg->header.stamp = timestamp;
    dmiMsg->header.frame_id = _frameId;
    dmiMsg->TimeDistance.Time1 = dmi.mTimeDistance.mTime1;
    dmiMsg->TimeDistance.Time2 = dmi.mTimeDistance.mTime2;
    dmiMsg->TimeDistance.DistanceTag = dmi.mTimeDistance.mDistanceTag;
    dmiMsg->TimeDistance.TimeType = dmi.mTimeDistance.mTimeType;
    dmiMsg->TimeDistance.DistanceType = dmi.mTimeDistance.mDistanceType;
    dmiMsg->SignedDistanceTraveled = dmi.mSignedDistanceTraveled;
    dmiMsg->UnsignedDistanceTraveled = dmi.mUnsignedDistanceTraveled;
    dmiMsg->DMIScaleFactor = dmi.mDMIScaleFactor;
    dmiMsg->DataStatus = dmi.mDataStatus;
    dmiMsg->DMIType = dmi.mDMIType;
    dmiMsg->DMIDataRate = dmi.mDMIDataRate;
    _timeTaggedDMIDataPublisher.publish(dmiMsg);
    _dmiFreq->tick();
  }

  void PosLvNode::diagnoseTCPConnection(
      diagnostic_updater::DiagnosticStatusWrapper& status) {
    if (_tcpConnection != nullptr && _tcpConnection->isOpen())
      status.summaryf(diagnostic_msgs::DiagnosticStatus::OK,
        "TCP connection opened on %s:%d.",
        _tcpConnection->getServerIP().c_str(),
        _tcpConnection->getPort());
    else
     status.summaryf(diagnostic_msgs::DiagnosticStatus::ERROR,
      "TCP connection closed.");
  }

  void PosLvNode::diagnoseAlignStatus(
      diagnostic_updater::DiagnosticStatusWrapper& status) {
    switch (_alignStatus) {
      case 0:
        status.summaryf(diagnostic_msgs::DiagnosticStatus::OK,
          "Full navigation");
        break;
      case 1:
        status.summaryf(diagnostic_msgs::DiagnosticStatus::WARN,
          "Fine alignment is active");
        break;
      case 2:
        status.summaryf(diagnostic_msgs::DiagnosticStatus::WARN,
          "GC CHI 2");
        break;
      case 3:
        status.summaryf(diagnostic_msgs::DiagnosticStatus::WARN,
          "PC CHI 2");
        break;
      case 4:
        status.summaryf(diagnostic_msgs::DiagnosticStatus::WARN,
          "GC CHI 1");
        break;
      case 5:
        status.summaryf(diagnostic_msgs::DiagnosticStatus::WARN,
          "PC CHI 1");
        break;
      case 6:
        status.summaryf(diagnostic_msgs::DiagnosticStatus::WARN,
          "Coarse leveling is active");
        break;
      case 7:
        status.summaryf(diagnostic_msgs::DiagnosticStatus::WARN,
          "Initial solution assigned");
        break;
      case 8:
        status.summaryf(diagnostic_msgs::DiagnosticStatus::ERROR,
          "No valid solution");
        break;
      default:
        break;
    }
  }

  void PosLvNode::diagnoseNavStatusPrimary(
      diagnostic_updater::DiagnosticStatusWrapper& status) {
    switch (_navStatus1) {
      case -1:
        status.summaryf(diagnostic_msgs::DiagnosticStatus::ERROR,
          "Unkown");
        break;
      case 0:
        status.summaryf(diagnostic_msgs::DiagnosticStatus::ERROR,
          "No data from receiver");
        break;
      case 1:
        status.summaryf(diagnostic_msgs::DiagnosticStatus::WARN,
          "Horizontal C/A mode");
        break;
      case 2:
        status.summaryf(diagnostic_msgs::DiagnosticStatus::WARN,
          "3-dimension C/A mode");
        break;
      case 3:
        status.summaryf(diagnostic_msgs::DiagnosticStatus::WARN,
          "Horizontal DGPS mode");
        break;
      case 4:
        status.summaryf(diagnostic_msgs::DiagnosticStatus::WARN,
          "3-dimension DGPS mode");
        break;
      case 5:
        status.summaryf(diagnostic_msgs::DiagnosticStatus::WARN,
          "Float RTK mode");
        break;
      case 6:
        status.summaryf(diagnostic_msgs::DiagnosticStatus::WARN,
          "Integer wide lane RTK mode");
        break;
      case 7:
        status.summaryf(diagnostic_msgs::DiagnosticStatus::ERROR,
          "Integer narrow lane RTK mode");
        break;
      case 8:
        status.summaryf(diagnostic_msgs::DiagnosticStatus::ERROR,
          "P-Code");
        break;
      default:
        break;
    }
  }

  void PosLvNode::diagnoseNavStatusSecondary(
      diagnostic_updater::DiagnosticStatusWrapper& status) {
    switch (_navStatus2) {
      case -1:
        status.summaryf(diagnostic_msgs::DiagnosticStatus::ERROR,
          "Unkown");
        break;
      case 0:
        status.summaryf(diagnostic_msgs::DiagnosticStatus::ERROR,
          "No data from receiver");
        break;
      case 1:
        status.summaryf(diagnostic_msgs::DiagnosticStatus::WARN,
          "Horizontal C/A mode");
        break;
      case 2:
        status.summaryf(diagnostic_msgs::DiagnosticStatus::WARN,
          "3-dimension C/A mode");
        break;
      case 3:
        status.summaryf(diagnostic_msgs::DiagnosticStatus::WARN,
          "Horizontal DGPS mode");
        break;
      case 4:
        status.summaryf(diagnostic_msgs::DiagnosticStatus::WARN,
          "3-dimension DGPS mode");
        break;
      case 5:
        status.summaryf(diagnostic_msgs::DiagnosticStatus::WARN,
          "Float RTK mode");
        break;
      case 6:
        status.summaryf(diagnostic_msgs::DiagnosticStatus::WARN,
          "Integer wide lane RTK mode");
        break;
      case 7:
        status.summaryf(diagnostic_msgs::DiagnosticStatus::ERROR,
          "Integer narrow lane RTK mode");
        break;
      case 8:
        status.summaryf(diagnostic_msgs::DiagnosticStatus::ERROR,
          "P-Code");
        break;
      default:
        break;
    }
  }

  void PosLvNode::spin() {
    _tcpConnection.reset(new TCPConnectionClient(_deviceIpStr, _devicePort));
    POSLVComTCP device(*_tcpConnection);
    Timer timer;
    while (_nodeHandle.ok()) {
      try {
        std::shared_ptr<Packet> packet = device.readPacket();
        const ros::Time timestamp = ros::Time::now();
        const Group& group = packet->groupCast();
        if (group.instanceOf<VehicleNavigationSolution>()) {
          const VehicleNavigationSolution& vns =
            group.typeCast<VehicleNavigationSolution>();
          publishVehicleNavigationSolution(timestamp, vns);
        }
        else if (group.instanceOf<VehicleNavigationPerformance>()) {
          const VehicleNavigationPerformance& vnp =
            group.typeCast<VehicleNavigationPerformance>();
          publishVehicleNavigationPerformance(timestamp, vnp);
        }
        else if (group.instanceOf<TimeTaggedDMIData>()) {
          const TimeTaggedDMIData& dmi = group.typeCast<TimeTaggedDMIData>();
          publishTimeTaggedDMIData(timestamp, dmi);
        }
        else if (group.instanceOf<PrimaryGPSStatus>()) {
          const PrimaryGPSStatus& gps = group.typeCast<PrimaryGPSStatus>();
          _navStatus1 = gps.mNavigationSolutionStatus;
        }
        else if (group.instanceOf<SecondaryGPSStatus>()) {
          const SecondaryGPSStatus& gps = group.typeCast<SecondaryGPSStatus>();
          _navStatus2 = gps.mNavigationSolutionStatus;
        }
      } 
      catch (const IOException& e) {
        ROS_WARN_STREAM("IOException: " << e.what());
        ROS_WARN_STREAM("Retrying in " << _retryTimeout << " [s]");
        timer.sleep(_retryTimeout);
      }
      catch (SystemException& e) {
        ROS_WARN_STREAM("SystemException: " << e.what());
        ROS_WARN_STREAM("Retrying in " << _retryTimeout << " [s]");
        timer.sleep(_retryTimeout);
      }
      _updater.update();
      ros::spinOnce();
    }
  }

  void PosLvNode::getParameters() {
    _nodeHandle.param<std::string>("ros/frame_id", _frameId,
      "vehicle_base_link");
    _nodeHandle.param<int>("ros/queue_depth", _queueDepth, 100);
    _nodeHandle.param<std::string>("connection/device_ip", _deviceIpStr,
      "129.132.39.171");
    _nodeHandle.param<int>("connection/device_port", _devicePort, 5602);
    _nodeHandle.param<double>("connection/retry_timeout", _retryTimeout, 1);
    _nodeHandle.param<double>("diagnostics/vns_min_freq", _vnsMinFreq, 1);
    _nodeHandle.param<double>("diagnostics/vns_max_freq", _vnsMaxFreq, 200);
    _nodeHandle.param<double>("diagnostics/vnp_min_freq", _vnpMinFreq, 0.5);
    _nodeHandle.param<double>("diagnostics/vnp_max_freq", _vnpMaxFreq, 1);
    _nodeHandle.param<double>("diagnostics/dmi_min_freq", _dmiMinFreq, 1);
    _nodeHandle.param<double>("diagnostics/dmi_max_freq", _dmiMaxFreq, 200);
  }

}