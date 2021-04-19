// Copyright 2015-2020 Tier IV, Inc. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <quaternion_operation/quaternion_operation.h>

#include <awapi_accessor/accessor.hpp>
#include <memory>
#include <openscenario_msgs/msg/waypoints_array.hpp>
#include <string>
#include <traffic_simulator/entity/ego_entity.hpp>
#include <unordered_map>
#include <vector>

namespace traffic_simulator
{
namespace entity
{
std::unordered_map<std::string, std::shared_ptr<autoware_api::Accessor> > EgoEntity::autowares{};

void EgoEntity::requestAssignRoute(
  const std::vector<openscenario_msgs::msg::LaneletPose> & waypoints)
{
  assert(1 < waypoints.size());

  const auto destination = (*hdmap_utils_ptr_).toMapPose(waypoints.back());

  std::vector<geometry_msgs::msg::PoseStamped> constraints{};

  for (auto iter = std::cbegin(waypoints); std::next(iter) != std::cend(waypoints); ++iter) {
    constraints.push_back((*hdmap_utils_ptr_).toMapPose(*iter));
  }

  return requestAcquirePosition(destination, constraints);
}

const openscenario_msgs::msg::WaypointsArray EgoEntity::getWaypoints()
{
  openscenario_msgs::msg::WaypointsArray waypoints{};

  for (const auto & point : std::atomic_load(&autowares.at(name))->getTrajectory().points) {
    waypoints.waypoints.emplace_back(point.pose.position);
  }

  return waypoints;
}

bool EgoEntity::setStatus(const openscenario_msgs::msg::EntityStatus & status)
{
  // NOTE Currently, setStatus always succeeds.
  const bool success = VehicleEntity::setStatus(status);

  const auto current = getStatus();

  if (autoware_initialized) {
    updateAutoware(current.pose);
  }

  if (!initial_pose_) {
    initial_pose_ = current.pose;
  }

  return success;
}

void EgoEntity::requestLaneChange(const std::int64_t to_lanelet_id)
{
  std::stringstream what{};
  what << "From scenario, a lane change was requested to Ego type entity '" << name << "'. ";
  what << "In general, such a request is an error, ";
  what << "since Ego cars make autonomous decisions about everything but their destination.";
  throw std::runtime_error(what.str());
}

void EgoEntity::onUpdate(double current_time, double step_time)
{
  if (current_time < 0) {
    updateEntityStatusTimestamp(current_time);
  } else {
    Eigen::VectorXd input(2);
    {
      input << std::atomic_load(&autowares.at(name))->getVehicleCommand().control.velocity,
        std::atomic_load(&autowares.at(name))->getVehicleCommand().control.steering_angle;
    }

    (*vehicle_model_ptr_).setInput(input);
    (*vehicle_model_ptr_).update(step_time);

    setStatus(getEntityStatus(current_time + step_time, step_time));

    if (previous_linear_velocity_) {
      linear_jerk_ = (vehicle_model_ptr_->getVx() - previous_linear_velocity_.get()) / step_time;
    } else {
      linear_jerk_ = 0;
    }

    previous_linear_velocity_ = vehicle_model_ptr_->getVx();
    previous_angular_velocity_ = vehicle_model_ptr_->getWz();
  }
}

const openscenario_msgs::msg::EntityStatus EgoEntity::getEntityStatus(
  const double time, const double step_time) const
{
  geometry_msgs::msg::Vector3 rpy;
  {
    rpy.x = 0;
    rpy.y = 0;
    rpy.z = vehicle_model_ptr_->getYaw();
  }

  geometry_msgs::msg::Pose pose;
  {
    pose.position.x = vehicle_model_ptr_->getX();
    pose.position.y = vehicle_model_ptr_->getY();
    pose.position.z = 0.0;
    pose.orientation = quaternion_operation::convertEulerAngleToQuaternion(rpy);
  }

  geometry_msgs::msg::Twist twist;
  {
    twist.linear.x = vehicle_model_ptr_->getVx();
    twist.angular.z = vehicle_model_ptr_->getWz();
  }

  geometry_msgs::msg::Accel accel;
  {
    if (previous_angular_velocity_ && previous_linear_velocity_) {
      accel.linear.x = (twist.linear.x - previous_linear_velocity_.get()) / step_time;
      accel.angular.z = (twist.angular.z - previous_angular_velocity_.get()) / step_time;
    }
  }

  Eigen::VectorXd v(3);
  {
    v(0) = pose.position.x;
    v(1) = pose.position.y;
    v(2) = pose.position.z;

    v = quaternion_operation::getRotationMatrix((*initial_pose_).orientation) * v;
  }

  openscenario_msgs::msg::EntityStatus status;
  {
    status.time = time;
    status.type.type = openscenario_msgs::msg::EntityType::EGO;
    status.bounding_box = getBoundingBox();
    status.action_status.twist = twist;
    status.action_status.accel = accel;
    status.pose.position.x = v(0) + initial_pose_.get().position.x;
    status.pose.position.y = v(1) + initial_pose_.get().position.y;
    status.pose.position.z = v(2) + initial_pose_.get().position.z;
    const auto closest_lanelet_id = hdmap_utils_ptr_->getClosetLanletId(status.pose);
    if (!closest_lanelet_id) {
      throw SimulationRuntimeError("failed to closest lane.");
    }
    traffic_simulator::math::CatmullRomSpline spline(
      hdmap_utils_ptr_->getCenterPoints(closest_lanelet_id.get()));
    const auto s_value = spline.getSValue(status.pose.position);
    if (s_value) {
      status.pose.position.z = spline.getPoint(s_value.get()).z;
    }

    status.pose.orientation = initial_pose_.get().orientation * pose.orientation;

    const auto lanelet_pose = hdmap_utils_ptr_->toLaneletPose(status.pose);

    status.lanelet_pose_valid = static_cast<bool>(lanelet_pose);

    if (lanelet_pose) {
      status.lanelet_pose = lanelet_pose.get();
    }
  }

  return status;
}
}  // namespace entity
}  // namespace traffic_simulator
