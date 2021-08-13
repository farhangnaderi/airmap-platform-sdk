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
#include <airmap/rest/client.h>

airmap::rest::Client::Client(const Configuration& configuration, const std::shared_ptr<Context>& parent,
                             const std::shared_ptr<net::udp::Sender>& sender, const Requesters& requesters,
                             const std::shared_ptr<net::mqtt::Broker>& broker)
    : configuration_{configuration},
      parent_{parent},
      udp_sender_{sender},
      mqtt_broker_{broker},
      authenticator_{std::make_shared<airmap::net::http::AuthorizedRequester>(configuration_.credentials.api_key,
                                                                              requesters.authenticator,
                                                                              nullptr),
                                                                              requesters.sso},
      advisory_{std::make_shared<airmap::net::http::AuthorizedRequester>(configuration_.credentials.api_key,
                                                                         requesters.advisory,
                                                                         &authenticator_)},
      aircrafts_{std::make_shared<airmap::net::http::AuthorizedRequester>(configuration_.credentials.api_key,
                                                                          requesters.aircrafts,
                                                                          &authenticator_)},
      airspaces_{std::make_shared<airmap::net::http::AuthorizedRequester>(configuration_.credentials.api_key,
                                                                          requesters.airspaces,
                                                                          &authenticator_)},
      flight_plans_{std::make_shared<airmap::net::http::AuthorizedRequester>(configuration_.credentials.api_key,
                                                                             requesters.flight_plans,
                                                                             &authenticator_)},
      flights_{std::make_shared<airmap::net::http::AuthorizedRequester>(configuration_.credentials.api_key,
                                                                        requesters.flights,
                                                                        &authenticator_)},
      pilots_{std::make_shared<airmap::net::http::AuthorizedRequester>(configuration_.credentials.api_key,
                                                                       requesters.pilots,
                                                                       &authenticator_)},
      rulesets_{std::make_shared<airmap::net::http::AuthorizedRequester>(configuration_.credentials.api_key,
                                                                         requesters.rulesets,
                                                                         &authenticator_)},
      status_{std::make_shared<airmap::net::http::AuthorizedRequester>(configuration_.credentials.api_key,
                                                                       requesters.status,
                                                                       &authenticator_)},
      telemetry_{std::make_shared<detail::OpenSSLAES256Encryptor>(), udp_sender_},
      traffic_{mqtt_broker_} {
}

airmap::rest::Client::~Client() {
}

airmap::Advisory& airmap::rest::Client::advisory() {
  return advisory_;
}

airmap::Aircrafts& airmap::rest::Client::aircrafts() {
  return aircrafts_;
}

airmap::Airspaces& airmap::rest::Client::airspaces() {
  return airspaces_;
}

airmap::Authenticator& airmap::rest::Client::authenticator() {
  return authenticator_;
}

airmap::FlightPlans& airmap::rest::Client::flight_plans() {
  return flight_plans_;
}

airmap::Flights& airmap::rest::Client::flights() {
  return flights_;
}

airmap::Pilots& airmap::rest::Client::pilots() {
  return pilots_;
}

airmap::RuleSets& airmap::rest::Client::rulesets() {
  return rulesets_;
}

airmap::Status& airmap::rest::Client::status() {
  return status_;
}

airmap::Telemetry& airmap::rest::Client::telemetry() {
  return telemetry_;
}

airmap::Traffic& airmap::rest::Client::traffic() {
  return traffic_;
}
