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

#include <simulation_api/behavior/vehicle/behavior_tree.hpp>
#include <simulation_api/behavior/vehicle/follow_lane_sequence/follow_lane_action.hpp>
#include <simulation_api/math/catmull_rom_spline.hpp>

#include <string>
#include <vector>


namespace entity_behavior
{
namespace vehicle
{
namespace follow_lane_sequence
{
FollowLaneAction::FollowLaneAction(
  const std::string & name,
  const BT::NodeConfiguration & config)
: entity_behavior::VehicleActionNode(name, config) {}

const boost::optional<openscenario_msgs::msg::Obstacle> FollowLaneAction::calculateObstacle(
  const openscenario_msgs::msg::WaypointsArray & waypoints)
{
  return boost::none;
}

const openscenario_msgs::msg::WaypointsArray FollowLaneAction::calculateWaypoints()
{
  if (!entity_status.lanelet_pose_valid) {
    throw BehaviorTreeRuntimeError("failed to assign lane");
  }
  if (entity_status.action_status.twist.linear.x >= 0) {
    openscenario_msgs::msg::WaypointsArray waypoints;
    double horizon =
      boost::algorithm::clamp(entity_status.action_status.twist.linear.x * 5, 20, 50);
    simulation_api::math::CatmullRomSpline spline(hdmap_utils->getCenterPoints(route_lanelets));
    waypoints.waypoints = spline.getTrajectory(
      entity_status.lanelet_pose.s,
      entity_status.lanelet_pose.s + horizon, 1.0);
    return waypoints;
  } else {
    return openscenario_msgs::msg::WaypointsArray();
  }
}

void FollowLaneAction::getBlackBoardValues()
{
  openscenario_msgs::msg::LaneletPose target_lanelet_pose;
  VehicleActionNode::getBlackBoardValues();
  if (!getInput<openscenario_msgs::msg::LaneletPose>("target_lanelet_pose", target_lanelet_pose)) {
    target_lanelet_pose_ = boost::none;
  } else {
    target_lanelet_pose_ = target_lanelet_pose;
  }
}

BT::NodeStatus FollowLaneAction::tick()
{
  getBlackBoardValues();
  if (request != "none" && request != "follow_lane") {
    return BT::NodeStatus::FAILURE;
  }
  if (driver_model.see_around) {
    if (getRightOfWayEntities(route_lanelets).size() != 0) {
      return BT::NodeStatus::FAILURE;
    }
    auto distance_to_front_entity = getDistanceToFrontEntity();
    if (distance_to_front_entity) {
      if (distance_to_front_entity.get() <=
        calculateStopDistance() +
        vehicle_parameters.bounding_box.dimensions.x + 5)
      {
        return BT::NodeStatus::FAILURE;
      }
    }
    auto distance_to_stopline = getDistanceToStopLine(route_lanelets);
    auto distance_to_conflicting_entity = getDistanceToConflictingEntity(route_lanelets);
    if (distance_to_stopline) {
      if (distance_to_stopline.get() <=
        calculateStopDistance() +
        vehicle_parameters.bounding_box.dimensions.x * 0.5 + 5)
      {
        return BT::NodeStatus::FAILURE;
      }
    }
    if (distance_to_conflicting_entity) {
      if (distance_to_conflicting_entity.get() <
        (vehicle_parameters.bounding_box.dimensions.x + calculateStopDistance()))
      {
        return BT::NodeStatus::FAILURE;
      }
    }
  }
  if (!entity_status.lanelet_pose_valid) {
    setOutput(
      "updated_status",
      stopAtEndOfRoad());
    return BT::NodeStatus::RUNNING;
  }

  if (!target_speed) {
    target_speed = hdmap_utils->getSpeedLimit(route_lanelets);
  }
  auto updated_status = calculateEntityStatusUpdated(target_speed.get());
  setOutput("updated_status", updated_status);
  const auto waypoints = calculateWaypoints();
  const auto obstacle = calculateObstacle(waypoints);
  setOutput("waypoints", waypoints);
  setOutput("obstacle", obstacle);
  return BT::NodeStatus::RUNNING;
}
}  // namespace follow_lane_sequence
}  // namespace vehicle
}  // namespace entity_behavior
