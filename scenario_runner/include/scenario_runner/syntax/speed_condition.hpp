// Copyright 2015-2020 Autoware Foundation. All rights reserved.
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

#ifndef SCENARIO_RUNNER__SYNTAX__SPEED_CONDITION_HPP_
#define SCENARIO_RUNNER__SYNTAX__SPEED_CONDITION_HPP_

#include <scenario_runner/syntax/rule.hpp>
#include <scenario_runner/syntax/triggering_entities.hpp>

namespace scenario_runner
{
inline namespace syntax
{
/* ==== SpeedCondition =======================================================
 *
 * <xsd:complexType name="SpeedCondition">
 *   <xsd:attribute name="value" type="Double" use="required"/>
 *   <xsd:attribute name="rule" type="Rule" use="required"/>
 * </xsd:complexType>
 *
 * ======================================================================== */
struct SpeedCondition
{
  const Double value;

  const Rule compare;

  Scope inner_scope;

  const TriggeringEntities trigger;

  template<typename Node>
  explicit SpeedCondition(
    const Node & node, Scope & outer_scope,
    const TriggeringEntities & trigger)
  : value{readAttribute<Double>(node, outer_scope, "value")},
    compare{readAttribute<Rule>(node, outer_scope, "rule")},
    inner_scope{outer_scope},
    trigger{trigger}
  {}

  auto evaluate()
  {
    // return
    //   asBoolean(
    //     trigger([&](auto&& entity)
    //     {
    //       return compare(inner_scope.getEntityStatus(entity).twist.linear.x, value);
    //     }));

    return false_v;
  }
};
}
}  // namespace scenario_runner

#endif  // SCENARIO_RUNNER__SYNTAX__SPEED_CONDITION_HPP_
