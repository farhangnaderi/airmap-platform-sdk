// AirMap Platform SDK
// Copyright © 2021 AirMap, Inc. All rights reserved.
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
#ifndef AIRMAP_NET_HTTP_JWT_PROVIDER_H_
#define AIRMAP_NET_HTTP_JWT_PROVIDER_H_

#include <airmap/optional.h>

namespace airmap {
namespace net {
namespace http {

class JWTProvider {
public:
    virtual Optional<std::string> jwt_string() = 0;
};

}  // namespace http
}  // namespace net
}  // namespace airmap

#endif  // AIRMAP_NET_HTTP_JWT_PROVIDER_H_
