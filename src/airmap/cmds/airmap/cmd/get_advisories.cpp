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
#include <airmap/cmds/airmap/cmd/get_advisories.h>

#include <airmap/client.h>
#include <airmap/codec.h>
#include <airmap/context.h>
#include <airmap/date_time.h>
#include <airmap/paths.h>
#include <airmap/rest/client.h>

#include <signal.h>

namespace cli = airmap::util::cli;
namespace cmd = airmap::cmds::airmap::cmd;

using json = nlohmann::json;

namespace {

constexpr const char* component{"get-advisories"};

void print_advisories(std::ostream& out, const std::vector<airmap::Advisory::AirspaceAdvisory>& v) {
  cli::TabWriter tw;

  tw << "id"
     << "name"
     << "type"
     << "color";

  for (const auto& a : v) {
    tw << cli::TabWriter::NewLine{};
    tw << a.advisory.airspace.id() << a.advisory.airspace.name() << a.advisory.airspace.type() << a.advisory.color;
  }

  tw << cli::TabWriter::NewLine{};
  tw.flush(out);
}

}  // namespace

cmd::GetAdvisories::GetAdvisories()
    : cli::CommandWithFlagsAndAction{"get-advisories",
                                     "searches for advisories by geometry or flight plan id from the AirMap services",
                                     "searches for advisories by geometry or flight plan id from the AirMap services"} {
  flag(flags::version(version_));
  flag(flags::log_level(log_level_));
  flag(flags::config_file(config_file_));
  flag(cli::make_flag("geometry-file", "use the polygon defined in this geojson file", geometry_file_));
  flag(cli::make_flag("flight-plan-id", "id of flight plan", flight_plan_id_));
  flag(cli::make_flag("rulesets", "comma-separated list of rulesets", rulesets_));
  flag(cli::make_flag("start", "planned start time of flight", start_));
  flag(cli::make_flag("end", "planned end time of flight", end_));

  action([this](const cli::Command::Context& ctxt) {
    log_ = util::FormattingLogger{create_filtering_logger(log_level_, create_default_logger(ctxt.cerr))};

    if (!config_file_) {
      config_file_ = ConfigFile{paths::config_file(version_).string()};
    }

    std::ifstream in_config{config_file_.get()};
    if (!in_config) {
      log_.errorf(component, "failed to open configuration file %s for reading", config_file_);
      return 1;
    }

    if (!token_file_) {
      token_file_ = TokenFile{paths::token_file(version_).string()};
    }

    std::ifstream in_token{token_file_.get()};
    Optional<Token> token = Optional<Token>();
    if (!in_token) {
      log_.errorf(component, "failed to open token file %s for reading, not using token for advisory search", token_file_);
    }
    else {
      token = Token::load_from_json(in_token);
    }

    if (!flight_plan_id_ && (!geometry_file_ || !rulesets_)) {
      log_.errorf(component, "missing parameter 'flight-plan-id' and either 'geometry-file' or 'rulesets'");
      return 1;
    }

    auto result = ::airmap::Context::create(log_.logger());

    if (!result) {
      log_.errorf(component, "failed to acquire resources for accessing AirMap services");
      return 1;
    }

    context_    = result.value();
    auto config = Client::load_configuration_from_json(in_config);

    log_.infof(component,
               "client configuration:\n"
               "  host:                %s\n"
               "  version:             %s\n"
               "  telemetry.host:      %s\n"
               "  telemetry.port:      %d\n"
               "  credentials.api_key: %s\n",
               config.host, config.version, config.telemetry.host, config.telemetry.port, config.credentials.api_key);

    context_->create_client_with_configuration(
        config, [this, &ctxt, token](const ::airmap::Context::ClientCreateResult& result) {
          if (not result) {
            log_.errorf(component, "failed to create client: %s", result.error());
            context_->stop(::airmap::Context::ReturnCode::error);
            return;
          }

          client_ = result.value();
          auto c = dynamic_cast<::airmap::rest::Client*>(client_.get());
          if (c && token) {
            c->handle_auth_update(token.get().id());
          }

          if (flight_plan_id_) {
            Advisory::ForId::Parameters params;
            params.id = flight_plan_id_.get();
            if (start_ && end_) {
              params.start = iso8601::parse(start_.get());
              params.end   = iso8601::parse(end_.get());
            } else {
              params.start = DateTime(Clock::universal_time().date());
              params.end   = params.start;
            }
            client_->advisory().for_id(params, std::bind(&GetAdvisories::handle_advisory_for_id_result, this,
                                                         std::placeholders::_1, std::ref(ctxt)));
          } else if (geometry_file_ && rulesets_) {
            std::ifstream in{geometry_file_.get()};
            if (!in) {
              log_.errorf(component, "failed to open %s for reading", geometry_file_.get());
              return;
            }
            Geometry geometry = json::parse(in);
            Advisory::Search::Parameters params;
            params.geometry = geometry;
            params.rulesets = rulesets_.get();
            if (start_ && end_) {
              params.start = iso8601::parse(start_.get());
              params.end   = iso8601::parse(end_.get());
            } else {
              params.start = DateTime(Clock::universal_time().date());
              params.end   = params.start;
            }
            client_->advisory().search(params, std::bind(&GetAdvisories::handle_advisory_search_result, this,
                                                         std::placeholders::_1, std::ref(ctxt)));
          }
        });

    return context_->exec({SIGINT, SIGQUIT},
                          [this](int sig) {
                            log_.infof(component, "received [%s], shutting down", ::strsignal(sig));
                            context_->stop();
                          }) == ::airmap::Context::ReturnCode::success
               ? 0
               : 1;
  });
}

void cmd::GetAdvisories::handle_advisory_for_id_result(const Advisory::ForId::Result& result, ConstContextRef context) {
  if (result) {
    log_.infof(component, "successfully obtained advisories");
    print_advisories(context.get().cout, result.value());
    context_->stop();
  } else {
    log_.errorf(component, "failed to obtain advisories: %s", result.error());
    context_->stop(::airmap::Context::ReturnCode::error);
    return;
  }
}

void cmd::GetAdvisories::handle_advisory_search_result(const Advisory::Search::Result& result,
                                                       ConstContextRef context) {
  if (result) {
    log_.infof(component, "successfully obtained advisories");
    print_advisories(context.get().cout, result.value());
    context_->stop();
  } else {
    log_.errorf(component, "failed to obtain advisories: %s", result.error());
    context_->stop(::airmap::Context::ReturnCode::error);
    return;
  }
}
