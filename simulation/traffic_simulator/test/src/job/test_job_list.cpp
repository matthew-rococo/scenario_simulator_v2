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

#include <gtest/gtest.h>

#include <traffic_simulator/job/job_list.hpp>

int main(int argc, char ** argv)
{
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

/**
 * @note 
 */
TEST(JobList, append)
{
  bool was_cleanup_func_called = false;
  auto update_func = [](const double) { return true; };
  auto cleanup_func = [&was_cleanup_func_called]() { was_cleanup_func_called = true; };
  const auto type = traffic_simulator::job::Type::UNKOWN;
  const auto event = traffic_simulator::job::Event::POST_UPDATE;
  const bool is_exclusive = true;

  auto job_list = traffic_simulator::job::JobList();

  job_list.append(update_func, cleanup_func, type, is_exclusive, event);
  const double step_time = 0.0;

  job_list.update(step_time, event);

  EXPECT_TRUE(was_cleanup_func_called);
}

/**
 * @note 
 */
TEST(JobList, append_doubled)
{
  bool first_cleanup = false;
  bool first_update = false;
  bool second_cleanup = false;
  bool second_update = false;

  auto first_update_func = [&first_update](const double) { return first_update = true; };
  auto first_cleanup_func = [&first_cleanup]() { first_cleanup = true; };
  auto second_update_func = [&second_update](const double) { return second_update = true; };
  auto second_cleanup_func = [&second_cleanup]() { second_cleanup = true; };

  const auto type = traffic_simulator::job::Type::UNKOWN;
  const auto event = traffic_simulator::job::Event::POST_UPDATE;
  const bool is_exclusive = true;

  auto job_list = traffic_simulator::job::JobList();

  job_list.append(first_update_func, first_cleanup_func, type, is_exclusive, event);
  job_list.append(second_update_func, second_cleanup_func, type, is_exclusive, event);

  const double step_time = 0.0;
  job_list.update(step_time, event);

  EXPECT_TRUE(first_cleanup);
  EXPECT_FALSE(first_update);
  EXPECT_TRUE(second_cleanup);
  EXPECT_TRUE(second_update);
}

/**
 * @note 
 */
TEST(JobList, update)
{
  int update_count = 0;
  int cleanup_count = 0;

  auto update_func = [&update_count](const double) {
    update_count++;
    return update_count >= 2;
  };
  auto cleanup_func = [&cleanup_count]() { cleanup_count++; };

  const auto type = traffic_simulator::job::Type::UNKOWN;
  const auto event = traffic_simulator::job::Event::POST_UPDATE;
  const bool is_exclusive = true;

  auto job_list = traffic_simulator::job::JobList();

  job_list.append(update_func, cleanup_func, type, is_exclusive, event);

  const double step_time = 0.0;

  EXPECT_EQ(0, update_count);
  EXPECT_EQ(0, cleanup_count);

  job_list.update(step_time, event);

  EXPECT_EQ(1, update_count);
  EXPECT_EQ(0, cleanup_count);

  job_list.update(step_time, event);

  EXPECT_EQ(2, update_count);
  EXPECT_EQ(1, cleanup_count);

  job_list.update(step_time, event);

  EXPECT_EQ(2, update_count);
  EXPECT_EQ(1, cleanup_count);
}
