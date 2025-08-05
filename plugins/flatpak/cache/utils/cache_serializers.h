/*
 * Copyright 2020-2024 Toyota Connected North America
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef PLUGINS_FLATPAK_CACHE_SERIALIZERS_H
#define PLUGINS_FLATPAK_CACHE_SERIALIZERS_H

#include <flutter/encodable_value.h>
#include <string>
#include "../../messages.g.h"

namespace flatpak_plugin {
namespace cache_utils {

/**
 * @brief Utility class for serializing/deserializing Installation objects
 */
class InstallationSerializer {
 public:
  /**
   * @brief Serialize Installation to JSON string
   * @param installation The installation object to serialize
   * @return JSON string representation
   */
  static std::string Serialize(const Installation& installation);

  /**
   * @brief Deserialize Installation from JSON string
   * @param json_str The JSON string to deserialize
   * @return Installation object or nullopt if deserialization fails
   */
  static std::optional<Installation> Deserialize(const std::string& json_str);

 private:
  static flutter::EncodableMap ToEncodableMap(const Installation& installation);
  static std::optional<Installation> FromEncodableMap(
      const flutter::EncodableMap& map);
};

/**
 * @brief Utility class for serializing/deserializing Remote objects
 */
class RemoteSerializer {
 public:
  /**
   * @brief Serialize Remote to JSON string
   * @param remote The remote object to serialize
   * @return JSON string representation
   */
  static std::string Serialize(const Remote& remote);

  /**
   * @brief Deserialize Remote from JSON string
   * @param json_str The JSON string to deserialize
   * @return Remote object or nullopt if deserialization fails
   */
  static std::optional<Remote> Deserialize(const std::string& json_str);

 private:
  static flutter::EncodableMap ToEncodableMap(const Remote& remote);
  static std::optional<Remote> FromEncodableMap(
      const flutter::EncodableMap& map);
};

/**
 * @brief Utility class for serializing/deserializing EncodableList objects
 */
class EncodableListSerializer {
 public:
  /**
   * @brief Serialize EncodableList to JSON string
   * @param list The list to serialize
   * @return JSON string representation
   */
  static std::string Serialize(const flutter::EncodableList& list);

  /**
   * @brief Deserialize EncodableList from JSON string
   * @param json_str The JSON string to deserialize
   * @return EncodableList or nullopt if deserialization fails
   */
  static std::optional<flutter::EncodableList> Deserialize(
      const std::string& json_str);
};

class ApplicationSerializer {
 public:
  static std::string Serialize(const Application& application);
  static std::optional<Application> Deserialize(const std::string& hex_str);

 private:
  static flutter::EncodableMap ToEncodableMap(const Application& application);
  static std::optional<Application> FromEncodableMap(
      const flutter::EncodableMap& map);
};

}  // namespace cache_utils
}  // namespace flatpak_plugin

#endif  // PLUGINS_FLATPAK_CACHE_SERIALIZERS_H