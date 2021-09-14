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
#include <airmap/cmds/airmap/cmd/fetch_rules.h>

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

constexpr const char* component{"fetch-rules"};

void print_rulesets(std::ostream& out, const std::vector<airmap::RuleSet>& v) {
  cli::TabWriter tw;
  tw << "id"
     << "# rules" << cli::TabWriter::NewLine{};
  for (const auto& r : v)
    tw << r.id << r.rules.size() << cli::TabWriter::NewLine{};
  tw.flush(out);
}

}  // namespace

cmd::FetchRules::FetchRules()
    : cli::CommandWithFlagsAndAction{
          "fetch-rules", "fetches the rules that apply to the rulesets",
          "fetches the rules that apply to the rulesets as well as the corresponding flight features"} {
  flag(flags::version(version_));
  flag(flags::log_level(log_level_));
  flag(flags::config_file(config_file_));
  flag(cli::make_flag("rulesets", "comma-separated list of rulesets", rulesets_));

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
    if (!in_token) {
      log_.errorf(component, "failed to open token file %s for reading", token_file_);
      return 1;
    }

    token_ = Token::load_from_json(in_token);

    if (!rulesets_) {
      log_.errorf(component, "missing parameter 'rulesets'");
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
               "  credentials.api_key: %s\n",
               config.host, config.version, config.telemetry.host, config.telemetry.port, config.credentials.api_key);

    context->create_client_with_configuration(
        config, [this, &ctxt, context](const ::airmap::Context::ClientCreateResult& result) {
          if (not result) {
            log_.errorf(component, "failed to create client: %s", result.error());
            context->stop(::airmap::Context::ReturnCode::error);
            return;
          }

          auto client = result.value();
          auto c = dynamic_cast<::airmap::rest::Client*>(client.get());
          if (c && token_) {
            c->handle_auth_update(token_.get().id());
          }
          
          auto handler = [this, &ctxt, context, client](const RuleSets::FetchRules::Result& result) {
            if (result) {
              log_.infof(component, "succesfully obtained rules for list of rulesets\n");
              print_rulesets(ctxt.cout, result.value());
              context->stop();
            } else {
              log_.errorf(component, "failed to obtain rules: %s", result.error());
              context->stop(::airmap::Context::ReturnCode::error);
              return;
            }
          };

          params_.rulesets = rulesets_.get();
          client->rulesets().fetch_rules(params_, handler);
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
