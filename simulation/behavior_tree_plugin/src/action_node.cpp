// Copyright 2015 TIER IV, Inc. All rights reserved.
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

#include <algorithm>
#include <behavior_tree_plugin/action_node.hpp>
#include <geometry/bounding_box.hpp>
#include <memory>
#include <rclcpp/rclcpp.hpp>
#include <scenario_simulator_exception/exception.hpp>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace entity_behavior
{
ActionNode::ActionNode(const std::string & name, const BT::NodeConfiguration & config)
: BT::ActionNodeBase(name, config)
{
}

BT::NodeStatus ActionNode::executeTick() { return BT::ActionNodeBase::executeTick(); }

void ActionNode::getBlackBoardValues()
{
  if (!getInput<traffic_simulator::behavior::Request>("request", request)) {
    THROW_SIMULATION_ERROR("failed to get input request in ActionNode");
  }
  if (!getInput<double>("step_time", step_time)) {
    THROW_SIMULATION_ERROR("failed to get input step_time in ActionNode");
  }
  if (!getInput<double>("current_time", current_time)) {
    THROW_SIMULATION_ERROR("failed to get input current_time in ActionNode");
  }
  if (!getInput<std::shared_ptr<hdmap_utils::HdMapUtils>>("hdmap_utils", hdmap_utils)) {
    THROW_SIMULATION_ERROR("failed to get input hdmap_utils in ActionNode");
  }
  if (!getInput<std::shared_ptr<traffic_simulator::TrafficLightManagerBase>>(
        "traffic_light_manager", traffic_light_manager)) {
    THROW_SIMULATION_ERROR("failed to get input traffic_light_manager in ActionNode");
  }
  if (!getInput<traffic_simulator_msgs::msg::EntityStatus>("entity_status", entity_status)) {
    THROW_SIMULATION_ERROR("failed to get input entity_status in ActionNode");
  }

  if (!getInput<boost::optional<double>>("target_speed", target_speed)) {
    target_speed = boost::none;
  }

  if (!getInput<std::unordered_map<std::string, traffic_simulator_msgs::msg::EntityStatus>>(
        "other_entity_status", other_entity_status)) {
    THROW_SIMULATION_ERROR("failed to get input other_entity_status in ActionNode");
  }
  if (!getInput<std::unordered_map<std::string, traffic_simulator_msgs::msg::EntityType>>(
        "entity_type_list", entity_type_list)) {
    THROW_SIMULATION_ERROR("failed to get input entity_type_list in ActionNode");
  }
  if (!getInput<std::vector<std::int64_t>>("route_lanelets", route_lanelets)) {
    THROW_SIMULATION_ERROR("failed to get input route_lanelets in ActionNode");
  }
}

double ActionNode::getHorizon() const
{
  return boost::algorithm::clamp(entity_status.action_status.twist.linear.x * 5, 20, 50);
}

traffic_simulator_msgs::msg::EntityStatus ActionNode::stopAtEndOfRoad() const
{
  traffic_simulator_msgs::msg::EntityStatus entity_status_updated = entity_status;
  entity_status_updated.time = current_time + step_time;
  entity_status_updated.action_status.twist = geometry_msgs::msg::Twist();
  entity_status_updated.action_status.accel = geometry_msgs::msg::Accel();
  return entity_status_updated;
}

std::vector<traffic_simulator_msgs::msg::EntityStatus> ActionNode::getOtherEntityStatus(
  std::int64_t lanelet_id)
{
  std::vector<traffic_simulator_msgs::msg::EntityStatus> ret;
  for (const auto & status : other_entity_status) {
    if (status.second.lanelet_pose_valid) {
      if (status.second.lanelet_pose.lanelet_id == lanelet_id) {
        ret.emplace_back(status.second);
      }
    }
  }
  return ret;
}

boost::optional<double> ActionNode::getYieldStopDistance(
  const std::vector<std::int64_t> & following_lanelets)
{
  std::set<double> distances;
  for (const auto & lanelet : following_lanelets) {
    const auto right_of_way_ids = hdmap_utils->getRightOfWayLaneletIds(lanelet);
    for (const auto right_of_way_id : right_of_way_ids) {
      const auto other_status = getOtherEntityStatus(right_of_way_id);
      if (other_status.size() != 0) {
        auto distance = hdmap_utils->getLongitudinalDistance(
          entity_status.lanelet_pose.lanelet_id, entity_status.lanelet_pose.s, lanelet, 0);
        if (distance) {
          distances.insert(distance.get());
        }
      }
    }
    if (distances.size() != 0) {
      return *distances.begin();
    }
  }
  return boost::none;
}

std::vector<traffic_simulator_msgs::msg::EntityStatus> ActionNode::getRightOfWayEntities(
  const std::vector<std::int64_t> & following_lanelets)
{
  std::vector<traffic_simulator_msgs::msg::EntityStatus> ret;
  const auto lanelet_ids_list = hdmap_utils->getRightOfWayLaneletIds(following_lanelets);
  for (const auto & status : other_entity_status) {
    for (const auto & following_lanelet : following_lanelets) {
      for (const std::int64_t & lanelet_id : lanelet_ids_list.at(following_lanelet)) {
        if (lanelet_id == status.second.lanelet_pose.lanelet_id) {
          ret.emplace_back(status.second);
        }
      }
    }
  }
  return ret;
}

std::vector<traffic_simulator_msgs::msg::EntityStatus> ActionNode::getRightOfWayEntities()
{
  std::vector<traffic_simulator_msgs::msg::EntityStatus> ret;
  const auto lanelet_ids =
    hdmap_utils->getRightOfWayLaneletIds(entity_status.lanelet_pose.lanelet_id);
  if (lanelet_ids.empty()) {
    return ret;
  }
  for (const auto & status : other_entity_status) {
    for (const std::int64_t & lanelet_id : lanelet_ids) {
      if (lanelet_id == status.second.lanelet_pose.lanelet_id) {
        ret.emplace_back(status.second);
      }
    }
  }
  return ret;
}

boost::optional<double> ActionNode::getDistanceToTrafficLightStopLine(
  const std::vector<std::int64_t> & route_lanelets,
  const math::geometry::CatmullRomSplineInterface & spline)
{
  const auto traffic_light_ids = hdmap_utils->getTrafficLightIdsOnPath(route_lanelets);
  if (traffic_light_ids.empty()) {
    return boost::none;
  }
  std::set<double> collision_points = {};
  for (const auto id : traffic_light_ids) {
    using Color = traffic_simulator::TrafficLight::Color;
    using Status = traffic_simulator::TrafficLight::Status;
    using Shape = traffic_simulator::TrafficLight::Shape;
    if (auto && traffic_light = traffic_light_manager->getTrafficLight(id);
        traffic_light.contains(Color::red, Status::solid_on, Shape::circle) or
        traffic_light.contains(Color::yellow, Status::solid_on, Shape::circle)) {
      const auto collision_point = hdmap_utils->getDistanceToTrafficLightStopLine(spline, id);
      if (collision_point) {
        collision_points.insert(collision_point.get());
      }
    }
  }
  if (collision_points.empty()) {
    return boost::none;
  }
  return *collision_points.begin();
}

boost::optional<double> ActionNode::getDistanceToStopLine(
  const std::vector<std::int64_t> & route_lanelets,
  const std::vector<geometry_msgs::msg::Point> & waypoints)
{
  return hdmap_utils->getDistanceToStopLine(route_lanelets, waypoints);
}

boost::optional<double> ActionNode::getDistanceToFrontEntity(
  const math::geometry::CatmullRomSplineInterface & spline)
{
  auto name = getFrontEntityName(spline);
  if (!name) {
    return boost::none;
  }
  return getDistanceToTargetEntityPolygon(spline, name.get());
}

boost::optional<std::string> ActionNode::getFrontEntityName(
  const math::geometry::CatmullRomSplineInterface & spline)
{
  std::vector<double> distances;
  std::vector<std::string> entities;
  for (const auto & each : other_entity_status) {
    const auto distance = getDistanceToTargetEntityPolygon(spline, each.first);
    const auto quat = quaternion_operation::getRotation(
      entity_status.pose.orientation, other_entity_status.at(each.first).pose.orientation);
    /**
     * @note hard-coded parameter, if the Yaw value of RPY is in ~1.5708 -> 1.5708, entity is a candidate of front entity.
     */
    if (
      std::fabs(quaternion_operation::convertQuaternionToEulerAngle(quat).z) <=
      boost::math::constants::half_pi<double>()) {
      if (distance && distance.get() < 40) {
        entities.emplace_back(each.first);
        distances.emplace_back(distance.get());
      }
    }
  }
  if (entities.size() != distances.size()) {
    THROW_SIMULATION_ERROR("size of entities and distances vector does not match.");
  }
  if (distances.empty()) {
    return boost::none;
  }
  std::vector<double>::iterator iter = std::min_element(distances.begin(), distances.end());
  size_t index = std::distance(distances.begin(), iter);
  return entities[index];
}

boost::optional<double> ActionNode::getDistanceToTargetEntityOnCrosswalk(
  const math::geometry::CatmullRomSplineInterface & spline,
  const traffic_simulator_msgs::msg::EntityStatus & status)
{
  if (status.lanelet_pose_valid) {
    auto polygon = hdmap_utils->getLaneletPolygon(status.lanelet_pose.lanelet_id);
    return spline.getCollisionPointIn2D(polygon, false, true);
  }
  return boost::none;
}

traffic_simulator_msgs::msg::EntityStatus ActionNode::getEntityStatus(
  const std::string target_name) const
{
  if (other_entity_status.find(target_name) != other_entity_status.end()) {
    return other_entity_status.at(target_name);
  }
  THROW_SIMULATION_ERROR("other entity : ", target_name, " does not exist.");
}

boost::optional<double> ActionNode::getDistanceToTargetEntityPolygon(
  const math::geometry::CatmullRomSplineInterface & spline, const std::string target_name,
  double width_extension_right, double width_extension_left, double length_extension_front,
  double length_extension_rear)
{
  const auto status = getEntityStatus(target_name);
  if (status.lanelet_pose_valid == true) {
    return getDistanceToTargetEntityPolygon(
      spline, status, width_extension_right, width_extension_left, length_extension_front,
      length_extension_rear);
  }
  return boost::none;
}

boost::optional<double> ActionNode::getDistanceToTargetEntityPolygon(
  const math::geometry::CatmullRomSplineInterface & spline,
  const traffic_simulator_msgs::msg::EntityStatus & status, double width_extension_right,
  double width_extension_left, double length_extension_front, double length_extension_rear)
{
  if (status.lanelet_pose_valid) {
    const auto polygon = math::geometry::transformPoints(
      status.pose, math::geometry::getPointsFromBbox(
                     status.bounding_box, width_extension_right, width_extension_left,
                     length_extension_front, length_extension_rear));
    return spline.getCollisionPointIn2D(polygon, false, true);
  }
  return boost::none;
}

boost::optional<double> ActionNode::getDistanceToConflictingEntity(
  const std::vector<std::int64_t> & route_lanelets,
  const math::geometry::CatmullRomSplineInterface & spline)
{
  auto crosswalk_entity_status = getConflictingEntityStatusOnCrossWalk(route_lanelets);
  auto lane_entity_status = getConflictingEntityStatusOnLane(route_lanelets);
  std::set<double> distances;
  for (const auto & status : crosswalk_entity_status) {
    const auto s = getDistanceToTargetEntityOnCrosswalk(spline, status);
    if (s) {
      distances.insert(s.get());
    }
  }
  for (const auto & status : lane_entity_status) {
    const auto s = getDistanceToTargetEntityPolygon(spline, status, 0.0, 0.0, 0.0, 1.0);
    if (s) {
      distances.insert(s.get());
    }
  }
  if (distances.empty()) {
    return boost::none;
  }
  return *distances.begin();
}

std::vector<traffic_simulator_msgs::msg::EntityStatus>
ActionNode::getConflictingEntityStatusOnCrossWalk(
  const std::vector<std::int64_t> & route_lanelets) const
{
  std::vector<traffic_simulator_msgs::msg::EntityStatus> conflicting_entity_status;
  auto conflicting_crosswalks = hdmap_utils->getConflictingCrosswalkIds(route_lanelets);
  for (const auto & status : other_entity_status) {
    if (
      std::count(
        conflicting_crosswalks.begin(), conflicting_crosswalks.end(),
        status.second.lanelet_pose.lanelet_id) >= 1) {
      conflicting_entity_status.push_back(status.second);
    }
  }
  return conflicting_entity_status;
}

std::vector<traffic_simulator_msgs::msg::EntityStatus> ActionNode::getConflictingEntityStatusOnLane(
  const std::vector<std::int64_t> & route_lanelets) const
{
  std::vector<traffic_simulator_msgs::msg::EntityStatus> conflicting_entity_status;
  auto conflicting_lanes = hdmap_utils->getConflictingLaneIds(route_lanelets);
  for (const auto & status : other_entity_status) {
    if (
      std::count(
        conflicting_lanes.begin(), conflicting_lanes.end(),
        status.second.lanelet_pose.lanelet_id) >= 1) {
      conflicting_entity_status.push_back(status.second);
    }
  }
  return conflicting_entity_status;
}

bool ActionNode::foundConflictingEntity(const std::vector<std::int64_t> & following_lanelets) const
{
  auto conflicting_crosswalks = hdmap_utils->getConflictingCrosswalkIds(following_lanelets);
  auto conflicting_lanes = hdmap_utils->getConflictingLaneIds(following_lanelets);
  for (const auto & status : other_entity_status) {
    if (
      std::count(
        conflicting_crosswalks.begin(), conflicting_crosswalks.end(),
        status.second.lanelet_pose.lanelet_id) >= 1) {
      return true;
    }
    if (
      std::count(
        conflicting_lanes.begin(), conflicting_lanes.end(),
        status.second.lanelet_pose.lanelet_id) >= 1) {
      return true;
    }
  }
  return false;
}

traffic_simulator_msgs::msg::EntityStatus ActionNode::calculateEntityStatusUpdated(
  double target_speed, const traffic_simulator_msgs::msg::DynamicConstraints & constraints) const
{
  double linear_jerk_new = planLinearJerk(target_speed, constraints);
  geometry_msgs::msg::Accel accel_new =
    planAccel(linear_jerk_new, entity_status.action_status.accel, constraints);
  geometry_msgs::msg::Twist twist_new =
    planTwist(accel_new, entity_status.action_status.twist, constraints);
  accel_new = timeDerivative(twist_new, entity_status.action_status.twist);
  linear_jerk_new = timeDerivative(accel_new, entity_status.action_status.accel);

  std::int64_t new_lanelet_id = entity_status.lanelet_pose.lanelet_id;
  double new_s =
    entity_status.lanelet_pose.s +
    (twist_new.linear.x + entity_status.action_status.twist.linear.x) / 2.0 * step_time;
  if (new_s < 0) {
    auto previous_lanelet_ids =
      hdmap_utils->getPreviousLaneletIds(entity_status.lanelet_pose.lanelet_id);
    new_lanelet_id = previous_lanelet_ids[0];
    new_s = new_s + hdmap_utils->getLaneletLength(new_lanelet_id) - 0.01;
    traffic_simulator_msgs::msg::EntityStatus entity_status_updated;
    entity_status_updated.time = current_time + step_time;
    entity_status_updated.lanelet_pose.lanelet_id = new_lanelet_id;
    entity_status_updated.lanelet_pose.s = new_s;
    entity_status_updated.lanelet_pose.offset = entity_status.lanelet_pose.offset;
    entity_status_updated.lanelet_pose.rpy = entity_status.lanelet_pose.rpy;
    entity_status_updated.action_status.twist = twist_new;
    entity_status_updated.action_status.accel = accel_new;
    entity_status_updated.action_status.linear_jerk = linear_jerk_new;
    entity_status_updated.pose = hdmap_utils->toMapPose(entity_status_updated.lanelet_pose).pose;
    return entity_status_updated;
  } else {
    bool calculation_success = false;
    for (size_t i = 0; i < route_lanelets.size(); i++) {
      if (route_lanelets[i] == entity_status.lanelet_pose.lanelet_id) {
        double length = hdmap_utils->getLaneletLength(entity_status.lanelet_pose.lanelet_id);
        calculation_success = true;
        if (length < new_s) {
          if (i != (route_lanelets.size() - 1)) {
            new_s = new_s - length;
            new_lanelet_id = route_lanelets[i + 1];
            break;
          } else {
            new_s = new_s - length;
            auto next_ids = hdmap_utils->getNextLaneletIds(route_lanelets[i]);
            if (next_ids.empty()) {
              return stopAtEndOfRoad();
            }
            new_lanelet_id = next_ids[0];
            break;
          }
        }
      }
    }
    if (!calculation_success) {
      THROW_SIMULATION_ERROR(
        "failed to calculate next status calculateEntityStatusUpdated function");
    }
    traffic_simulator_msgs::msg::EntityStatus entity_status_updated;
    entity_status_updated.time = current_time + step_time;
    entity_status_updated.lanelet_pose.lanelet_id = new_lanelet_id;
    entity_status_updated.lanelet_pose.s = new_s;
    entity_status_updated.lanelet_pose.offset = entity_status.lanelet_pose.offset;
    entity_status_updated.lanelet_pose.rpy = entity_status.lanelet_pose.rpy;
    entity_status_updated.pose = hdmap_utils->toMapPose(entity_status_updated.lanelet_pose).pose;
    entity_status_updated.action_status.twist = twist_new;
    entity_status_updated.action_status.accel = accel_new;
    entity_status_updated.action_status.linear_jerk = linear_jerk_new;
    return entity_status_updated;
  }
  THROW_SIMULATION_ERROR("failed to calculate next status calculateEntityStatusUpdated function");
}

traffic_simulator_msgs::msg::EntityStatus ActionNode::calculateEntityStatusUpdatedInWorldFrame(
  double target_speed, const traffic_simulator_msgs::msg::DynamicConstraints & constraints) const
{
  double target_accel = (target_speed - entity_status.action_status.twist.linear.x) / step_time;
  if (entity_status.action_status.twist.linear.x > target_speed) {
    target_accel = target_accel - step_time * constraints.max_deceleration_rate;
    target_accel = boost::algorithm::clamp(target_accel, constraints.max_deceleration * -1, 0);
  } else {
    target_accel = target_accel + step_time * constraints.max_acceleration_rate;
    target_accel = boost::algorithm::clamp(target_accel, 0, constraints.max_acceleration);
  }
  geometry_msgs::msg::Accel accel_new;
  accel_new = entity_status.action_status.accel;
  accel_new.linear.x = target_accel;

  geometry_msgs::msg::Twist twist_new;
  twist_new.linear.x = entity_status.action_status.twist.linear.x + accel_new.linear.x * step_time;
  twist_new.linear.y = entity_status.action_status.twist.linear.y + accel_new.linear.y * step_time;
  twist_new.linear.z = entity_status.action_status.twist.linear.z + accel_new.linear.z * step_time;
  twist_new.angular.x =
    entity_status.action_status.twist.angular.x + accel_new.angular.x * step_time;
  twist_new.angular.y =
    entity_status.action_status.twist.angular.y + accel_new.angular.y * step_time;
  twist_new.angular.z =
    entity_status.action_status.twist.angular.z + accel_new.angular.z * step_time;

  geometry_msgs::msg::Pose pose_new;
  geometry_msgs::msg::Vector3 angular_trans_vec;
  angular_trans_vec.z = twist_new.angular.z * step_time;
  geometry_msgs::msg::Quaternion angular_trans_quat =
    quaternion_operation::convertEulerAngleToQuaternion(angular_trans_vec);
  pose_new.orientation =
    quaternion_operation::rotation(entity_status.pose.orientation, angular_trans_quat);
  Eigen::Vector3d trans_vec;
  trans_vec(0) = twist_new.linear.x * step_time;
  trans_vec(1) = twist_new.linear.y * step_time;
  trans_vec(2) = 0;
  Eigen::Matrix3d rotation_mat = quaternion_operation::getRotationMatrix(pose_new.orientation);
  trans_vec = rotation_mat * trans_vec;
  pose_new.position.x = trans_vec(0) + entity_status.pose.position.x;
  pose_new.position.y = trans_vec(1) + entity_status.pose.position.y;
  pose_new.position.z = trans_vec(2) + entity_status.pose.position.z;
  traffic_simulator_msgs::msg::EntityStatus entity_status_updated;
  entity_status_updated.time = current_time + step_time;
  entity_status_updated.pose = pose_new;
  entity_status_updated.action_status.twist = twist_new;
  entity_status_updated.action_status.accel = accel_new;
  entity_status_updated.lanelet_pose_valid = false;
  return entity_status_updated;
}

double ActionNode::calculateStopDistance(
  const traffic_simulator_msgs::msg::DynamicConstraints & constraints) const
{
  double v = entity_status.action_status.twist.linear.x;
  /**
   * @brief When the entity moving forward.
   */
  if (v >= 0) {
    double t = std::sqrt(2 * v) / constraints.max_deceleration_rate;
    if (t * constraints.max_deceleration_rate <= constraints.max_deceleration) {
      return (t * t * t) / 6.0 * constraints.max_deceleration_rate;
    } else {
      double t1 = constraints.max_deceleration / constraints.max_deceleration_rate;
      double v1 = v - t1 * constraints.max_deceleration_rate;
      return (t1 * t1 * t1) / 6.0 * constraints.max_deceleration_rate +
             (v1 * v1) / (2 * std::fabs(constraints.max_deceleration));
    }
  }
  /**
   * @brief When the entity moving backward.
   */
  else {
    double t = std::sqrt(2 * v) / constraints.max_acceleration_rate;
    if (t * constraints.max_acceleration_rate <= constraints.max_acceleration) {
      return (t * t * t) / 6.0 * constraints.max_acceleration_rate;
    } else {
      double t1 = constraints.max_acceleration / constraints.max_acceleration_rate;
      double v1 = v - t1 * constraints.max_acceleration_rate;
      return (t1 * t1 * t1) / 6.0 * constraints.max_acceleration_rate +
             (v1 * v1) / (2 * std::fabs(constraints.max_acceleration));
    }
  }
}
}  // namespace entity_behavior
