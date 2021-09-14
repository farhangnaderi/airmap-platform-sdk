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
#include <airmap/cmds/airmap/cmd/simulate_scenario.h>

#include <airmap/client.h>
#include <airmap/codec.h>
#include <airmap/context.h>
#include <airmap/paths.h>

#include <airmap/boost/context.h>
#include <airmap/mavlink/boost/tcp_route.h>
#include <airmap/mavlink/router.h>
#include <airmap/util/formatting_logger.h>
#include <airmap/util/scenario_simulator.h>
#include <airmap/util/telemetry_simulator.h>

#include <boost/asio.hpp>

#include <signal.h>

#include <fstream>
#include <iterator>
#include <thread>

namespace cli = airmap::util::cli;
namespace cmd = airmap::cmds::airmap::cmd;
namespace ph  = std::placeholders;

using json = nlohmann::json;

namespace {

namespace mavlink {
constexpr std::uint8_t component_id       = 42;
constexpr const std::uint32_t custom_mode = 0;
constexpr float altitude_msl              = 100.;
constexpr float altitude_gl               = 100.;
constexpr float vx                        = 0.;
constexpr float vy                        = 0.;
constexpr float vz                        = 100.;
constexpr float heading                   = 180.;
constexpr std::uint8_t mission_type       = 0;
constexpr std::uint8_t target_system      = 0;
constexpr std::uint8_t target_component   = 0;
constexpr std::uint8_t frame              = 0;
constexpr std::uint16_t command           = 0;
constexpr std::uint8_t current            = 0;
constexpr std::uint8_t autocontinue       = 0;
constexpr float p1                        = 0.;
constexpr float p2                        = 0.;
constexpr float p3                        = 0.;
constexpr float p4                        = 0.;
}  // namespace mavlink

const std::string device_name = boost::asio::ip::host_name();
const airmap::Seconds duration(airmap::seconds(60));
constexpr const char* component{"simulate-scenario-cli"};

class TcpRouteMonitor : public airmap::mavlink::boost::TcpRoute::Monitor {
 public:
  explicit TcpRouteMonitor(const airmap::util::Scenario& scenario) : scenario_{scenario} {
  }

  // From airmap::mavlink::boost::TcpRoute::Monitor
  void on_new_session(const std::shared_ptr<airmap::mavlink::boost::TcpRoute::Session>& session) override {
    // We announce all participants in the scenario.
    auto now = airmap::Clock::universal_time();

    for (const auto& p : scenario_.participants) {
      const auto& g          = p.geometry;
      const auto& outer_ring = g.details_for_polygon().outer_ring;
      const auto& to         = outer_ring.coordinates.at(0);

      {
        mavlink_message_t msg;
        mavlink_msg_global_position_int_pack(p.id, mavlink::component_id, &msg, airmap::milliseconds_since_epoch(now),
                                             to.latitude * 1E7, to.longitude * 1E7, mavlink::altitude_msl * 1E3,
                                             mavlink::altitude_gl * 1E3, mavlink::vx, mavlink::vy, mavlink::vz,
                                             mavlink::heading * 1E2);
        session->process(msg);
      }

      {
        mavlink_message_t msg;
        mavlink_msg_mission_count_pack(p.id, mavlink::component_id, &msg, mavlink::target_system,
                                       mavlink::target_component, outer_ring.coordinates.size(), mavlink::mission_type);
        session->process(msg);
      }

      // Send geometry as sequence of waypoints as part of mavlink mission
      uint16_t seq = 0;
      for (const auto& c : outer_ring.coordinates) {
        {
          mavlink_message_t msg;
          mavlink_msg_mission_item_pack(p.id, mavlink::component_id, &msg, mavlink::target_system,
                                        mavlink::target_component, seq, mavlink::frame, mavlink::command,
                                        mavlink::current, mavlink::autocontinue, mavlink::p1, mavlink::p2, mavlink::p3,
                                        mavlink::p4, c.latitude, c.longitude, mavlink::vz, MAV_CMD_NAV_WAYPOINT);
          session->process(msg);
          seq++;
        }
      }

      {
        mavlink_message_t msg;
        mavlink_msg_heartbeat_pack(p.id, mavlink::component_id, &msg, MAV_TYPE_HELICOPTER, MAV_AUTOPILOT_GENERIC,
                                   MAV_MODE_GUIDED_ARMED, mavlink::custom_mode, MAV_STATE_ACTIVE);
        session->process(msg);
      }
    }
  }

 private:
  airmap::util::Scenario scenario_;
};

}  // namespace

cmd::SimulateScenario::Collector::Collector(const util::Scenario& scenario) : scenario_{scenario} {
}

void cmd::SimulateScenario::Collector::collect_authentication_for(util::Scenario::Participants::iterator it,
                                                                  const std::string& authentication) {
  it->authentication = authentication;
}

void cmd::SimulateScenario::Collector::collect_authentication_for_index(std::size_t index,
                                                                        const std::string& authentication) {
  scenario_.participants.at(index).authentication = authentication;
}

void cmd::SimulateScenario::Collector::collect_flight_id_for(util::Scenario::Participants::iterator it,
                                                             const Flight& flight) {
  it->flight = flight;
}

void cmd::SimulateScenario::Collector::collect_flight_id_for_index(std::size_t index, const Flight& flight) {
  scenario_.participants.at(index).flight = flight;
}

void cmd::SimulateScenario::Collector::collect_traffic_monitor_for(util::Scenario::Participants::iterator it,
                                                                   const std::shared_ptr<Traffic::Monitor>& monitor) {
  it->monitor = monitor;
}

void cmd::SimulateScenario::Collector::collect_traffic_monitor_for_index(
    std::size_t index, const std::shared_ptr<Traffic::Monitor>& monitor) {
  scenario_.participants.at(index).monitor = monitor;
}

bool cmd::SimulateScenario::Collector::collect_key_for(util::Scenario::Participants::iterator it,
                                                       const std::string& key) {
  it->encryption_key = key;
  return scenario_.participants.size() == ++key_counter_;
}

bool cmd::SimulateScenario::Collector::collect_key_for_index(std::size_t index, const std::string& key) {
  scenario_.participants.at(index).encryption_key = key;
  return scenario_.participants.size() == ++key_counter_;
}

const airmap::util::Scenario& cmd::SimulateScenario::Collector::scenario() const {
  return scenario_;
}

airmap::util::Scenario& cmd::SimulateScenario::Collector::scenario() {
  return scenario_;
}

cmd::SimulateScenario::SimulateScenario()
    : cli::CommandWithFlagsAndAction{"simulate-scenario",
                                     "simulates multiple vehicles carrying out missions and submitting telemetry",
                                     "simulates multiple vehicles carrying out missions and submitting telemetry"} {
  flag(flags::version(params_.version));
  flag(flags::log_level(params_.log_level));
  flag(flags::config_file(params_.config_file));
  flag(flags::telemetry_host(params_.host));
  flag(flags::telemetry_port(params_.port));
  flag(cli::make_flag("mavlink-router-endpoint-port", "export a mavlink router on this ip",
                      params_.mavlink_router_endpoint_port));
  flag(cli::make_flag("scenario-file", "use the scenario defined in this json file", params_.scenario_file));

  action([this](const cli::Command::Context& ctxt) {
    log_ = util::FormattingLogger{create_filtering_logger(params_.log_level, create_default_logger(ctxt.cerr))};

    if (!params_.config_file) {
      params_.config_file = ConfigFile{paths::config_file(params_.version).string()};
    }

    if (params_.host && !params_.host.get().validate()) {
      log_.errorf(component, "parameter 'host' must not be empty");
      return 1;
    }

    if (!params_.scenario_file) {
      log_.errorf(component, "missing parameter 'scenario-file'");
      return 1;
    }

    std::ifstream in{params_.scenario_file.get()};
    if (!in) {
      log_.errorf(component, "failed to open %s for reading", params_.scenario_file.get());
      return 1;
    }
    util::Scenario scenario = json::parse(in);

    if (!scenario.duration) {
      scenario.duration = duration;
    }

    collector_ = std::make_shared<Collector>(scenario);
    runner_    = std::make_shared<util::ScenarioSimulator::Runner>(5);
    context_   = ::airmap::boost::Context::create(log_.logger());

    std::ifstream config_file{params_.config_file.get()};
    if (!config_file) {
      log_.errorf(component, "failed to open config file %s for reading", params_.config_file.get());
      return 1;
    }
    auto config = Client::load_configuration_from_json(config_file);

    if (params_.host)
      config.telemetry.host = params_.host.get();
    if (params_.port)
      config.telemetry.port = params_.port.get();

    auto route = ::airmap::mavlink::boost::TcpRoute::create(
        context_->io_service(),
        ::boost::asio::ip::tcp::endpoint{::boost::asio::ip::tcp::v4(), params_.mavlink_router_endpoint_port},
        log_.logger(), {std::make_shared<TcpRouteMonitor>(scenario)});

    router_ = std::shared_ptr<::airmap::mavlink::Router>(new ::airmap::mavlink::Router{route});
    router_->start();

    log_.infof(component,
               "configuration:\n"
               "  host:                      %s\n"
               "  version:                   %s\n"
               "  telemetry.host:            %s\n"
               "  telemetry.port:            %d\n"
               "  mavlink router (tcp) port: %d\n"
               "  credentials.api_key:       %s\n"
               "  duration:                  %s",
               config.host, config.version, config.telemetry.host, config.telemetry.port,
               params_.mavlink_router_endpoint_port, config.credentials.api_key, scenario.duration);

    context_->create_client_with_configuration(config, [this](const auto& result) mutable {
      if (not result) {
        log_.errorf(component, "failed to create client: %s", result.error());
        context_->stop(::airmap::Context::ReturnCode::error);
        return;
      }
      log_.infof(component, "successfully created client for AirMap services");

      client_  = result.value();
      auto it  = collector_->scenario().participants.begin();
      auto itE = collector_->scenario().participants.end();

      while (it != itE) {
        this->request_authentication_for(it);
        // this->deactivate(it);
        ++it;
      }
    });

    return context_->exec({SIGINT, SIGQUIT},
                          [this](int sig) {
                            log_.infof(component, "received [%s], shutting down", ::strsignal(sig));
                            context_->stop(::airmap::Context::ReturnCode::success);
                          }) == ::airmap::Context::ReturnCode::success
               ? 0
               : 1;
  });
}

void cmd::SimulateScenario::deactivate(util::Scenario::Participants::iterator participant) {
  context_->schedule_in([this, participant]() {
    mavlink_message_t msg;
    mavlink_msg_heartbeat_pack(participant->id, ::mavlink::component_id, &msg, MAV_TYPE_HELICOPTER,
                               MAV_AUTOPILOT_GENERIC, MAV_MODE_GUIDED_DISARMED, ::mavlink::custom_mode,
                               MAV_STATE_STANDBY);
    router_->route(msg);
  }, microseconds(1000 * 1000 * 10));
}

void cmd::SimulateScenario::request_authentication_for(util::Scenario::Participants::iterator participant) {
  if (participant->user) {
    Authenticator::AuthenticateWithPassword::Params params;
    params.oauth.client_id = participant->user.get().client_id;
    params.oauth.username  = participant->user.get().username;
    params.oauth.password  = participant->user.get().password;
    params.oauth.device_id = device_name;
    client_->authenticator().authenticate_with_password(
        params, std::bind(&SimulateScenario::handle_authenticated_with_password_result_for, this, participant, ph::_1));
  } else {
    client_->authenticator().authenticate_anonymously(
        {participant->pilot.id},
        std::bind(&SimulateScenario::handle_authenticated_anonymously_result_for, this, participant, ph::_1));
  }
}

void cmd::SimulateScenario::handle_authenticated_with_password_result_for(
    util::Scenario::Participants::iterator participant, const Authenticator::AuthenticateWithPassword::Result& result) {
  if (result) {
    log_.infof(component, "successfully authenticated with the AirMap services");
    collector_->collect_authentication_for(participant, result.value().id);
    request_flight_status_for(participant);
    request_create_flight_for(participant);
  } else {
    log_.errorf(component, "failed to authenticate with the Airmap services: %s", result.error());
    context_->stop(::airmap::Context::ReturnCode::error);
  }
}

void cmd::SimulateScenario::handle_authenticated_anonymously_result_for(
    util::Scenario::Participants::iterator participant, const Authenticator::AuthenticateAnonymously::Result& result) {
  if (result) {
    log_.infof(component, "successfully authenticated with the AirMap services");
    collector_->collect_authentication_for(participant, result.value().id);
    request_flight_status_for(participant);
    request_create_flight_for(participant);
  } else {
    log_.errorf(component, "failed to authenticate with the Airmap services: %s", result.error());
    context_->stop(::airmap::Context::ReturnCode::error);
  }
}

void cmd::SimulateScenario::request_flight_status_for(util::Scenario::Participants::iterator participant) {
  Status::GetStatus::Parameters params;
  const auto& polygon     = participant->geometry.details_for_polygon();
  params.latitude         = polygon.outer_ring.coordinates[0].latitude;
  params.longitude        = polygon.outer_ring.coordinates[0].longitude;
  params.geometry         = participant->geometry;
  params.flight_date_time = Clock::universal_time();
  params.weather          = true;

  client_->status().get_status_by_polygon(
      params, std::bind(&SimulateScenario::handle_flight_status_result_for, this, participant, ph::_1));
}

void cmd::SimulateScenario::handle_flight_status_result_for(util::Scenario::Participants::iterator,
                                                            const Status::GetStatus::Result& result) {
  if (result) {
    log_.infof(component,
               "successfully received status:\n"
               "  max-safe-distance: %d\n"
               "  advisory-color:    %s\n"
               "  weather:             \n"
               "    condition:       %s\n"
               "    temperature:     %s\n"
               "    humidity:        %s\n"
               "    visibility:      %d\n"
               "    precipitation:   %d\n"
               "    wind:\n"
               "      heading:       %d\n"
               "      speed:         %d\n"
               "      gusting:       %d\n",
               result.value().max_safe_distance, result.value().advisory_color, result.value().weather.condition,
               result.value().weather.temperature, result.value().weather.humidity, result.value().weather.visibility,
               result.value().weather.precipitation, result.value().weather.wind.heading,
               result.value().weather.wind.speed, result.value().weather.wind.gusting);
  } else {
    log_.errorf(component, "failed to get flight status: %s", result.error());
  }
}

void cmd::SimulateScenario::request_create_flight_for(util::Scenario::Participants::iterator participant) {
  Flights::CreateFlight::Parameters params;
  const auto& polygon  = participant->geometry.details_for_polygon();
  params.start_time    = Clock::universal_time();
  params.end_time      = Clock::universal_time() + collector_->scenario().duration.get();
  params.aircraft_id   = participant->aircraft.id;
  params.latitude      = polygon.outer_ring.coordinates[0].latitude;
  params.longitude     = polygon.outer_ring.coordinates[0].longitude;
  params.geometry      = participant->geometry;

  client_->flights().create_flight_by_polygon(
      params, std::bind(&SimulateScenario::handle_create_flight_result_for, this, participant, ph::_1));
}

void cmd::SimulateScenario::handle_create_flight_result_for(util::Scenario::Participants::iterator participant,
                                                            const Flights::CreateFlight::Result& result) {
  if (result) {
    log_.infof(component, "successfully created flight for polygon");
    collector_->collect_flight_id_for(participant, result.value());
    request_traffic_monitoring_for(participant);
    request_start_flight_comms_for(participant);

    context_->schedule_in([this, participant]() {
        client_->flights().end_flight_communications(
          {participant->flight.get().id},
          std::bind(&SimulateScenario::handle_end_flight_comms, this, participant, ph::_1));
      },
      microseconds(collector_->scenario().duration.get().total_microseconds())
    );

  } else {
    log_.errorf(component, "failed to create flight for polygon: %s", result.error());
    context_->stop(::airmap::Context::ReturnCode::error);
  }
}

void cmd::SimulateScenario::request_traffic_monitoring_for(util::Scenario::Participants::iterator participant) {
  client_->traffic().monitor(
      {participant->flight.get().id, participant->authentication.get()},
      std::bind(&SimulateScenario::handle_traffic_monitoring_result_for, this, participant, ph::_1));
}

void cmd::SimulateScenario::handle_traffic_monitoring_result_for(util::Scenario::Participants::iterator participant,
                                                                 const Traffic::Monitor::Result& result) {
  if (result) {
    log_.infof(component, "successfully started monitoring traffic for flight: %s", participant->flight.get().id);
    auto monitor = result.value();
    monitor->subscribe(std::make_shared<Traffic::Monitor::LoggingSubscriber>(component, log_.logger()));
    collector_->collect_traffic_monitor_for(participant, monitor);
  } else {
    log_.errorf(component, "failed to start monitoring traffic for flight %s: %s", participant->flight.get().id,
                result.error());
    context_->stop(::airmap::Context::ReturnCode::error);
  }
}

void cmd::SimulateScenario::request_start_flight_comms_for(util::Scenario::Participants::iterator participant) {
  client_->flights().start_flight_communications(
      {participant->flight.get().id},
      std::bind(&SimulateScenario::handle_start_flight_comms_result_for, this, participant, ph::_1));
}

void cmd::SimulateScenario::handle_start_flight_comms_result_for(
    util::Scenario::Participants::iterator participant, const Flights::StartFlightCommunications::Result& result) {
  if (result) {
    log_.infof(component, "successfully started flight comms");

    if (collector_->collect_key_for(participant, result.value().key)) {
      auto simulator = std::make_shared<util::ScenarioSimulator>(collector_->scenario(), log_.logger());
      runner_->start(
          simulator, [this](const DateTime& now, const util::Scenario::Participant& p, const Geometry::Coordinate& c) {
            if (p.flight && p.encryption_key) {
              Telemetry::Position position;
              position.timestamp           = milliseconds_since_epoch(now);
              position.latitude            = c.latitude;
              position.longitude           = c.longitude;
              position.altitude_msl        = p.altitude_msl ? p.altitude_msl.get() : ::mavlink::altitude_msl;
              position.altitude_gl         = p.altitude_agl ? p.altitude_agl.get() : ::mavlink::altitude_gl;
              position.horizontal_accuracy = 2.;

              Telemetry::Update update{position};
              client_->telemetry().submit_updates(p.flight.get(), p.encryption_key.get(), {update});
            }

            mavlink_message_t msg;
            mavlink_msg_global_position_int_pack(p.id, ::mavlink::component_id, &msg, milliseconds_since_epoch(now),
                                                 c.latitude * 1E7, c.longitude * 1E7, ::mavlink::altitude_msl * 1E3,
                                                 ::mavlink::altitude_gl * 1E3, ::mavlink::vx, ::mavlink::vy,
                                                 ::mavlink::vz, ::mavlink::heading * 1E2);
            router_->route(msg);
          });
    }
  } else {
    log_.errorf(component, "failed to start flight comms for flight %s: %s", participant->flight.get().id,
                result.error());
    context_->stop(::airmap::Context::ReturnCode::error);
  }
}

void cmd::SimulateScenario::handle_end_flight_comms(util::Scenario::Participants::iterator participant,
                                                    const Flights::EndFlightCommunications::Result& result) {
  if (result) {
    log_.infof(component, "successfully ended flight comms");
    client_->flights().end_flight({participant->flight.get().id},
                                  std::bind(&SimulateScenario::handle_end_flight, this, participant, ph::_1));
  } else {
    log_.errorf(component, "failed to end flight comms: %s", result.error());
    context_->stop(::airmap::Context::ReturnCode::error);
  }
}

void cmd::SimulateScenario::handle_end_flight(util::Scenario::Participants::iterator participant,
                                              const Flights::EndFlight::Result& result) {
  if (result) {
    log_.infof(component, "successfully ended flight: %s", participant->flight.get().id);
    context_->stop(::airmap::Context::ReturnCode::success);
  } else {
    log_.errorf(component, "failed to end flight: %s", result.error());
    context_->stop(::airmap::Context::ReturnCode::error);
  }
}
