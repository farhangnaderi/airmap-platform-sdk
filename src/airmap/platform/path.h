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
#ifndef AIRMAP_PLATFORM_PATH_H_
#define AIRMAP_PLATFORM_PATH_H_

#include <filesystem>

namespace airmap {
namespace platform {

using Path = ::std::filesystem::path;
using ::std::filesystem::create_directories;
using ::std::filesystem::current_path;
using ::std::filesystem::exists;

}  // namespace platform
}  // namespace airmap

#endif  // AIRMAP_PLATFORM_PATH_H_
