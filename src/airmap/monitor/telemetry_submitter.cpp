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
#include <airmap/monitor/telemetry_submitter.h>

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

namespace uuids = boost::uuids;

namespace {
constexpr const char* component{"airmap::monitor::TelemetrySubmitter"};
}  // namespace

std::shared_ptr<airmap::monitor::TelemetrySubmitter> airmap::monitor::TelemetrySubmitter::create(
    const Credentials& credentials, const std::string& aircraft_id, const std::shared_ptr<Logger>& logger,
    const std::shared_ptr<airmap::Client>& client,
    const std::shared_ptr<Traffic::Monitor::Subscriber>& traffic_subscriber) {
  return std::shared_ptr<airmap::monitor::TelemetrySubmitter>{
      new airmap::monitor::TelemetrySubmitter{credentials, aircraft_id, logger, client, traffic_subscriber}};
}

airmap::monitor::TelemetrySubmitter::TelemetrySubmitter(
    const Credentials& credentials, const std::string& aircraft_id, const std::shared_ptr<Logger>& logger,
    const std::shared_ptr<airmap::Client>& client,
    const std::shared_ptr<Traffic::Monitor::Subscriber>& traffic_subscriber)
    : log_{logger},
      client_{client},
      traffic_subscriber_{traffic_subscriber},
      credentials_{credentials},
      aircraft_id_{aircraft_id} {
}

void airmap::monitor::TelemetrySubmitter::activate() {
  state_ = State::active;
  request_authorization();
}

void airmap::monitor::TelemetrySubmitter::set_mission_geometry(const Geometry& geometry) {
  mission_geometry_ = geometry;
}

void airmap::monitor::TelemetrySubmitter::deactivate() {
  if (state_ == State::inactive)
    return;

  state_ = State::inactive;

  if (authorization_ && flight_) {
    Flights::EndFlight::Parameters parameters;
    parameters.id            = flight_.get().id;

    client_->flights().end_flight(parameters, [sp = shared_from_this()](const auto& result) {
      if (!result) {
        sp->log_.errorf(component, "failed to end flight: %s", result.error());
      } else {
        sp->log_.infof(component, "successfully ended flight");
      }
    });
  }

  authorization_requested_      = false;
  pilot_id_requested_           = false;
  active_flights_requested_     = false;
  end_active_flights_requested_ = false;
  create_flight_plan_requested_ = false;
  submit_flight_plan_requested_ = false;
  traffic_monitoring_requested_ = false;
  start_flight_comms_requested_ = false;

  authorization_.reset();
  pilot_id_.reset();
  flight_plan_.reset();
  flight_.reset();
  traffic_monitor_.reset();
  encryption_key_.reset();
}

void airmap::monitor::TelemetrySubmitter::submit(const mavlink::GlobalPositionInt& position) {
  current_position_ = position;

  if (state_ == State::inactive)
    return;
  if (!authorization_ || !flight_ || !encryption_key_) {
    request_authorization();
    return;
  }

  client_->telemetry().submit_updates(flight_.get(), encryption_key_.get(),
                                      {Telemetry::Update{Telemetry::Position{
                                          milliseconds_since_epoch(Clock::universal_time()), position.lat / 1E7,
                                          position.lon / 1E7, position.alt / 1E3, position.relative_alt / 1E3, 2.}}});
}

void airmap::monitor::TelemetrySubmitter::request_authorization() {
  if (authorization_) {
    handle_request_authorization_finished(authorization_.get());
    return;
  }

  if (authorization_requested_)
    return;

  authorization_requested_ = true;

  if (credentials_.oauth) {
    Authenticator::AuthenticateWithPassword::Params params;
    params.oauth = credentials_.oauth.get();
    client_->authenticator().authenticate_with_password(params, [sp = shared_from_this()](const auto& result) {
      if (result) {
        sp->handle_request_authorization_finished(result.value().id);
      } else {
        sp->authorization_requested_ = false;
        sp->log_.errorf(component, "failed to authenticate with AirMap services: %s", result.error());
      }
    });
  } else {
    Authenticator::AuthenticateAnonymously::Params params{uuids::to_string(uuids::random_generator()())};
    client_->authenticator().authenticate_anonymously(params, [sp = shared_from_this()](const auto& result) {
      if (result) {
        sp->handle_request_authorization_finished(result.value().id);
      } else {
        sp->authorization_requested_ = false;
        sp->log_.errorf(component, "failed to authenticate with AirMap services: %s", result.error());
      }
    });
  }
}

void airmap::monitor::TelemetrySubmitter::handle_request_authorization_finished(std::string authorization) {
  log_.infof(component, "successfully requested authorization from AirMap services %s", authorization);
  authorization_ = authorization;
  request_pilot_id();
}

void airmap::monitor::TelemetrySubmitter::request_pilot_id() {
  if (pilot_id_) {
    handle_request_pilot_id_finished(pilot_id_.get());
    return;
  }

  if (pilot_id_requested_)
    return;

  pilot_id_requested_ = true;

  if (authorization_) {
    Pilots::Authenticated::Parameters params;
    params.authorization = authorization_.get();
    client_->pilots().authenticated(params, [sp = shared_from_this()](const auto& result) {
      if (result) {
        sp->handle_request_pilot_id_finished(result.value().id);
      } else {
        sp->pilot_id_requested_ = false;
        sp->log_.errorf(component, "failed to request pilot_id: %s", result.error());
      }
    });
  }
}

void airmap::monitor::TelemetrySubmitter::handle_request_pilot_id_finished(std::string pilot_id) {
  log_.infof(component, "successfully requested pilot_id from AirMap services %s", pilot_id);
  pilot_id_ = pilot_id;
  request_active_flights();
}

void airmap::monitor::TelemetrySubmitter::request_active_flights() {
  if (active_flights_) {
    handle_request_active_flights_finished(active_flights_.get());
    return;
  }

  if (active_flights_requested_) {
    return;
  }

  active_flights_requested_ = true;

  Flights::Search::Parameters params;
  params.start_before = Clock::universal_time();
  params.end_after    = Clock::universal_time();
  params.pilot_id     = pilot_id_;

  client_->flights().search(params, [sp = shared_from_this()](const auto& result) {
    if (result) {
      sp->handle_request_active_flights_finished(result.value().flights);
    } else {
      sp->active_flights_requested_ = false;
      sp->log_.errorf(component, "failed to request active flights: %s", result.error());
    }
  });
}

void airmap::monitor::TelemetrySubmitter::handle_request_active_flights_finished(std::vector<Flight> flights) {
  active_flights_ = flights;
  request_end_active_flights();
}

void airmap::monitor::TelemetrySubmitter::request_end_active_flights() {
  if (active_flights_ && active_flights_.get().empty()) {
    handle_request_end_active_flights_finished();
    return;
  }

  if (end_active_flights_requested_)
    return;

  end_active_flights_requested_ = true;

  for (const auto& flight : active_flights_.get()) {
    Flights::EndFlight::Parameters params;
    params.authorization = authorization_.get();
    params.id            = flight.id;

    client_->flights().end_flight(params, [sp = shared_from_this(), id = flight.id](const auto& result) {
      if (result) {
        sp->handle_request_end_active_flight_finished(id);
      } else {
        sp->end_active_flights_requested_ = false;
        sp->log_.errorf(component, "failed to end active flight %s: %s", id, result.error());
      }
    });
  }
}

void airmap::monitor::TelemetrySubmitter::handle_request_end_active_flight_finished(std::string id) {
  log_.infof(component, "successfully ended active flight: %s", id);

  auto it = std::find_if(active_flights_.get().begin(), active_flights_.get().end(),
                         [id](const auto& flight) { return flight.id == id; });

  if (it != active_flights_.get().end()) {
    active_flights_.get().erase(it);
  }

  if (active_flights_.get().empty()) {
    handle_request_end_active_flights_finished();
  }
}

void airmap::monitor::TelemetrySubmitter::handle_request_end_active_flights_finished() {
  log_.infof(component, "successfully ended all active flights");
  request_create_flight_plan();
}

void airmap::monitor::TelemetrySubmitter::request_create_flight_plan() {
  if (flight_plan_) {
    handle_request_create_flight_plan_finished(flight_plan_.get());
    return;
  }

  if (create_flight_plan_requested_)
    return;

  create_flight_plan_requested_ = true;

  if (current_position_) {
    if (mission_geometry_) {
      FlightPlans::Create::Parameters params;
      params.authorization = authorization_.get();
      params.latitude      = current_position_.get().lat / 1E7;
      params.longitude     = current_position_.get().lon / 1E7;
      if (!aircraft_id_.empty())
        params.aircraft = Pilot::Aircraft{aircraft_id_};
      params.pilot            = Pilot{pilot_id_.get()};
      params.start_time       = Clock::universal_time();
      params.end_time         = params.start_time + hours(1);
      params.geometry         = mission_geometry_.get();
      params.buffer           = 100;
      const auto& coordinates = mission_geometry_.get().details_for_polygon().outer_ring.coordinates;
      auto it                 = std::max_element(coordinates.begin(), coordinates.end(),
                                 [](Geometry::Coordinate const& lhs, Geometry::Coordinate const& rhs) {
                                   return lhs.altitude.get() < rhs.altitude.get();
                                 });
      params.max_altitude     = it->altitude.get();
      client_->flight_plans().create_by_polygon(params, [sp = shared_from_this()](const auto& result) {
        if (result) {
          sp->handle_request_create_flight_plan_finished(result.value());
        } else {
          sp->create_flight_plan_requested_ = false;
          sp->log_.errorf(component, "failed to create flight plan: %s", result.error());
        }
      });
    } else {
      Flights::CreateFlight::Parameters params;
      params.authorization = authorization_.get();
      params.latitude      = current_position_.get().lat / 1E7;
      params.longitude     = current_position_.get().lon / 1E7;
      params.aircraft_id   = aircraft_id_;
      params.start_time    = Clock::universal_time();
      params.end_time      = params.start_time + hours(1);
      client_->flights().create_flight_by_point(params, [sp = shared_from_this()](const auto& result) {
        if (result) {
          sp->handle_request_submit_flight_plan_finished(result.value());
        } else {
          sp->create_flight_plan_requested_ = false;
          sp->log_.errorf(component, "failed to create flight by point: %s", result.error());
        }
      });
    }
  }
}

void airmap::monitor::TelemetrySubmitter::handle_request_create_flight_plan_finished(FlightPlan plan) {
  log_.infof(component, "successfully created plan: %s", plan.id);
  flight_plan_ = plan;

  request_submit_flight_plan();
}

void airmap::monitor::TelemetrySubmitter::request_submit_flight_plan() {
  if (flight_) {
    handle_request_submit_flight_plan_finished(flight_.get());
    return;
  }

  if (submit_flight_plan_requested_)
    return;

  submit_flight_plan_requested_ = true;

  FlightPlans::Submit::Parameters params;
  params.authorization = authorization_.get();
  params.id            = flight_plan_.get().id;
  client_->flight_plans().submit(params, [sp = shared_from_this()](const auto& result) {
    if (result) {
      FlightPlan plan = result.value();
      Flight flight{plan.flight_id.get(),
                    plan.id,
                    plan.pilot,
                    plan.aircraft,
                    plan.takeoff.latitude,
                    plan.takeoff.longitude,
                    plan.altitude_agl.max,
                    plan.geometry,
                    Clock::universal_time(),
                    plan.start_time,
                    plan.end_time};
      sp->handle_request_submit_flight_plan_finished(flight);
    } else {
      sp->submit_flight_plan_requested_ = false;
      sp->log_.errorf(component, "failed to submit flight plan: %s", result.error());
    }
  });
}

void airmap::monitor::TelemetrySubmitter::handle_request_submit_flight_plan_finished(Flight flight) {
  log_.infof(component, "successfully created flight: %s", flight.id);
  flight_ = flight;

  request_monitor_traffic();
  request_start_flight_comms();
}

void airmap::monitor::TelemetrySubmitter::request_monitor_traffic() {
  if (traffic_monitor_) {
    handle_request_monitor_traffic_finished(traffic_monitor_.get());
    return;
  }

  if (traffic_monitoring_requested_)
    return;

  traffic_monitoring_requested_ = true;

  Traffic::Monitor::Params params;
  params.flight_id     = flight_.get().id;
  params.authorization = authorization_.get();

  client_->traffic().monitor(params, [sp = shared_from_this()](const auto& result) {
    if (result) {
      sp->handle_request_monitor_traffic_finished(result.value());
    } else {
      sp->traffic_monitoring_requested_ = false;
      sp->log_.errorf(component, "failed to start monitoring traffic: %s", result.error());
    }
  });
}

void airmap::monitor::TelemetrySubmitter::handle_request_monitor_traffic_finished(
    std::shared_ptr<Traffic::Monitor> monitor) {
  log_.infof(component, "successfully started to monitor traffic");
  traffic_monitor_ = monitor;
  traffic_monitor_.get()->subscribe(traffic_subscriber_);
}

void airmap::monitor::TelemetrySubmitter::request_start_flight_comms() {
  if (encryption_key_) {
    handle_request_start_flight_comms_finished(encryption_key_.get());
    return;
  }

  if (start_flight_comms_requested_)
    return;

  start_flight_comms_requested_ = true;

  Flights::StartFlightCommunications::Parameters params{authorization_.get(), flight_.get().id};

  client_->flights().start_flight_communications(params, [sp = shared_from_this()](const auto& result) {
    if (result) {
      sp->handle_request_start_flight_comms_finished(result.value().key);
    } else {
      sp->start_flight_comms_requested_ = false;
      sp->log_.errorf(component, "failed to start flight communications: %s", result.error());
    }
  });
}

void airmap::monitor::TelemetrySubmitter::handle_request_start_flight_comms_finished(std::string key) {
  log_.infof(component, "successfully started flight comms: %s", key);
  encryption_key_ = key;
}
