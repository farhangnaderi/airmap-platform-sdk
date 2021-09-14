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
#ifndef AIRMAP_REST_CLIENT_H_
#define AIRMAP_REST_CLIENT_H_

#include <airmap/client.h>
#include <airmap/context.h>

#include <airmap/net/http/authorized_requester.h>
#include <airmap/net/mqtt/broker.h>

#include <airmap/rest/advisory.h>
#include <airmap/rest/aircrafts.h>
#include <airmap/rest/airspaces.h>
#include <airmap/rest/authenticator.h>
#include <airmap/rest/flight_plans.h>
#include <airmap/rest/flights.h>
#include <airmap/rest/pilots.h>
#include <airmap/rest/rulesets.h>
#include <airmap/rest/status.h>
#include <airmap/rest/telemetry.h>
#include <airmap/rest/traffic.h>

#include <memory>

namespace airmap {
namespace rest {

class Client : public airmap::Client {
 public:
  struct Requesters {
    std::shared_ptr<net::http::Requester> advisory;
    std::shared_ptr<net::http::Requester> aircrafts;
    std::shared_ptr<net::http::Requester> airspaces;
    std::shared_ptr<net::http::Requester> authenticator;
    std::shared_ptr<net::http::Requester> flight_plans;
    std::shared_ptr<net::http::Requester> flights;
    std::shared_ptr<net::http::Requester> pilots;
    std::shared_ptr<net::http::Requester> rulesets;
    std::shared_ptr<net::http::Requester> status;
    std::shared_ptr<net::http::Requester> sso;
  };

  explicit Client(const Configuration& configuration, const std::shared_ptr<Context>& parent,
                  const std::shared_ptr<net::udp::Sender>& sender, const Requesters& requesters,
                  const std::shared_ptr<net::mqtt::Broker>& broker);
  ~Client();

  // From airmap::Client
  airmap::Advisory& advisory() override;
  airmap::Aircrafts& aircrafts() override;
  airmap::Airspaces& airspaces() override;
  airmap::Authenticator& authenticator() override;
  airmap::FlightPlans& flight_plans() override;
  airmap::Flights& flights() override;
  airmap::Pilots& pilots() override;
  airmap::RuleSets& rulesets() override;
  airmap::Status& status() override;
  airmap::Telemetry& telemetry() override;
  airmap::Traffic& traffic() override;
  void handle_auth_update(std::string token);

 private:
  Configuration configuration_;
  std::shared_ptr<Context> parent_;
  Optional<std::string> auth_token_;
  boost::signals2::connection auth_connection_;
  std::shared_ptr<net::udp::Sender> udp_sender_;
  struct {
    std::shared_ptr<net::http::Requester> airmap_requester;
    std::shared_ptr<net::http::Requester> sso_requester;
  } http_;
  std::shared_ptr<net::mqtt::Broker> mqtt_broker_;

  rest::Advisory advisory_;
  rest::Aircrafts aircrafts_;
  rest::Airspaces airspaces_;
  rest::Authenticator authenticator_;
  rest::FlightPlans flight_plans_;
  rest::Flights flights_;
  rest::Pilots pilots_;
  rest::RuleSets rulesets_;
  rest::Status status_;
  rest::Telemetry telemetry_;
  rest::Traffic traffic_;
};

}  // namespace rest
}  // namespace airmap

#endif  // AIRMAP_REST_CLIENT_H_
