// AirMap Platform SDK
// Copyright © 2018 AirMap, Inc. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the License);
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//   http://www.apache.org/licenses/LICENSE-2.0
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an AS IS BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#define BOOST_TEST_MODULE platform

#include <airmap/platform/interface.h>
#include <airmap/platform/standard_paths.h>

#include <airmap/platform/null/interface.h>

#include <boost/test/included/unit_test.hpp>

#include <filesystem>

BOOST_AUTO_TEST_CASE(scope_is_inserted_correctly_into_output_stream) {
  {
    std::stringstream ss;
    ss << airmap::platform::StandardPaths::Scope::system;
    BOOST_CHECK(ss.str() == "system");
  }
  {
    std::stringstream ss;
    ss << airmap::platform::StandardPaths::Scope::user;
    BOOST_CHECK(ss.str() == "user");
  }
}

BOOST_AUTO_TEST_CASE(location_is_inserted_correctly_into_output_stream) {
  {
    std::stringstream ss;
    ss << airmap::platform::StandardPaths::Location::cache;
    BOOST_CHECK(ss.str() == "cache");
  }
  {
    std::stringstream ss;
    ss << airmap::platform::StandardPaths::Location::config;
    BOOST_CHECK(ss.str() == "config");
  }
  {
    std::stringstream ss;
    ss << airmap::platform::StandardPaths::Location::data;
    BOOST_CHECK(ss.str() == "data");
  }
  {
    std::stringstream ss;
    ss << airmap::platform::StandardPaths::Location::runtime;
    BOOST_CHECK(ss.str() == "runtime");
  }
}

BOOST_AUTO_TEST_CASE(null_platform_always_returns_initial_path) {
  const std::filesystem::path initial_path = airmap::platform::current_path();
  airmap::platform::null::Interface itf;
  for (auto scope : {airmap::platform::StandardPaths::Scope::user, airmap::platform::StandardPaths::Scope::system})
    for (auto location :
         {airmap::platform::StandardPaths::Location::cache, airmap::platform::StandardPaths::Location::config,
          airmap::platform::StandardPaths::Location::data, airmap::platform::StandardPaths::Location::runtime})
      BOOST_CHECK(itf.standard_paths().path(scope, location) == initial_path);
}

#if defined(AIRMAP_PLATFORM_LINUX)

BOOST_AUTO_TEST_CASE(linux_platform_never_throws_for_queries_for_paths) {
  airmap::platform::null::Interface itf;
  for (auto scope : {airmap::platform::StandardPaths::Scope::user, airmap::platform::StandardPaths::Scope::system})
    for (auto location :
         {airmap::platform::StandardPaths::Location::cache, airmap::platform::StandardPaths::Location::config,
          airmap::platform::StandardPaths::Location::data, airmap::platform::StandardPaths::Location::runtime})
      BOOST_TEST_NOTHROW(itf.standard_paths().path(scope, location));
}

#endif  // AIRMAP_PLATFORM_LINUX
