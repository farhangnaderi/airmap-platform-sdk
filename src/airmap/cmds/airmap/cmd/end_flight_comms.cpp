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
#include <airmap/cmds/airmap/cmd/end_flight_comms.h>

#include <airmap/client.h>
#include <airmap/codec.h>
#include <airmap/context.h>
#include <airmap/paths.h>
#include <airmap/rest/client.h>
#include <airmap/util/telemetry_simulator.h>

#include <signal.h>

#include <fstream>
#include <iterator>
#include <thread>

namespace cli = airmap::util::cli;
namespace cmd = airmap::cmds::airmap::cmd;

using json = nlohmann::json;

namespace {
constexpr const char* component{"end-flight-comms"};
}

cmd::EndFlightComms::EndFlightComms()
    : cli::CommandWithFlagsAndAction{"end-flight-comms", "ends injection of telemetry data",
                                     "ends injection of telemetry data"} {
  flag(flags::version(params_.version));
  flag(flags::log_level(params_.log_level));
  flag(flags::config_file(params_.config_file));
  flag(flags::flight_id(params_.flight_id));

  action([this](const cli::Command::Context& ctxt) {
    log_ = util::FormattingLogger{create_filtering_logger(params_.log_level, create_default_logger(ctxt.cerr))};

    if (!params_.config_file) {
      params_.config_file = ConfigFile{paths::config_file(params_.version).string()};
    }

    std::ifstream in_config{params_.config_file.get()};
    if (!in_config) {
      log_.errorf(component, "failed to open configuration file %s for reading", params_.config_file);
      return 1;
    }

    if (!params_.flight_id) {
      log_.errorf(component, "missing parameter 'flight-id'");
      return 1;
    }

    if (!params_.flight_id.get().validate()) {
      log_.errorf(component, "parameter 'flight-id' for accessing AirMap services must not be empty");
      return 1;
    }

    auto result = ::airmap::Context::create(log_.logger());

    if (!result) {
      log_.errorf(component, "failed to acquire resources for accessing AirMap services");
      return 1;
    }

    auto context = result.value();
    auto config  = Client::load_configuration_from_json(in_config);

    log_.infof(component,
               "client configuration:\n"
               "  host:                %s\n"
               "  version:             %s\n"
               "  telemetry.host:      %s\n"
               "  telemetry.port:      %d\n"
               "  credentials.api_key: %s",
               config.host, config.version, config.telemetry.host, config.telemetry.port, config.credentials.api_key);

    context->create_client_with_configuration(
        config, [this, &ctxt, context](const ::airmap::Context::ClientCreateResult& result) {
          if (not result) {
            log_.errorf(component, "failed to create client: %s", result.error());
            context->stop(::airmap::Context::ReturnCode::error);
            return;
          }

          auto client = result.value();
          client->flights().end_flight_communications(
              Flights::EndFlightCommunications::Parameters{params_.flight_id.get()},
              [this, &ctxt, context, client](const Flights::EndFlightCommunications::Result& result) {
                if (!result) {
                  log_.errorf(component, "failed to end flight communications: %s", result.error());
                  context->stop(::airmap::Context::ReturnCode::error);
                  return;
                }

                log_.infof(component, "successfully ended flight communications");
                context->stop();
              });
        });

    return context->exec({SIGINT, SIGQUIT},
                         [this, context](int sig) {
                           log_.infof(component, "received [%s], shutting down", ::strsignal(sig));
                           context->stop();
                         }) == ::airmap::Context::ReturnCode::success
               ? 0
               : 1;
  });
}
