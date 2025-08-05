#include "cache_serializers.h"
#include <flutter/standard_message_codec.h>
#include <spdlog/spdlog.h>
#include <algorithm>
#include <cctype>
#include <iomanip>
#include <sstream>
#include <stack>


namespace flatpak_plugin::cache_utils {

bool ValidateEncodableValue(const flutter::EncodableValue& value,
                            const std::string& context = "") {

  struct ValidationItem {
    const flutter::EncodableValue* value;
    std::string context;
  };

  std::stack<ValidationItem> validation_stack;
  validation_stack.push({&value, context});

  auto check_simple_type = [](const flutter::EncodableValue& val, const std::string& ctx) -> bool {
    if (std::holds_alternative<std::monostate>(val)) {
      spdlog::debug("EncodableValue is null/monostate in {}", ctx);
      return true;
    } else if (std::holds_alternative<bool>(val)) {
      spdlog::debug("EncodableValue is bool in {}", ctx);
      return true;
    } else if (std::holds_alternative<int32_t>(val)) {
      spdlog::debug("EncodableValue is int32_t in {}", ctx);
      return true;
    } else if (std::holds_alternative<int64_t>(val)) {
      spdlog::debug("EncodableValue is int64_t in {}", ctx);
      return true;
    } else if (std::holds_alternative<double>(val)) {
      spdlog::debug("EncodableValue is double in {}", ctx);
      return true;
    } else if (std::holds_alternative<std::string>(val)) {
      spdlog::debug("EncodableValue is string in {}", ctx);
      return true;
    } else if (std::holds_alternative<std::vector<uint8_t>>(val)) {
      spdlog::debug("EncodableValue is uint8_t vector in {}", ctx);
      return true;
    } else if (std::holds_alternative<std::vector<int32_t>>(val)) {
      spdlog::debug("EncodableValue is int32_t vector in {}", ctx);
      return true;
    } else if (std::holds_alternative<std::vector<int64_t>>(val)) {
      spdlog::debug("EncodableValue is int64_t vector in {}", ctx);
      return true;
    } else if (std::holds_alternative<std::vector<double>>(val)) {
      spdlog::debug("EncodableValue is double vector in {}", ctx);
      return true;
    }
    return false;
  };

  try {
    while (!validation_stack.empty()) {
      auto current = validation_stack.top();
      validation_stack.pop();

      const auto& current_value = *current.value;
      const auto& current_context = current.context;

      if (check_simple_type(current_value, current_context)) {
        continue;
      } else if (std::holds_alternative<flutter::EncodableList>(current_value)) {
        spdlog::debug("EncodableValue is EncodableList in {}", current_context);
        const auto& list = std::get<flutter::EncodableList>(current_value);

        for (int i = static_cast<int>(list.size()) - 1; i >= 0; --i) {
          validation_stack.push({
            &list[i],
            current_context + "[" + std::to_string(i) + "]"
          });
        }
      } else if (std::holds_alternative<flutter::EncodableMap>(current_value)) {
        spdlog::debug("EncodableValue is EncodableMap in {}", current_context);
        const auto& map = std::get<flutter::EncodableMap>(current_value);

        for (const auto& [key, val] : map) {
          validation_stack.push({&key, current_context + ".key"});
          validation_stack.push({&val, current_context + ".value"});
        }
      } else {
        spdlog::error("Unknown EncodableValue type in {}", current_context);
        return false;
      }
    }

    return true;

  } catch (const std::exception& e) {
    spdlog::error("Exception validating EncodableValue in {}: {}", context, e.what());
    return false;
  }
}

bool ValidateEncodableValueForSerialization(
    const flutter::EncodableValue& value,
    const std::string& context = "") {

  // Use iterative approach with a stack to avoid recursion
  struct ValidationItem {
    const flutter::EncodableValue* value;
    std::string context;
  };

  std::stack<ValidationItem> validation_stack;
  validation_stack.push({&value, context});

  // Helper lambda to check if a value is a simple supported type
  auto is_simple_supported_type = [](const flutter::EncodableValue& val) {
    return std::holds_alternative<std::monostate>(val) ||
           std::holds_alternative<bool>(val) ||
           std::holds_alternative<int32_t>(val) ||
           std::holds_alternative<int64_t>(val) ||
           std::holds_alternative<double>(val) ||
           std::holds_alternative<std::string>(val) ||
           std::holds_alternative<std::vector<uint8_t>>(val) ||
           std::holds_alternative<std::vector<int32_t>>(val) ||
           std::holds_alternative<std::vector<int64_t>>(val) ||
           std::holds_alternative<std::vector<double>>(val);
  };

  try {
    while (!validation_stack.empty()) {
      auto current = validation_stack.top();
      validation_stack.pop();

      const auto& current_value = *current.value;
      const auto& current_context = current.context;

      if (is_simple_supported_type(current_value)) {

      } else if (std::holds_alternative<flutter::EncodableList>(current_value)) {
        const auto& list = std::get<flutter::EncodableList>(current_value);

        for (int i = static_cast<int>(list.size()) - 1; i >= 0; --i) {
          validation_stack.push({
            &list[i],
            current_context + "[" + std::to_string(i) + "]"
          });
        }
      } else if (std::holds_alternative<flutter::EncodableMap>(current_value)) {
        const auto& map = std::get<flutter::EncodableMap>(current_value);

        for (const auto& [key, val] : map) {
          validation_stack.push({&key, current_context + ".key"});
          validation_stack.push({&val, current_context + ".value"});
        }
      } else {
        spdlog::error(
            "Unsupported EncodableValue type in {} - this will cause "
            "serialization failure",
            current_context);
        return false;
      }
    }

    return true; // All items validated successfully

  } catch (const std::exception& e) {
    spdlog::error("Exception validating EncodableValue in {}: {}", context, e.what());
    return false;
  }
}

flutter::EncodableMap ApplicationToEncodableMap(const Application& app) {
  return flutter::EncodableMap{
      {"name", flutter::EncodableValue(app.name())},
      {"id", flutter::EncodableValue(app.id())},
      {"summary", flutter::EncodableValue(app.summary())},
      {"version", flutter::EncodableValue(app.version())},
      {"origin", flutter::EncodableValue(app.origin())},
      {"license", flutter::EncodableValue(app.license())},
      {"installed_size", flutter::EncodableValue(app.installed_size())},
      {"deploy_dir", flutter::EncodableValue(app.deploy_dir())},
      {"is_current", flutter::EncodableValue(app.is_current())},
      {"content_rating_type",
       flutter::EncodableValue(app.content_rating_type())},
      {"content_rating", flutter::EncodableValue(app.content_rating())},
      {"latest_commit", flutter::EncodableValue(app.latest_commit())},
      {"eol", flutter::EncodableValue(app.eol())},
      {"eol_rebase", flutter::EncodableValue(app.eol_rebase())},
      {"subpaths", flutter::EncodableValue(app.subpaths())},
      {"metadata", flutter::EncodableValue(app.metadata())},
      {"appdata", flutter::EncodableValue(app.appdata())}};
}

std::string InstallationSerializer::Serialize(
    const Installation& installation) {
  try {
    auto map = ToEncodableMap(installation);
    auto& codec = flutter::StandardMessageCodec::GetInstance();
    auto encoded = codec.EncodeMessage(flutter::EncodableValue(map));

    // Convert to base64 or hex string for storage
    std::string result;
    result.reserve(encoded->size() * 2);
    for (const auto& byte : *encoded) {
      std::ostringstream oss;
      oss << std::hex << std::setfill('0') << std::setw(2)
          << static_cast<int>(static_cast<unsigned char>(byte));
      result += oss.str();
    }
    return result;
  } catch (const std::exception& e) {
    spdlog::error("Failed to serialize Installation: {}", e.what());
    return "";
  }
}

std::optional<Installation> InstallationSerializer::Deserialize(
    const std::string& json_str) {
  if (json_str.empty()) {
    return std::nullopt;
  }

  try {
    // Convert hex string back to bytes
    std::vector<uint8_t> bytes;
    bytes.reserve(json_str.size() / 2);
    for (size_t i = 0; i < json_str.size(); i += 2) {
      std::string hex_byte = json_str.substr(i, 2);
      auto byte = static_cast<uint8_t>(std::stoi(hex_byte, nullptr, 16));
      bytes.push_back(byte);
    }

    auto& codec = flutter::StandardMessageCodec::GetInstance();
    auto decoded = codec.DecodeMessage(bytes);

    if (decoded && std::holds_alternative<flutter::EncodableMap>(*decoded)) {
      auto map = std::get<flutter::EncodableMap>(*decoded);
      return FromEncodableMap(map);
    }
    return std::nullopt;
  } catch (const std::exception& e) {
    spdlog::error("Failed to deserialize Installation: {}", e.what());
    return std::nullopt;
  }
}

flutter::EncodableMap InstallationSerializer::ToEncodableMap(
    const Installation& installation) {
  return flutter::EncodableMap{
      {"id", flutter::EncodableValue(installation.id())},
      {"display_name", flutter::EncodableValue(installation.display_name())},
      {"path", flutter::EncodableValue(installation.path())},
      {"no_interaction",
       flutter::EncodableValue(installation.no_interaction())},
      {"is_user", flutter::EncodableValue(installation.is_user())},
      {"priority", flutter::EncodableValue(installation.priority())},
      {"default_languages",
       flutter::EncodableValue(installation.default_lanaguages())},
      {"default_locale",
       flutter::EncodableValue(installation.default_locale())},
      {"remotes", flutter::EncodableValue(installation.remotes())}};
}

std::optional<Installation> InstallationSerializer::FromEncodableMap(
    const flutter::EncodableMap& map) {
  try {
    auto get_string = [&map](const std::string& key) -> std::string {
      auto it = map.find(flutter::EncodableValue(key));
      if (it != map.end() && std::holds_alternative<std::string>(it->second)) {
        return std::get<std::string>(it->second);
      }
      return "";
    };

    auto get_bool = [&map](const std::string& key) -> bool {
      auto it = map.find(flutter::EncodableValue(key));
      if (it != map.end() && std::holds_alternative<bool>(it->second)) {
        return std::get<bool>(it->second);
      }
      return false;
    };

    auto get_int64 = [&map](const std::string& key) -> int64_t {
      auto it = map.find(flutter::EncodableValue(key));
      if (it != map.end() && std::holds_alternative<int64_t>(it->second)) {
        return std::get<int64_t>(it->second);
      }
      return 0;
    };

    auto get_list = [&map](const std::string& key) -> flutter::EncodableList {
      auto it = map.find(flutter::EncodableValue(key));
      if (it != map.end() &&
          std::holds_alternative<flutter::EncodableList>(it->second)) {
        return std::get<flutter::EncodableList>(it->second);
      }
      return flutter::EncodableList{};
    };

    return Installation(get_string("id"), get_string("display_name"),
                        get_string("path"), get_bool("no_interaction"),
                        get_bool("is_user"), get_int64("priority"),
                        get_list("default_languages"),
                        get_list("default_locale"), get_list("remotes"));
  } catch (const std::exception& e) {
    spdlog::error("Failed to create Installation from map: {}", e.what());
    return std::nullopt;
  }
}

std::string RemoteSerializer::Serialize(const Remote& remote) {
  try {
    auto map = ToEncodableMap(remote);
    auto& codec = flutter::StandardMessageCodec::GetInstance();
    auto encoded = codec.EncodeMessage(flutter::EncodableValue(map));

    std::string result;
    result.reserve(encoded->size() * 2);
    for (const auto& byte : *encoded) {
      std::ostringstream oss;
      oss << std::hex << std::setfill('0') << std::setw(2)
          << static_cast<int>(static_cast<unsigned char>(byte));
      result += oss.str();
    }
    return result;
  } catch (const std::exception& e) {
    spdlog::error("Failed to serialize Remote: {}", e.what());
    return "";
  }
}

std::optional<Remote> RemoteSerializer::Deserialize(
    const std::string& json_str) {
  if (json_str.empty()) {
    return std::nullopt;
  }

  try {
    std::vector<uint8_t> bytes;
    bytes.reserve(json_str.size() / 2);
    for (size_t i = 0; i < json_str.size(); i += 2) {
      std::string hex_byte = json_str.substr(i, 2);
      auto byte = static_cast<uint8_t>(std::stoi(hex_byte, nullptr, 16));
      bytes.push_back(byte);
    }

    auto& codec = flutter::StandardMessageCodec::GetInstance();
    auto decoded = codec.DecodeMessage(bytes);

    if (decoded && std::holds_alternative<flutter::EncodableMap>(*decoded)) {
      auto map = std::get<flutter::EncodableMap>(*decoded);
      return FromEncodableMap(map);
    }
    return std::nullopt;
  } catch (const std::exception& e) {
    spdlog::error("Failed to deserialize Remote: {}", e.what());
    return std::nullopt;
  }
}

flutter::EncodableMap RemoteSerializer::ToEncodableMap(const Remote& remote) {
  return flutter::EncodableMap{
      {"name", flutter::EncodableValue(remote.name())},
      {"url", flutter::EncodableValue(remote.url())},
      {"collection_id", flutter::EncodableValue(remote.collection_id())},
      {"title", flutter::EncodableValue(remote.title())},
      {"comment", flutter::EncodableValue(remote.comment())},
      {"description", flutter::EncodableValue(remote.description())},
      {"homepage", flutter::EncodableValue(remote.homepage())},
      {"icon", flutter::EncodableValue(remote.icon())},
      {"default_branch", flutter::EncodableValue(remote.default_branch())},
      {"main_ref", flutter::EncodableValue(remote.main_ref())},
      {"remote_type", flutter::EncodableValue(remote.remote_type())},
      {"filter", flutter::EncodableValue(remote.filter())},
      {"appstream_timestamp",
       flutter::EncodableValue(remote.appstream_timestamp())},
      {"appstream_dir", flutter::EncodableValue(remote.appstream_dir())},
      {"gpg_verify", flutter::EncodableValue(remote.gpg_verify())},
      {"no_enumerate", flutter::EncodableValue(remote.no_enumerate())},
      {"no_deps", flutter::EncodableValue(remote.no_deps())},
      {"disabled", flutter::EncodableValue(remote.disabled())},
      {"prio", flutter::EncodableValue(remote.prio())}};
}

std::optional<Remote> RemoteSerializer::FromEncodableMap(
    const flutter::EncodableMap& map) {
  try {
    auto get_string = [&map](const std::string& key) -> std::string {
      auto it = map.find(flutter::EncodableValue(key));
      if (it != map.end() && std::holds_alternative<std::string>(it->second)) {
        return std::get<std::string>(it->second);
      }
      return "";
    };

    auto get_bool = [&map](const std::string& key) -> bool {
      auto it = map.find(flutter::EncodableValue(key));
      if (it != map.end() && std::holds_alternative<bool>(it->second)) {
        return std::get<bool>(it->second);
      }
      return false;
    };

    auto get_int64 = [&map](const std::string& key) -> int64_t {
      auto it = map.find(flutter::EncodableValue(key));
      if (it != map.end() && std::holds_alternative<int64_t>(it->second)) {
        return std::get<int64_t>(it->second);
      }
      return 0;
    };

    return Remote(
        get_string("name"), get_string("url"), get_string("collection_id"),
        get_string("title"), get_string("comment"), get_string("description"),
        get_string("homepage"), get_string("icon"),
        get_string("default_branch"), get_string("main_ref"),
        get_string("remote_type"), get_string("filter"),
        get_string("appstream_timestamp"), get_string("appstream_dir"),
        get_bool("gpg_verify"), get_bool("no_enumerate"), get_bool("no_deps"),
        get_bool("disabled"), get_int64("prio"));
  } catch (const std::exception& e) {
    spdlog::error("Failed to create Remote from map: {}", e.what());
    return std::nullopt;
  }
}

std::string EncodableListSerializer::Serialize(
    const flutter::EncodableList& list) {
  try {
    spdlog::debug("Serializing EncodableList with {} items", list.size());

    // Validate all items in the list first with enhanced validation
    for (size_t i = 0; i < list.size(); ++i) {
      if (!ValidateEncodableValueForSerialization(
              list[i], "list[" + std::to_string(i) + "]")) {
        spdlog::error(
            "Invalid EncodableValue at index {} - this will cause assertion "
            "failure",
            i);
        return "";
      }
    }

    auto& codec = flutter::StandardMessageCodec::GetInstance();
    auto encoded = codec.EncodeMessage(flutter::EncodableValue(list));

    if (!encoded) {
      spdlog::error("StandardMessageCodec returned null");
      return "";
    }

    std::string result;
    result.reserve(encoded->size() * 2);

    static constexpr char hex_chars[] = "0123456789abcdef";
    for (uint8_t byte : *encoded) {
      result += hex_chars[byte >> 4];
      result += hex_chars[byte & 0x0f];
    }

    spdlog::debug("Serialized to {} hex characters", result.size());
    return result;
  } catch (const std::exception& e) {
    spdlog::error("Failed to serialize EncodableList: {}", e.what());
    return "";
  }
}

std::optional<flutter::EncodableList> EncodableListSerializer::Deserialize(
    const std::string& hex_str) {
  if (hex_str.empty()) {
    return flutter::EncodableList{};
  }

  try {
    if (hex_str.size() % 2 != 0) {
      spdlog::error("Invalid hex string length: {}", hex_str.size());
      return std::nullopt;
    }

    std::vector<uint8_t> bytes;
    bytes.reserve(hex_str.size() / 2);

    for (size_t i = 0; i < hex_str.size(); i += 2) {
      std::string hex_byte = hex_str.substr(i, 2);

      if (!std::all_of(hex_byte.begin(), hex_byte.end(), ::isxdigit)) {
        spdlog::error("Invalid hex character in string at position {}", i);
        return std::nullopt;
      }

      auto byte = static_cast<uint8_t>(std::stoi(hex_byte, nullptr, 16));
      bytes.push_back(byte);
    }

    auto& codec = flutter::StandardMessageCodec::GetInstance();
    auto decoded = codec.DecodeMessage(bytes);

    if (!decoded) {
      spdlog::error("StandardMessageCodec failed to decode");
      return std::nullopt;
    }

    if (!std::holds_alternative<flutter::EncodableList>(*decoded)) {
      spdlog::error("Decoded message is not EncodableList");
      return std::nullopt;
    }

    auto result = std::get<flutter::EncodableList>(*decoded);
    spdlog::debug("Deserialized EncodableList with {} items", result.size());
    return result;
  } catch (const std::exception& e) {
    spdlog::error("Failed to deserialize EncodableList: {}", e.what());
    return std::nullopt;
  }
}

std::string ApplicationSerializer::Serialize(const Application& application) {
  try {
    auto map = ToEncodableMap(application);
    auto& codec = flutter::StandardMessageCodec::GetInstance();
    auto encoded = codec.EncodeMessage(flutter::EncodableValue(map));

    if (!encoded) {
      spdlog::error("StandardMessageCodec returned null for Application");
      return "";
    }

    // Convert to hex string for storage
    std::string result;
    result.reserve(encoded->size() * 2);
    static constexpr char hex_chars[] = "0123456789abcdef";
    for (uint8_t byte : *encoded) {
      result += hex_chars[byte >> 4];
      result += hex_chars[byte & 0x0f];
    }

    spdlog::debug("Serialized Application '{}' to {} hex characters",
                  application.name(), result.size());
    return result;
  } catch (const std::exception& e) {
    spdlog::error("Failed to serialize Application: {}", e.what());
    return "";
  }
}

std::optional<Application> ApplicationSerializer::Deserialize(
    const std::string& hex_str) {
  if (hex_str.empty()) {
    return std::nullopt;
  }

  try {
    // Validate hex string
    if (hex_str.size() % 2 != 0) {
      spdlog::error("Invalid hex string length: {}", hex_str.size());
      return std::nullopt;
    }

    std::vector<uint8_t> bytes;
    bytes.reserve(hex_str.size() / 2);

    for (size_t i = 0; i < hex_str.size(); i += 2) {
      std::string hex_byte = hex_str.substr(i, 2);

      // Validate hex characters
      if (!std::all_of(hex_byte.begin(), hex_byte.end(), ::isxdigit)) {
        spdlog::error("Invalid hex character in string at position {}", i);
        return std::nullopt;
      }

      auto byte = static_cast<uint8_t>(std::stoi(hex_byte, nullptr, 16));
      bytes.push_back(byte);
    }

    auto& codec = flutter::StandardMessageCodec::GetInstance();
    auto decoded = codec.DecodeMessage(bytes);

    if (!decoded) {
      spdlog::error("StandardMessageCodec failed to decode Application");
      return std::nullopt;
    }

    if (!std::holds_alternative<flutter::EncodableMap>(*decoded)) {
      spdlog::error("Decoded message is not EncodableMap for Application");
      return std::nullopt;
    }

    auto map = std::get<flutter::EncodableMap>(*decoded);
    return FromEncodableMap(map);
  } catch (const std::exception& e) {
    spdlog::error("Failed to deserialize Application: {}", e.what());
    return std::nullopt;
  }
}

flutter::EncodableMap ApplicationSerializer::ToEncodableMap(
    const Application& application) {
  return flutter::EncodableMap{
      {"name", flutter::EncodableValue(application.name())},
      {"id", flutter::EncodableValue(application.id())},
      {"summary", flutter::EncodableValue(application.summary())},
      {"version", flutter::EncodableValue(application.version())},
      {"origin", flutter::EncodableValue(application.origin())},
      {"license", flutter::EncodableValue(application.license())},
      {"installed_size", flutter::EncodableValue(application.installed_size())},
      {"deploy_dir", flutter::EncodableValue(application.deploy_dir())},
      {"is_current", flutter::EncodableValue(application.is_current())},
      {"content_rating_type",
       flutter::EncodableValue(application.content_rating_type())},
      {"content_rating", flutter::EncodableValue(application.content_rating())},
      {"latest_commit", flutter::EncodableValue(application.latest_commit())},
      {"eol", flutter::EncodableValue(application.eol())},
      {"eol_rebase", flutter::EncodableValue(application.eol_rebase())},
      {"subpaths", flutter::EncodableValue(application.subpaths())},
      {"metadata", flutter::EncodableValue(application.metadata())},
      {"appdata", flutter::EncodableValue(application.appdata())}};
}

std::optional<Application> ApplicationSerializer::FromEncodableMap(
    const flutter::EncodableMap& map) {
  try {
    auto get_string = [&map](const std::string& key) -> std::string {
      auto it = map.find(flutter::EncodableValue(key));
      if (it != map.end() && std::holds_alternative<std::string>(it->second)) {
        return std::get<std::string>(it->second);
      }
      return "";
    };

    auto get_bool = [&map](const std::string& key) -> bool {
      auto it = map.find(flutter::EncodableValue(key));
      if (it != map.end() && std::holds_alternative<bool>(it->second)) {
        return std::get<bool>(it->second);
      }
      return false;
    };

    auto get_int64 = [&map](const std::string& key) -> int64_t {
      auto it = map.find(flutter::EncodableValue(key));
      if (it != map.end() && std::holds_alternative<int64_t>(it->second)) {
        return std::get<int64_t>(it->second);
      }
      return 0;
    };

    auto get_encodable_map =
        [&map](const std::string& key) -> flutter::EncodableMap {
      auto it = map.find(flutter::EncodableValue(key));
      if (it != map.end() &&
          std::holds_alternative<flutter::EncodableMap>(it->second)) {
        return std::get<flutter::EncodableMap>(it->second);
      }
      return flutter::EncodableMap{};
    };

    auto get_encodable_list =
        [&map](const std::string& key) -> flutter::EncodableList {
      auto it = map.find(flutter::EncodableValue(key));
      if (it != map.end() &&
          std::holds_alternative<flutter::EncodableList>(it->second)) {
        return std::get<flutter::EncodableList>(it->second);
      }
      return flutter::EncodableList{};
    };

    return Application(
        get_string("name"), get_string("id"), get_string("summary"),
        get_string("version"), get_string("origin"), get_string("license"),
        get_int64("installed_size"), get_string("deploy_dir"),
        get_bool("is_current"), get_string("content_rating_type"),
        get_encodable_map("content_rating"), get_string("latest_commit"),
        get_string("eol"), get_string("eol_rebase"),
        get_encodable_list("subpaths"), get_string("metadata"),
        get_string("appdata"));
  } catch (const std::exception& e) {
    spdlog::error("Failed to create Application from map: {}", e.what());
    return std::nullopt;
  }
}

} // namespace flatpak_plugin::cache_utils
