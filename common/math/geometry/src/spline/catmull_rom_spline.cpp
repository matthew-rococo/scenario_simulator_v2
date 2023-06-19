// Copyright 2015 TIER IV.inc. All rights reserved.
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

#include <geometry/linear_algebra.hpp>
#include <geometry/spline/catmull_rom_spline.hpp>
#include <iostream>
#include <limits>
#include <optional>
#include <rclcpp/rclcpp.hpp>
#include <scenario_simulator_exception/exception.hpp>
#include <string>
#include <utility>
#include <vector>

namespace math
{
namespace geometry
{
const std::vector<geometry_msgs::msg::Point> CatmullRomSpline::getPolygon(
  const double width, const size_t num_points, const double z_offset)
{
  std::vector<geometry_msgs::msg::Point> points;
  std::vector<geometry_msgs::msg::Point> left_bounds = getLeftBounds(width, num_points, z_offset);
  std::vector<geometry_msgs::msg::Point> right_bounds = getRightBounds(width, num_points, z_offset);
  for (size_t i = 0; i < static_cast<size_t>(left_bounds.size() - 1); i++) {
    geometry_msgs::msg::Point pr_0 = right_bounds[i];
    geometry_msgs::msg::Point pl_0 = left_bounds[i];
    geometry_msgs::msg::Point pr_1 = right_bounds[i + 1];
    geometry_msgs::msg::Point pl_1 = left_bounds[i + 1];
    points.emplace_back(pr_0);
    points.emplace_back(pl_0);
    points.emplace_back(pr_1);
    points.emplace_back(pl_0);
    points.emplace_back(pl_1);
    points.emplace_back(pr_1);
  }
  return points;
}

const std::vector<geometry_msgs::msg::Point> CatmullRomSpline::getRightBounds(
  const double width, const size_t num_points, const double z_offset) const
{
  std::vector<geometry_msgs::msg::Point> points;
  double step_size = getLength() / static_cast<double>(num_points);
  for (size_t i = 0; i < static_cast<size_t>(num_points + 1); i++) {
    double s = step_size * static_cast<double>(i);
    points.emplace_back(
      [this](
        const double width, const double s, const double z_offset) -> geometry_msgs::msg::Point {
        geometry_msgs::msg::Vector3 vec = getNormalVector(s);
        double theta = std::atan2(vec.y, vec.x);
        geometry_msgs::msg::Point p = getPoint(s);
        geometry_msgs::msg::Point point;
        point.x = p.x + 0.5 * width * std::cos(theta);
        point.y = p.y + 0.5 * width * std::sin(theta);
        point.z = p.z + z_offset;
        return point;
      }(width, s, z_offset));
  }
  return points;
}

const std::vector<geometry_msgs::msg::Point> CatmullRomSpline::getLeftBounds(
  const double width, const size_t num_points, const double z_offset) const
{
  std::vector<geometry_msgs::msg::Point> points;
  double step_size = getLength() / static_cast<double>(num_points);
  for (size_t i = 0; i < static_cast<size_t>(num_points + 1); i++) {
    double s = step_size * static_cast<double>(i);
    points.emplace_back(
      [this](
        const double width, const double s, const double z_offset) -> geometry_msgs::msg::Point {
        geometry_msgs::msg::Vector3 vec = getNormalVector(s);
        double theta = std::atan2(vec.y, vec.x);
        geometry_msgs::msg::Point p = getPoint(s);
        geometry_msgs::msg::Point point;
        point.x = p.x - 0.5 * width * std::cos(theta);
        point.y = p.y - 0.5 * width * std::sin(theta);
        point.z = p.z + z_offset;
        return point;
      }(width, s, z_offset));
  }
  return points;
}

const std::vector<geometry_msgs::msg::Point> CatmullRomSpline::getTrajectory(
  const double start_s, const double end_s, const double resolution, const double offset) const
{
  if (start_s > end_s) {
    std::vector<geometry_msgs::msg::Point> ret;
    double s = start_s;
    while (s > end_s) {
      ret.emplace_back(getPoint(s, offset));
      s = s - std::fabs(resolution);
    }
    ret.emplace_back(getPoint(end_s, offset));
    return ret;
  } else {
    std::vector<geometry_msgs::msg::Point> ret;
    double s = start_s;
    while (s < end_s) {
      ret.emplace_back(getPoint(s, offset));
      s = s + std::fabs(resolution);
    }
    ret.emplace_back(getPoint(end_s, offset));
    return ret;
  }
}

CatmullRomSpline::CatmullRomSpline(const std::vector<geometry_msgs::msg::Point> & control_points)
: control_points(control_points), line_segments_(getLineSegments(control_points))
{
  switch (control_points.size()) {
    case 0:
      THROW_SEMANTIC_ERROR("Control points are empty. We cannot determine the shape of the curve.");
      break;
    /// @note In this case, spline is interpreted as point.
    case 1:
      break;
    /// @note In this case, spline is interpreted as line segment.
    case 2:
      break;
    /// @note In this case, spline is interpreted as curve.
    default:
      [this](const auto & control_points) -> void {
        size_t n = control_points.size() - 1;
        for (size_t i = 0; i < n; i++) {
          if (i == 0) {
            double ax = 0;
            double bx = control_points[0].x - 2 * control_points[1].x + control_points[2].x;
            double cx = -3 * control_points[0].x + 4 * control_points[1].x - control_points[2].x;
            double dx = 2 * control_points[0].x;
            double ay = 0;
            double by = control_points[0].y - 2 * control_points[1].y + control_points[2].y;
            double cy = -3 * control_points[0].y + 4 * control_points[1].y - control_points[2].y;
            double dy = 2 * control_points[0].y;
            double az = 0;
            double bz = control_points[0].z - 2 * control_points[1].z + control_points[2].z;
            double cz = -3 * control_points[0].z + 4 * control_points[1].z - control_points[2].z;
            double dz = 2 * control_points[0].z;
            ax = ax * 0.5;
            bx = bx * 0.5;
            cx = cx * 0.5;
            dx = dx * 0.5;
            ay = ay * 0.5;
            by = by * 0.5;
            cy = cy * 0.5;
            dy = dy * 0.5;
            az = az * 0.5;
            bz = bz * 0.5;
            cz = cz * 0.5;
            dz = dz * 0.5;
            curves_.emplace_back(HermiteCurve(ax, bx, cx, dx, ay, by, cy, dy, az, bz, cz, dz));
          } else if (i == (n - 1)) {
            double ax = 0;
            double bx = control_points[i - 1].x - 2 * control_points[i].x + control_points[i + 1].x;
            double cx = -1 * control_points[i - 1].x + control_points[i + 1].x;
            double dx = 2 * control_points[i].x;
            double ay = 0;
            double by = control_points[i - 1].y - 2 * control_points[i].y + control_points[i + 1].y;
            double cy = -1 * control_points[i - 1].y + control_points[i + 1].y;
            double dy = 2 * control_points[i].y;
            double az = 0;
            double bz = control_points[i - 1].z - 2 * control_points[i].z + control_points[i + 1].z;
            double cz = -1 * control_points[i - 1].z + control_points[i + 1].z;
            double dz = 2 * control_points[i].z;
            ax = ax * 0.5;
            bx = bx * 0.5;
            cx = cx * 0.5;
            dx = dx * 0.5;
            ay = ay * 0.5;
            by = by * 0.5;
            cy = cy * 0.5;
            dy = dy * 0.5;
            az = az * 0.5;
            bz = bz * 0.5;
            cz = cz * 0.5;
            dz = dz * 0.5;
            curves_.emplace_back(HermiteCurve(ax, bx, cx, dx, ay, by, cy, dy, az, bz, cz, dz));
          } else {
            double ax = -1 * control_points[i - 1].x + 3 * control_points[i].x -
                        3 * control_points[i + 1].x + control_points[i + 2].x;
            double bx = 2 * control_points[i - 1].x - 5 * control_points[i].x +
                        4 * control_points[i + 1].x - control_points[i + 2].x;
            double cx = -control_points[i - 1].x + control_points[i + 1].x;
            double dx = 2 * control_points[i].x;
            double ay = -1 * control_points[i - 1].y + 3 * control_points[i].y -
                        3 * control_points[i + 1].y + control_points[i + 2].y;
            double by = 2 * control_points[i - 1].y - 5 * control_points[i].y +
                        4 * control_points[i + 1].y - control_points[i + 2].y;
            double cy = -control_points[i - 1].y + control_points[i + 1].y;
            double dy = 2 * control_points[i].y;
            double az = -1 * control_points[i - 1].z + 3 * control_points[i].z -
                        3 * control_points[i + 1].z + control_points[i + 2].z;
            double bz = 2 * control_points[i - 1].z - 5 * control_points[i].z +
                        4 * control_points[i + 1].z - control_points[i + 2].z;
            double cz = -control_points[i - 1].z + control_points[i + 1].z;
            double dz = 2 * control_points[i].z;
            ax = ax * 0.5;
            bx = bx * 0.5;
            cx = cx * 0.5;
            dx = dx * 0.5;
            ay = ay * 0.5;
            by = by * 0.5;
            cy = cy * 0.5;
            dy = dy * 0.5;
            az = az * 0.5;
            bz = bz * 0.5;
            cz = cz * 0.5;
            dz = dz * 0.5;
            curves_.emplace_back(HermiteCurve(ax, bx, cx, dx, ay, by, cy, dy, az, bz, cz, dz));
          }
        }
        for (const auto & curve : curves_) {
          length_list_.emplace_back(curve.getLength());
          maximum_2d_curvatures_.emplace_back(curve.getMaximum2DCurvature());
        }
        total_length_ = 0;
        for (const auto & length : length_list_) {
          total_length_ = total_length_ + length;
        }
        checkConnection();
      }(control_points);
      break;
  }
}

std::pair<size_t, double> CatmullRomSpline::getCurveIndexAndS(const double s) const
{
  if (s < 0) {
    return std::make_pair(0, s);
  }
  if (s >= total_length_) {
    return std::make_pair(
      curves_.size() - 1, s - (total_length_ - curves_[curves_.size() - 1].getLength()));
  }
  double current_s = 0;
  for (size_t i = 0; i < curves_.size(); i++) {
    double prev_s = current_s;
    current_s = current_s + length_list_[i];
    if (prev_s <= s && s < current_s) {
      return std::make_pair(i, s - prev_s);
    }
  }
  THROW_SIMULATION_ERROR("failed to calculate curve index");  // LCOV_EXCL_LINE
}

double CatmullRomSpline::getSInSplineCurve(const size_t curve_index, const double s) const
{
  size_t n = curves_.size();
  double ret = 0;
  for (size_t i = 0; i < n; i++) {
    if (i == curve_index) {
      return ret + s;
    } else {
      ret = ret + curves_[i].getLength();
    }
  }
  THROW_SEMANTIC_ERROR("curve index does not match");  // LCOV_EXCL_LINE
}

std::optional<double> CatmullRomSpline::getCollisionPointIn2D(
  const std::vector<geometry_msgs::msg::Point> & polygon, const bool search_backward) const
{
  const auto get_collision_point_2d_with_curve =
    [this](const auto & polygon, const auto search_backward) -> std::optional<double> {
    size_t n = curves_.size();
    if (search_backward) {
      for (size_t i = 0; i < n; i++) {
        auto s = curves_[n - 1 - i].getCollisionPointIn2D(polygon, search_backward);
        if (s) {
          return getSInSplineCurve(n - 1 - i, s.value());
        }
      }
      return std::optional<double>();
    } else {
      for (size_t i = 0; i < n; i++) {
        auto s = curves_[i].getCollisionPointIn2D(polygon, search_backward);
        if (s) {
          return std::optional<double>(getSInSplineCurve(i, s.value()));
        }
      }
      return std::optional<double>();
    }
    return std::optional<double>();
  };
  const auto get_collision_point_2d_with_line =
    [this](const auto & polygon, const auto search_backward) -> std::optional<double> {
    const auto polygon_lines = getLineSegments(polygon);
    std::vector<double> s_value_candidates = {};
    for (const auto & line : getLineSegments(polygon)) {
      if (const auto s_value = line_segments_[0].getIntersection2DSValue(line)) {
        s_value_candidates.emplace_back(s_value.value());
      }
    }
    if (s_value_candidates.empty()) {
      return std::optional<double>();
    }
    return search_backward
             ? *std::max_element(s_value_candidates.begin(), s_value_candidates.end())
             : *std::min_element(s_value_candidates.begin(), s_value_candidates.end());
  };
  const auto get_collision_point_2d_with_point =
    [this](const auto & polygon, const auto search_backward) { return std::optional<double>(); };
  switch (control_points.size()) {
    case 0:
      THROW_SEMANTIC_ERROR("Control points are empty. We cannot determine the shape of the curve.");
      break;
    /// @note In this case, spline is interpreted as point.
    case 1:
      return get_collision_point_2d_with_point(polygon, search_backward);
    /// @note In this case, spline is interpreted as line segment.
    case 2:
      return get_collision_point_2d_with_line(polygon, search_backward);
    /// @note In this case, spline is interpreted as curve.
    default:
      return get_collision_point_2d_with_curve(polygon, search_backward);
  }
}

std::optional<double> CatmullRomSpline::getCollisionPointIn2D(
  const geometry_msgs::msg::Point & point0, const geometry_msgs::msg::Point & point1,
  const bool search_backward) const
{
  size_t n = curves_.size();
  if (search_backward) {
    for (size_t i = 0; i < n; i++) {
      auto s = curves_[n - 1 - i].getCollisionPointIn2D(point0, point1, search_backward);
      if (s) {
        return getSInSplineCurve(n - 1 - i, s.value());
      }
    }
    return std::nullopt;
  } else {
    for (size_t i = 0; i < n; i++) {
      auto s = curves_[i].getCollisionPointIn2D(point0, point1, search_backward);
      if (s) {
        return getSInSplineCurve(i, s.value());
      }
    }
    return std::nullopt;
  }
  return std::nullopt;
}

std::optional<double> CatmullRomSpline::getSValue(
  const geometry_msgs::msg::Pose & pose, double threshold_distance) const
{
  double s = 0;
  for (size_t i = 0; i < curves_.size(); i++) {
    auto s_value = curves_[i].getSValue(pose, threshold_distance, true);
    if (s_value) {
      s = s + s_value.value();
      return s;
    }
    s = s + curves_[i].getLength();
  }
  return std::nullopt;
}

double CatmullRomSpline::getSquaredDistanceIn2D(
  const geometry_msgs::msg::Point & point, double s) const
{
  const auto index_and_s = getCurveIndexAndS(s);
  return curves_[index_and_s.first].getSquaredDistanceIn2D(point, index_and_s.second, true);
}

geometry_msgs::msg::Vector3 CatmullRomSpline::getSquaredDistanceVector(
  const geometry_msgs::msg::Point & point, double s) const
{
  const auto index_and_s = getCurveIndexAndS(s);
  return curves_[index_and_s.first].getSquaredDistanceVector(point, index_and_s.second, true);
}

const geometry_msgs::msg::Point CatmullRomSpline::getPoint(double s) const
{
  const auto index_and_s = getCurveIndexAndS(s);
  return curves_[index_and_s.first].getPoint(index_and_s.second, true);
}

const geometry_msgs::msg::Point CatmullRomSpline::getPoint(double s, double offset) const
{
  geometry_msgs::msg::Vector3 vec = getNormalVector(s);
  double theta = std::atan2(vec.y, vec.x);
  geometry_msgs::msg::Point p = getPoint(s);
  geometry_msgs::msg::Point point;
  point.x = p.x + offset * std::cos(theta);
  point.y = p.y + offset * std::sin(theta);
  point.z = p.z;
  return point;
}

double CatmullRomSpline::getMaximum2DCurvature() const
{
  if (maximum_2d_curvatures_.empty()) {
    THROW_SIMULATION_ERROR("maximum 2D curvature vector size is 0.");  // LCOV_EXCL_LINE
  }
  return *std::max_element(maximum_2d_curvatures_.begin(), maximum_2d_curvatures_.end());
}

const geometry_msgs::msg::Vector3 CatmullRomSpline::getNormalVector(double s) const
{
  const auto index_and_s = getCurveIndexAndS(s);
  return curves_[index_and_s.first].getNormalVector(index_and_s.second, true);
}

const geometry_msgs::msg::Vector3 CatmullRomSpline::getTangentVector(double s) const
{
  const auto index_and_s = getCurveIndexAndS(s);
  return curves_[index_and_s.first].getTangentVector(index_and_s.second, true);
}

const geometry_msgs::msg::Pose CatmullRomSpline::getPose(double s) const
{
  const auto index_and_s = getCurveIndexAndS(s);
  return curves_[index_and_s.first].getPose(index_and_s.second, true);
}

bool CatmullRomSpline::checkConnection() const
{
  if (control_points.size() != (curves_.size() + 1)) {
    THROW_SIMULATION_ERROR(                                    // LCOV_EXCL_LINE
      "number of control points and curves does not match.");  // LCOV_EXCL_LINE
  }
  for (size_t i = 0; i < curves_.size(); i++) {
    const auto control_point0 = control_points[i];
    const auto control_point1 = control_points[i + 1];
    const auto p0 = curves_[i].getPoint(0, false);
    const auto p1 = curves_[i].getPoint(1, false);
    if (equals(control_point0, p0) && equals(control_point1, p1)) {
      continue;
    } else if (!equals(control_point0, p0)) {                       // LCOV_EXCL_LINE
      THROW_SIMULATION_ERROR(                                       // LCOV_EXCL_LINE
        "start point of the curve number ", i, " does not match");  // LCOV_EXCL_LINE
    } else if (!equals(control_point1, p1)) {                       // LCOV_EXCL_LINE
      THROW_SIMULATION_ERROR(                                       // LCOV_EXCL_LINE
        "end point of the curve number ", i, " does not match");    // LCOV_EXCL_LINE
    }
  }
  if (curves_.empty()) {
    THROW_SIMULATION_ERROR("curve size should not be zero");  // LCOV_EXCL_LINE
  }
  return true;
}

bool CatmullRomSpline::equals(geometry_msgs::msg::Point p0, geometry_msgs::msg::Point p1) const
{
  constexpr double e = std::numeric_limits<float>::epsilon();
  if (std::abs(p0.x - p1.x) > e) {
    return false;  // LCOV_EXCL_LINE
  }
  if (std::abs(p0.y - p1.y) > e) {
    return false;  // LCOV_EXCL_LINE
  }
  if (std::abs(p0.z - p1.z) > e) {
    return false;  // LCOV_EXCL_LINE
  }
  return true;
}
}  // namespace geometry
}  // namespace math
