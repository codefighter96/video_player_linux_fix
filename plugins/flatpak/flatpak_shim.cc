/*
 * Copyright 2020-2025 Toyota Connected North America
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

#include "flatpak_shim.h"

#include <filesystem>
#include <sstream>

#include <libxml/tree.h>
#include <libxml/xmlstring.h>
#include <zlib.h>

#include <common/common.h>

#include "appstream_catalog.h"
#include "component.h"
#include "messages.g.h"

namespace flatpak_plugin {
std::optional<std::string> FlatpakShim::getOptionalAttribute(
    const xmlNode* node,
    const char* attrName) {
  if (xmlChar* xmlValue = xmlGetProp(node, BAD_CAST attrName)) {
    std::string value(reinterpret_cast<const char*>(xmlValue));
    xmlFree(xmlValue);
    return value;
  }
  return std::nullopt;
}

std::string FlatpakShim::getAttribute(const xmlNode* node,
                                      const char* attrName) {
  if (xmlChar* xmlValue = xmlGetProp(node, BAD_CAST attrName)) {
    std::string value(reinterpret_cast<const char*>(xmlValue));
    xmlFree(xmlValue);
    return value;
  }
  return "";
}

void FlatpakShim::PrintComponent(const Component& component) {
  spdlog::info("[FlatpakPlugin] Component [{}]", component.getId());
  spdlog::info("[FlatpakPlugin] \tName: {}", component.getName());
  spdlog::info("[FlatpakPlugin] \tPackage Name: {}", component.getPkgName());
  spdlog::info("[FlatpakPlugin] \tSummary: {}", component.getSummary());

  if (component.getReleases().has_value()) {
    spdlog::info("[FlatpakPlugin] \tReleases: ");
    for (const auto& release : component.getReleases().value()) {
      spdlog::info("[FlatpakPlugin] \t\tVersion: {}", release.getVersion());
      spdlog::info("[FlatpakPlugin] \t\tTimestamp: {}", release.getTimestamp());
      if (release.getDescription().has_value()) {
        spdlog::info("[FlatpakPlugin] \t\tDescription: {}",
                     release.getDescription().value());
      }
      if (release.getSize().has_value()) {
        spdlog::info("[FlatpakPlugin] \t\tSize: {}", release.getSize().value());
      }
    }
  }

  // Checking and printing optional fields
  if (component.getVersion().has_value()) {
    spdlog::info("[FlatpakPlugin] \tVersion: {}",
                 component.getVersion().value());
  }
  if (component.getOrigin().has_value()) {
    spdlog::info("[FlatpakPlugin] \tOrigin: {}", component.getOrigin().value());
  }
  if (component.getMediaBaseurl().has_value()) {
    spdlog::info("[FlatpakPlugin] \tMedia Base URL: {}",
                 component.getMediaBaseurl().value());
  }
  if (component.getArchitecture().has_value()) {
    spdlog::info("[FlatpakPlugin] \tArchitecture: {}",
                 component.getArchitecture().value());
  }
  if (component.getProjectLicense().has_value()) {
    spdlog::info("[FlatpakPlugin] \tProject License: {}",
                 component.getProjectLicense().value());
  }
  if (component.getDescription().has_value()) {
    spdlog::info("[FlatpakPlugin] \tDescription: {}",
                 component.getDescription().value());
  }
  if (component.getUrl().has_value()) {
    spdlog::info("[FlatpakPlugin] \tURL: {}", component.getUrl().value());
  }
  if (component.getProjectGroup().has_value()) {
    spdlog::info("[FlatpakPlugin] \tProject Group: {}",
                 component.getProjectGroup().value());
  }
  if (component.getIcons().has_value()) {
    spdlog::info("[FlatpakPlugin] \tIcons:");
    for (const auto& icon : component.getIcons().value()) {
      icon.printIconDetails();
    }
  }
  if (component.getCategories().has_value()) {
    spdlog::info("[FlatpakPlugin] \tCategories:");
    for (const auto& category : component.getCategories().value()) {
      spdlog::info("[FlatpakPlugin] \t\t{}", category);
    }
  }
  if (component.getScreenshots().has_value()) {
    for (const auto& screenshot : component.getScreenshots().value()) {
      screenshot.printScreenshotDetails();
    }
  }
  if (component.getKeywords().has_value()) {
    spdlog::info("[FlatpakPlugin] \tKeywords:");
    for (const auto& keyword : component.getKeywords().value()) {
      spdlog::info("[FlatpakPlugin] \t\t{}", keyword);
    }
  }
  // Additional optional fields
  if (component.getSourcePkgname().has_value()) {
    spdlog::info("[FlatpakPlugin] \tSource Pkgname: {}",
                 component.getSourcePkgname().value());
  }
  if (component.getBundle().has_value()) {
    spdlog::info("[FlatpakPlugin] \tBundle: {}", component.getBundle().value());
  }
  if (component.getContentRatingType().has_value()) {
    spdlog::info("[FlatpakPlugin] \tContent Rating Type: [{}]",
                 component.getContentRatingType().value());
  }
  if (component.getContentRating().has_value()) {
    if (!component.getContentRating().value().empty()) {
      spdlog::info("[FlatpakPlugin] \tContent Rating:");
      for (const auto& [key, value] : component.getContentRating().value()) {
        spdlog::info("[FlatpakPlugin] \t\t{} = {}", key,
                     Component::RatingValueToString(value));
      }
    }
  }
  if (component.getAgreement().has_value()) {
    spdlog::info("[FlatpakPlugin] \tAgreement: {}",
                 component.getAgreement().value());
  }
}

GPtrArray* FlatpakShim::get_system_installations() {
  GError* error = nullptr;
  const auto sys_installs = flatpak_get_system_installations(nullptr, &error);
  if (error) {
    spdlog::error("[FlatpakPlugin] Error getting system installations: {}",
                  error->message);
    g_clear_error(&error);
  }
  return sys_installs;
}

GPtrArray* FlatpakShim::get_remotes(FlatpakInstallation* installation) {
  GError* error = nullptr;
  const auto remotes =
      flatpak_installation_list_remotes(installation, nullptr, &error);
  if (error) {
    spdlog::error("[FlatpakPlugin] Error listing remotes: {}", error->message);
    g_clear_error(&error);
  }
  return remotes;
}

std::time_t FlatpakShim::get_appstream_timestamp(
    const std::filesystem::path& timestamp_filepath) {
  if (exists(timestamp_filepath)) {
    const auto fileTime = std::filesystem::last_write_time(timestamp_filepath);
    const auto sctp =
        std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            fileTime - std::filesystem::file_time_type::clock::now() +
            std::chrono::system_clock::now());
    return std::chrono::system_clock::to_time_t(sctp);
  }
  spdlog::error("[FlatpakPlugin] appstream_timestamp does not exist: {}",
                timestamp_filepath.c_str());
  return {};
}

void FlatpakShim::format_time_iso8601(const time_t raw_time,
                                      char* buffer,
                                      const size_t buffer_size) {
  tm tm_info{};
  localtime_r(&raw_time, &tm_info);
  strftime(buffer, buffer_size, "%Y-%m-%dT%H:%M:%S", &tm_info);
  long timezone_offset = tm_info.tm_gmtoff;
  const char sign = (timezone_offset >= 0) ? '+' : '-';
  timezone_offset = std::abs(timezone_offset);
  snprintf(buffer + strlen(buffer), buffer_size - strlen(buffer),
           "%c%02ld:%02ld", sign, timezone_offset / 3600,
           (timezone_offset % 3600) / 60);
}

flutter::EncodableList FlatpakShim::installation_get_default_languages(
    FlatpakInstallation* installation) {
  flutter::EncodableList languages;
  GError* error = nullptr;
  const auto default_languages =
      flatpak_installation_get_default_languages(installation, &error);
  if (error) {
    spdlog::error("[FlatpakPlugin] Error getting default languages: {}",
                  error->message);
    g_error_free(error);
  }
  if (default_languages) {
    for (auto language = default_languages; *language; ++language) {
      languages.emplace_back(static_cast<const char*>(*language));
    }
    g_strfreev(default_languages);
  }
  return languages;
}

flutter::EncodableList FlatpakShim::installation_get_default_locales(
    FlatpakInstallation* installation) {
  flutter::EncodableList locales;
  GError* error = nullptr;

  const auto default_locales =
      flatpak_installation_get_default_locales(installation, &error);
  if (error) {
    spdlog::error(
        "[FlatpakPlugin] flatpak_installation_get_default_locales: {}",
        error->message);
    g_error_free(error);
    error = nullptr;
  }
  if (default_locales != nullptr) {
    for (auto locale = default_locales; *locale != nullptr; ++locale) {
      locales.emplace_back(static_cast<const char*>(*locale));
    }
    g_strfreev(default_locales);
  }
  return locales;
}

std::string FlatpakShim::FlatpakRemoteTypeToString(
    const FlatpakRemoteType type) {
  switch (type) {
    case FLATPAK_REMOTE_TYPE_STATIC:
      // Statically configured remote
      return "Static";
    case FLATPAK_REMOTE_TYPE_USB:
      // Dynamically detected local pathname remote
      return "USB";
    case FLATPAK_REMOTE_TYPE_LAN:
      // Dynamically detected network remote
      return "LAN";
  }
  return "Unknown";
}

Installation FlatpakShim::get_installation(FlatpakInstallation* installation) {
  flutter::EncodableList remote_list;

  const auto remotes = get_remotes(installation);
  for (size_t j = 0; j < remotes->len; j++) {
    const auto remote =
        static_cast<FlatpakRemote*>(g_ptr_array_index(remotes, j));

    const auto name = flatpak_remote_get_name(remote);
    const auto url = flatpak_remote_get_url(remote);
    const auto collection_id = flatpak_remote_get_collection_id(remote);
    const auto title = flatpak_remote_get_title(remote);
    const auto comment = flatpak_remote_get_comment(remote);
    const auto description = flatpak_remote_get_description(remote);
    const auto homepage = flatpak_remote_get_homepage(remote);
    const auto icon = flatpak_remote_get_icon(remote);
    const auto default_branch = flatpak_remote_get_default_branch(remote);
    const auto main_ref = flatpak_remote_get_main_ref(remote);
    const auto filter = flatpak_remote_get_filter(remote);
    const bool gpg_verify = flatpak_remote_get_gpg_verify(remote);
    const bool no_enumerate = flatpak_remote_get_noenumerate(remote);
    const bool no_deps = flatpak_remote_get_nodeps(remote);
    const bool disabled = flatpak_remote_get_disabled(remote);
    const int32_t prio = flatpak_remote_get_prio(remote);

    const auto default_arch = flatpak_get_default_arch();
    auto appstream_timestamp_path = g_file_get_path(
        flatpak_remote_get_appstream_timestamp(remote, default_arch));
    auto appstream_dir_path =
        g_file_get_path(flatpak_remote_get_appstream_dir(remote, default_arch));

    std::filesystem::path appstream_xml_path = appstream_dir_path;
    appstream_xml_path /= "appstream.xml";
    auto catalog = AppstreamCatalog(appstream_xml_path, "");
    spdlog::debug("[FlatpakPlugin] Appstream Catalog Total components: {}",
                  catalog.getTotalComponentCount());
    const auto& components = catalog.getComponents();
    for (const auto& component : components) {
      PrintComponent(component);
    }

    auto appstream_timestamp =
        get_appstream_timestamp(appstream_timestamp_path);
    char formatted_time[30];
    format_time_iso8601(appstream_timestamp, formatted_time,
                        sizeof(formatted_time));

    remote_list.emplace_back(flutter::CustomEncodableValue(Remote(
        name ? name : "", url ? url : "", collection_id ? collection_id : "",
        title ? title : "", comment ? comment : "",
        description ? description : "", homepage ? homepage : "",
        icon ? icon : "", default_branch ? default_branch : "",
        main_ref ? main_ref : "",
        FlatpakRemoteTypeToString(flatpak_remote_get_remote_type(remote)),
        filter ? filter : "", formatted_time, appstream_dir_path, gpg_verify,
        no_enumerate, no_deps, disabled, prio)));
  }
  g_ptr_array_unref(remotes);

  const auto id = flatpak_installation_get_id(installation);
  const auto display_name = flatpak_installation_get_display_name(installation);
  const auto installationPath = flatpak_installation_get_path(installation);
  const auto path = g_file_get_path(installationPath);
  const auto no_interaction =
      flatpak_installation_get_no_interaction(installation);
  const auto is_user = flatpak_installation_get_is_user(installation);
  const auto priority = flatpak_installation_get_priority(installation);
  const auto default_languages =
      installation_get_default_languages(installation);
  const auto default_locales = installation_get_default_locales(installation);

  return Installation(id, display_name, path, no_interaction, is_user, priority,
                      default_languages, default_locales, remote_list);
}

flutter::EncodableMap FlatpakShim::get_content_rating_map(
    FlatpakInstalledRef* ref) {
  flutter::EncodableMap result;
  const auto content_rating =
      flatpak_installed_ref_get_appdata_content_rating(ref);
  if (!content_rating) {
    return result;
  }

  GHashTableIter iter;
  gpointer key, value;
  g_hash_table_iter_init(&iter, content_rating);
  while (g_hash_table_iter_next(&iter, &key, &value)) {
    result[flutter::EncodableValue(static_cast<char*>(key))] =
        flutter::EncodableValue(static_cast<char*>(value));
  }
  return result;
}

void FlatpakShim::get_application_list(
    FlatpakInstallation* installation,
    flutter::EncodableList& application_list) {
  flutter::EncodableList result;
  GError* error = nullptr;

  const auto refs =
      flatpak_installation_list_installed_refs(installation, nullptr, &error);
  if (error) {
    spdlog::error("[FlatpakPlugin] Error listing installed refs: {}",
                  error->message);
    g_clear_error(&error);
    g_object_unref(installation);
    return;
  }

  for (guint i = 0; i < refs->len; i++) {
    const auto ref =
        static_cast<FlatpakInstalledRef*>(g_ptr_array_index(refs, i));

    const auto appdata_name = flatpak_installed_ref_get_appdata_name(ref);
    const auto appdata_id = flatpak_installation_get_id(installation);
    const auto appdata_summary = flatpak_installed_ref_get_appdata_summary(ref);
    const auto appdata_version = flatpak_installed_ref_get_appdata_version(ref);
    const auto appdata_origin = flatpak_installed_ref_get_origin(ref);
    const auto appdata_license = flatpak_installed_ref_get_appdata_license(ref);
    const auto deploy_dir = flatpak_installed_ref_get_deploy_dir(ref);
    const auto content_rating_type =
        flatpak_installed_ref_get_appdata_content_rating_type(ref);
    const auto latest_commit = flatpak_installed_ref_get_latest_commit(ref);
    const auto eol = flatpak_installed_ref_get_eol(ref);
    const auto eol_rebase = flatpak_installed_ref_get_eol_rebase(ref);

    flutter::EncodableList subpath_list;
    if (const auto subpaths = flatpak_installed_ref_get_subpaths(ref);
        subpaths != nullptr) {
      for (auto sub_path = subpaths; *sub_path != nullptr; ++sub_path) {
        subpath_list.emplace_back(static_cast<const char*>(*sub_path));
      }
    }

    application_list.emplace_back(flutter::CustomEncodableValue(Application(
        appdata_name ? appdata_name : "", appdata_id ? appdata_id : "",
        appdata_summary ? appdata_summary : "",
        appdata_version ? appdata_version : "",
        appdata_origin ? appdata_origin : "",
        appdata_license ? appdata_license : "",
        static_cast<int64_t>(flatpak_installed_ref_get_installed_size(ref)),
        deploy_dir ? deploy_dir : "", flatpak_installed_ref_get_is_current(ref),
        content_rating_type ? content_rating_type : "",
        get_content_rating_map(ref), latest_commit ? latest_commit : "",
        eol ? eol : "", eol_rebase ? eol_rebase : "", subpath_list,
        get_metadata_as_string(ref), get_appdata_as_string(ref))));
  }
  g_ptr_array_unref(refs);
}

ErrorOr<flutter::EncodableList> FlatpakShim::GetApplicationsInstalled() {
  flutter::EncodableList application_list;
  GError* error = nullptr;
  auto installation = flatpak_installation_new_user(nullptr, &error);
  get_application_list(installation, application_list);

  const auto system_installations = get_system_installations();
  for (size_t i = 0; i < system_installations->len; i++) {
    installation = static_cast<FlatpakInstallation*>(
        g_ptr_array_index(system_installations, i));
    get_application_list(installation, application_list);
  }
  g_ptr_array_unref(system_installations);

  return application_list;
}

ErrorOr<Installation> FlatpakShim::GetUserInstallation() {
  GError* error = nullptr;
  const auto installation = flatpak_installation_new_user(nullptr, &error);
  if (error) {
    spdlog::error("[FlatpakPlugin] Error getting user installation: {}",
                  error->message);
    g_clear_error(&error);
    return ErrorOr<Installation>(
        FlutterError("INSTALLATION_ERROR", "Failed to get user installation"));
  }
  return get_installation(installation);
}

ErrorOr<flutter::EncodableList> FlatpakShim::GetSystemInstallations() {
  flutter::EncodableList installs_list;
  const auto system_installations = get_system_installations();
  for (size_t i = 0; i < system_installations->len; i++) {
    const auto installation = static_cast<FlatpakInstallation*>(
        g_ptr_array_index(system_installations, i));
    installs_list.emplace_back(
        flutter::CustomEncodableValue(get_installation(installation)));
  }
  g_ptr_array_unref(system_installations);

  return installs_list;
}

ErrorOr<flutter::EncodableList> FlatpakShim::get_remotes_by_installation_id(
    const std::string& installation_id) {
  try {
    FlatpakInstallation* installation = nullptr;

    // check user installation
    if (installation_id == "user") {
      GError* error = nullptr;
      installation = flatpak_installation_new_user(nullptr, &error);
      if (error) {
        const std::string error_msg = error->message;
        g_clear_error(&error);
        return ErrorOr<flutter::EncodableList>(
            FlutterError("INSTALLATION_ERROR",
                         "Failed to get user installation: " + error_msg));
      }
    } else {
      // check system installations
      const auto system_installations = get_system_installations();
      if (!system_installations) {
        return ErrorOr<flutter::EncodableList>(
            FlutterError("NO_INSTALLATIONS", "No system installation found"));
      }

      for (size_t i = 0; i < system_installations->len; i++) {
        auto* sys_installation = static_cast<FlatpakInstallation*>(
            g_ptr_array_index(system_installations, i));

        if (const auto id = flatpak_installation_get_id(sys_installation);
            id && installation_id == std::string(id)) {
          installation =
              static_cast<FlatpakInstallation*>(g_object_ref(sys_installation));
          break;
        }
      }

      g_ptr_array_unref(system_installations);

      if (!installation) {
        return ErrorOr<flutter::EncodableList>(FlutterError(
            "INSTALLATION_NOT_FOUND",
            "Installation with ID '" + installation_id + "' not found"));
      }
    }

    if (!installation) {
      return ErrorOr<flutter::EncodableList>(
          FlutterError("INSTALLATION_ERROR", "Failed to get installation"));
    }

    // get remotes for this installation
    GPtrArray* remotes = get_remotes(installation);
    if (!remotes) {
      g_object_unref(installation);
      return flutter::EncodableList{};
    }

    // convert remotes to EncodableList
    flutter::EncodableList remote_list =
        convert_remotes_to_EncodableList(remotes);

    g_ptr_array_unref(remotes);
    g_object_unref(installation);

    spdlog::debug(
        "[FlatpakPlugin] Successfully retrieved {} remotes for installation "
        "{} ",
        remote_list.size(), installation_id);
    return remote_list;
  } catch (const std::exception& e) {
    spdlog::error("[FlatpakPlugin] Exception occured while getting remotes]",
                  e.what());
    return ErrorOr<flutter::EncodableList>(FlutterError(
        "UNKNOWN_ERROR", "Failed to get remotes: " + std::string(e.what())));
  }
}

flutter::EncodableList FlatpakShim::convert_remotes_to_EncodableList(
    const GPtrArray* remotes) {
  flutter::EncodableList result;

  if (!remotes) {
    spdlog::warn("[FlatpakPlugin] Received null remotes array");
    return result;
  }

  for (size_t j = 0; j < remotes->len; j++) {
    const auto remote =
        static_cast<FlatpakRemote*>(g_ptr_array_index(remotes, j));
    if (!remote) {
      spdlog::warn("[FlatpakPlugin] Null remote at index {}", j);
      continue;
    }
    try {
      const auto name = flatpak_remote_get_name(remote);
      const auto url = flatpak_remote_get_url(remote);
      const auto collection_id = flatpak_remote_get_collection_id(remote);
      const auto title = flatpak_remote_get_title(remote);
      const auto comment = flatpak_remote_get_comment(remote);
      const auto description = flatpak_remote_get_description(remote);
      const auto homepage = flatpak_remote_get_homepage(remote);
      const auto icon = flatpak_remote_get_icon(remote);
      const auto default_branch = flatpak_remote_get_default_branch(remote);
      const auto main_ref = flatpak_remote_get_main_ref(remote);
      const auto filter = flatpak_remote_get_filter(remote);
      const bool gpg_verify = flatpak_remote_get_gpg_verify(remote);
      const bool no_enumerate = flatpak_remote_get_noenumerate(remote);
      const bool no_deps = flatpak_remote_get_nodeps(remote);
      const bool disabled = flatpak_remote_get_disabled(remote);
      const int32_t prio = flatpak_remote_get_prio(remote);

      const auto default_arch = flatpak_get_default_arch();
      auto appstream_timestamp_path = g_file_get_path(
          flatpak_remote_get_appstream_timestamp(remote, default_arch));
      auto appstream_dir_path = g_file_get_path(
          flatpak_remote_get_appstream_dir(remote, default_arch));

      auto appstream_timestamp =
          get_appstream_timestamp(appstream_timestamp_path);
      char formatted_time[30];
      format_time_iso8601(appstream_timestamp, formatted_time,
                          sizeof(formatted_time));

      result.emplace_back(flutter::CustomEncodableValue(Remote(
          name ? name : "", url ? url : "", collection_id ? collection_id : "",
          title ? title : "", comment ? comment : "",
          description ? description : "", homepage ? homepage : "",
          icon ? icon : "", default_branch ? default_branch : "",
          main_ref ? main_ref : "",
          FlatpakRemoteTypeToString(flatpak_remote_get_remote_type(remote)),
          filter ? filter : "", formatted_time,
          appstream_dir_path ? appstream_dir_path : "", gpg_verify,
          no_enumerate, no_deps, disabled, static_cast<int64_t>(prio))));

      if (appstream_timestamp_path) {
        g_free(appstream_timestamp_path);
      }

      if (appstream_dir_path) {
        g_free(appstream_dir_path);
      }
    } catch (const std::exception& e) {
      spdlog::error("[FlatpakPlugin] Received exception {}", e.what());
    }
  }
  spdlog::debug("[FlatpakPlugin] Converted {} remotes to encodable list ",
                result.size());
  return result;
}

std::string FlatpakShim::get_metadata_as_string(
    FlatpakInstalledRef* installed_ref) {
  GError* error = nullptr;

  const auto g_bytes =
      flatpak_installed_ref_load_metadata(installed_ref, nullptr, &error);

  if (!g_bytes) {
    if (error != nullptr) {
      spdlog::error("[FlatpakPlugin] Error loading metadata: %s\n",
                    error->message);
      g_clear_error(&error);
    }
    return {};
  }

  gsize size;
  const auto data = static_cast<const char*>(g_bytes_get_data(g_bytes, &size));
  std::string result(data, size);
  g_bytes_unref(g_bytes);
  return result;
}

std::string FlatpakShim::get_appdata_as_string(
    FlatpakInstalledRef* installed_ref) {
  GError* error = nullptr;
  const auto g_bytes =
      flatpak_installed_ref_load_appdata(installed_ref, nullptr, &error);
  if (!g_bytes) {
    if (error != nullptr) {
      SPDLOG_ERROR("[FlatpakPlugin] {}", error->message);
      g_clear_error(&error);
    }
    return {};
  }

  gsize size;
  const auto data =
      static_cast<const uint8_t*>(g_bytes_get_data(g_bytes, &size));
  const std::vector<char> compressedData(data, data + size);
  std::vector<char> decompressedData;
  decompress_gzip(compressedData, decompressedData);
  std::string decompressedString(decompressedData.begin(),
                                 decompressedData.end());
  return decompressedString;
}

std::vector<char> FlatpakShim::decompress_gzip(
    const std::vector<char>& compressedData,
    std::vector<char>& decompressedData) {
  z_stream zs = {};
  if (inflateInit2(&zs, 16 + MAX_WBITS) != Z_OK) {
    spdlog::error("[FlatpakPlugin] Unable to initialize zlib inflate");
    return {};
  }
  zs.next_in =
      reinterpret_cast<Bytef*>(const_cast<char*>(compressedData.data()));
  zs.avail_in = static_cast<uInt>(compressedData.size());
  int zlibResult;
  char buffer[BUFFER_SIZE];
  do {
    zs.next_out = reinterpret_cast<Bytef*>(buffer);
    zs.avail_out = BUFFER_SIZE;
    zlibResult = inflate(&zs, 0);
    decompressedData.insert(decompressedData.end(), buffer,
                            buffer + BUFFER_SIZE - zs.avail_out);
  } while (zlibResult == Z_OK);
  inflateEnd(&zs);
  if (zlibResult != Z_STREAM_END) {
    spdlog::error("[FlatpakPlugin] Gzip decompression error");
    return {};
  }
  return decompressedData;
}
}  // namespace flatpak_plugin