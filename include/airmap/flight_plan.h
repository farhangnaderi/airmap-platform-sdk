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
#ifndef AIRMAP_FLIGHT_PLAN_H_
#define AIRMAP_FLIGHT_PLAN_H_

#include <airmap/date_time.h>
#include <airmap/evaluation.h>
#include <airmap/geometry.h>
#include <airmap/optional.h>
#include <airmap/pilot.h>
#include <airmap/ruleset.h>
#include <airmap/status.h>
#include <airmap/visibility.h>

#include <cstdint>
#include <iosfwd>
#include <string>
#include <unordered_map>
#include <vector>

namespace airmap {

/// FlightPlan bundles together properties describing a plan for a flight.
struct AIRMAP_EXPORT FlightPlan {
  /// Id models a unique identifier for a flight plan in the context of AirMap.
  using Id = std::string;

  /// Briefing bundles together information and data for a flight plan.
  /// The target audience is a hypothetical pilot or operator conducting
  /// the flight described in the flight plan.
  struct AIRMAP_EXPORT Briefing {
    /// AdvisoryStatus summarizes the status of all advisories applying to a specific flight plan.
    struct AIRMAP_EXPORT AdvisoryStatus {
      Status::Color color;                       ///< The overall color of the status.
      std::vector<Status::Advisory> advisories;  ///< The collection of relevant advisories.
    };

    DateTime created_at;      ///< The timestamp when the briefing was requested and created by the AirMap services.
    AdvisoryStatus airspace;  ///< The summary over all advisories relevant to a specific briefing/flight plan.
    Evaluation evaluation;    ///< The airspace ruleset evaluation returned for the briefing.
  };

  Id id;                            ///< The unique identifier of a flight in the context of AirMap.
  Optional<std::string> flight_id;  ///< The unique identifier of the flight that is created on successful submission.
  Pilot pilot;                      ///< The pilot responsible for the flight.
  Pilot::Aircraft aircraft;         ///< The aircraft conducting the flight.
  struct {
    float latitude;   ///< The latitude component of the takeoff coordinates in [°].
    float longitude;  ///< The longitude component of the takeoff coordinates in [°].
  } takeoff;          ///< The takeoff coordinate.
  struct {
    float max;                        ///< The maximum altitude over the entire flight in [m].
    float min;                        ///< The minimum altitude over the entire flight in [m].
  } altitude_agl;                     ///< The altitude range of the flight in [m] above ground level.
  float buffer;                       ///< The buffer in [m] around the geometry.
  Geometry geometry;                  ///< The geometry describing the flight.
  DateTime start_time;                ///< Point in time when the flight will start/was started.
  DateTime end_time;                  ///< Point in time when the fligth will end.
  std::vector<RuleSet::Id> rulesets;  ///< RuleSets that apply to this flight plan.
  std::unordered_map<std::string, RuleSet::Feature::Value> features;  ///< Additional properties of the planned flight.

  void reset() {
    id.clear();
    flight_id.reset();
    takeoff.latitude = takeoff.longitude = 0.0;
    altitude_agl.min = altitude_agl.max = 0.0;
    buffer = 0.0;
    geometry.reset();
    start_time.date();
    end_time.date();
    rulesets.clear();
    features.clear();
  }
};

}  // namespace airmap

#endif  // AIRMAP_FLIGHT_PLAN_H_
