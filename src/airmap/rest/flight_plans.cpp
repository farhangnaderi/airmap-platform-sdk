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
#include <airmap/rest/flight_plans.h>

#include <airmap/codec.h>
#include <airmap/jsend.h>
#include <airmap/net/http/middleware.h>
#include <airmap/net/http/authorized_requester.h>
#include <airmap/util/fmt.h>

#include <nlohmann/json.hpp>

namespace fmt = airmap::util::fmt;
using json    = nlohmann::json;

std::string airmap::rest::FlightPlans::default_route_for_version(Client::Version version) {
  static constexpr const char* pattern{"/flight/%s"};

  switch (version) {
    case airmap::Client::Version::production:
      return fmt::sprintf(pattern, "v2");
    case airmap::Client::Version::staging:
      return fmt::sprintf(pattern, "stage");
  }

  throw std::logic_error{"should not reach here"};
}

airmap::rest::FlightPlans::FlightPlans(const std::shared_ptr<net::http::Requester>& requester) : requester_{requester} {
}

void airmap::rest::FlightPlans::for_id(const ForId::Parameters& parameters, const ForId::Callback& cb) {
  std::unordered_map<std::string, std::string> query, headers;

  requester_->get(fmt::sprintf("/plan/%s", parameters.id), std::move(query), std::move(headers),
                  net::http::jsend_parsing_request_callback<FlightPlan>(cb));
}

void airmap::rest::FlightPlans::create_by_polygon(const Create::Parameters& parameters, const Create::Callback& cb) {
  std::unordered_map<std::string, std::string> headers;

  json j = parameters;

  requester_->post("/plan", std::move(headers), j.dump(), net::http::jsend_parsing_request_callback<FlightPlan>(cb));
}

void airmap::rest::FlightPlans::update(const Update::Parameters& parameters, const Update::Callback& cb) {
  std::unordered_map<std::string, std::string> headers;

  json j;
  j = parameters.flight_plan;

  requester_->patch(fmt::sprintf("/plan/%s", parameters.flight_plan.id), std::move(headers), j.dump(),
                    net::http::jsend_parsing_request_callback<FlightPlan>(cb));
}

void airmap::rest::FlightPlans::delete_(const Delete::Parameters& parameters, const Delete::Callback& cb) {
  std::unordered_map<std::string, std::string> query, headers;

  requester_->delete_(fmt::sprintf("/plan/%s", parameters.id), std::move(query), std::move(headers),
                      net::http::jsend_parsing_request_callback<Delete::Response>(cb));
}

void airmap::rest::FlightPlans::render_briefing(const RenderBriefing::Parameters& parameters,
                                                const RenderBriefing::Callback& cb) {
  std::unordered_map<std::string, std::string> query, headers;

  requester_->get(fmt::sprintf("/plan/%s/briefing", parameters.id), std::move(query), std::move(headers),
                  net::http::jsend_parsing_request_callback<FlightPlan::Briefing>(cb));
}

void airmap::rest::FlightPlans::submit(const Submit::Parameters& parameters, const Submit::Callback& cb) {
  std::unordered_map<std::string, std::string> headers;

  requester_->post(fmt::sprintf("/plan/%s/submit", parameters.id), std::move(headers), std::string{},
                   net::http::jsend_parsing_request_callback<FlightPlan>(cb));
}

void airmap::rest::FlightPlans::set_auth_token(std::string token) {
  airmap::net::http::AuthorizedRequester *auth_requester = dynamic_cast<airmap::net::http::AuthorizedRequester*>(requester_.get());
  if (auth_requester) {
    auth_requester->set_auth_token(token);
  }
}
