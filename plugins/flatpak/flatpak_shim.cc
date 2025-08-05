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

#include <libxml/tree.h>
#include <libxml/xmlstring.h>
#include <zlib.h>

#include "plugins/common/common.h"

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
  if (remotes) {
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

        // Validate required fields before creating Remote object
        if (!name || !url) {
          spdlog::warn(
              "[FlatpakPlugin] Skipping remote with missing name or URL");
          if (appstream_timestamp_path) {
            g_free(appstream_timestamp_path);
          }
          if (appstream_dir_path) {
            g_free(appstream_dir_path);
          }
          continue;
        }

        // Convert Remote to EncodableMap
        flutter::EncodableMap remote_map{
            {"name", flutter::EncodableValue(std::string(name))},
            {"url", flutter::EncodableValue(std::string(url))},
            {"collection_id",
             flutter::EncodableValue(collection_id ? std::string(collection_id)
                                                   : "")},
            {"title", flutter::EncodableValue(title ? std::string(title) : "")},
            {"comment",
             flutter::EncodableValue(comment ? std::string(comment) : "")},
            {"description", flutter::EncodableValue(
                                description ? std::string(description) : "")},
            {"homepage",
             flutter::EncodableValue(homepage ? std::string(homepage) : "")},
            {"icon", flutter::EncodableValue(icon ? std::string(icon) : "")},
            {"default_branch",
             flutter::EncodableValue(
                 default_branch ? std::string(default_branch) : "")},
            {"main_ref",
             flutter::EncodableValue(main_ref ? std::string(main_ref) : "")},
            {"remote_type", flutter::EncodableValue(FlatpakRemoteTypeToString(
                                flatpak_remote_get_remote_type(remote)))},
            {"filter",
             flutter::EncodableValue(filter ? std::string(filter) : "")},
            {"appstream_timestamp",
             flutter::EncodableValue(std::string(formatted_time))},
            {"appstream_dir",
             flutter::EncodableValue(
                 appstream_dir_path ? std::string(appstream_dir_path) : "")},
            {"gpg_verify", flutter::EncodableValue(gpg_verify)},
            {"no_enumerate", flutter::EncodableValue(no_enumerate)},
            {"no_deps", flutter::EncodableValue(no_deps)},
            {"disabled", flutter::EncodableValue(disabled)},
            {"prio", flutter::EncodableValue(static_cast<int64_t>(prio))}};

        remote_list.emplace_back(remote_map);

        if (appstream_timestamp_path) {
          g_free(appstream_timestamp_path);
        }

        if (appstream_dir_path) {
          g_free(appstream_dir_path);
        }
      } catch (const std::exception& e) {
        spdlog::error("[FlatpakPlugin] Exception processing remote: {}",
                      e.what());
        continue;
      }
    }
    g_ptr_array_unref(remotes);
  }

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

  if (!id || !path) {
    spdlog::error("[FlatpakPlugin] Installation missing required fields");
    return Installation("", "", "", false, false, 0, flutter::EncodableList{},
                        flutter::EncodableList{}, flutter::EncodableList{});
  }

  return Installation(id, display_name ? display_name : "", path,
                      no_interaction, is_user, priority, default_languages,
                      default_locales, remote_list);
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
    // Validate pointers before casting
    if (key && value) {
      const char* key_str = static_cast<const char*>(key);
      const char* value_str = static_cast<const char*>(value);

      if (key_str && value_str && strlen(key_str) > 0 &&
          strlen(value_str) > 0) {
        result[flutter::EncodableValue(std::string(key_str))] =
            flutter::EncodableValue(std::string(value_str));
      }
    }
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

    // Convert Application to EncodableMap
    flutter::EncodableMap app_map{
        {"name", flutter::EncodableValue(
                     appdata_name ? std::string(appdata_name) : "")},
        {"id",
         flutter::EncodableValue(appdata_id ? std::string(appdata_id) : "")},
        {"summary", flutter::EncodableValue(
                        appdata_summary ? std::string(appdata_summary) : "")},
        {"version", flutter::EncodableValue(
                        appdata_version ? std::string(appdata_version) : "")},
        {"origin", flutter::EncodableValue(
                       appdata_origin ? std::string(appdata_origin) : "")},
        {"license", flutter::EncodableValue(
                        appdata_license ? std::string(appdata_license) : "")},
        {"installed_size", flutter::EncodableValue(static_cast<int64_t>(
                               flatpak_installed_ref_get_installed_size(ref)))},
        {"deploy_dir",
         flutter::EncodableValue(deploy_dir ? std::string(deploy_dir) : "")},
        {"is_current",
         flutter::EncodableValue(flatpak_installed_ref_get_is_current(ref))},
        {"content_rating_type",
         flutter::EncodableValue(
             content_rating_type ? std::string(content_rating_type) : "")},
        {"content_rating",
         flutter::EncodableValue(get_content_rating_map(ref))},
        {"latest_commit", flutter::EncodableValue(
                              latest_commit ? std::string(latest_commit) : "")},
        {"eol", flutter::EncodableValue(eol ? std::string(eol) : "")},
        {"eol_rebase",
         flutter::EncodableValue(eol_rebase ? std::string(eol_rebase) : "")},
        {"subpaths", flutter::EncodableValue(subpath_list)},
        {"metadata", flutter::EncodableValue(get_metadata_as_string(ref))},
        {"appdata", flutter::EncodableValue(get_appdata_as_string(ref))}};

    application_list.emplace_back(app_map);
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

ErrorOr<flutter::EncodableList> FlatpakShim::GetApplicationsRemote(
    const std::string& id) {
  try {
    if (id.empty()) {
      return ErrorOr<flutter::EncodableList>(
          FlutterError("INVALID_REMOTE_GET", "Remote id is required"));
    }
    spdlog::info("[FlatpakPlugin] Get Applications from Remote {}", id.c_str());
    flutter::EncodableList application_list;
    GError* error = nullptr;

    auto installation = flatpak_installation_new_user(nullptr, &error);
    if (error) {
      spdlog::error("[FlatpakPlugin] Failed to add Remote installation: {}",
                    error->message);
      g_clear_error(&error);
      return ErrorOr<flutter::EncodableList>(FlutterError(
          "INVALID_REMOTE_GET", "Failed to add Remote installation"));
    }

    auto remotes =
        flatpak_installation_list_remotes(installation, nullptr, &error);
    if (error) {
      spdlog::error("[FlatpakPlugin] Error occured while getting remotes",
                    error->message);
      g_clear_error(&error);
      g_object_unref(installation);
      return ErrorOr<flutter::EncodableList>(
          FlutterError("INVALID_REMOTE_GET", "Failed to get remotes"));
    }

    std::string found_remote_name;
    bool found_remote = false;

    for (guint i = 0; i < remotes->len; i++) {
      auto remote = static_cast<FlatpakRemote*>(g_ptr_array_index(remotes, i));
      const auto remote_name = flatpak_remote_get_name(remote);

      if (flatpak_remote_get_disabled(remote)) {
        continue;
      }
      if (remote_name && std::string(remote_name) == id) {
        found_remote_name = remote_name;
        found_remote = true;
        break;
      }
    }
    g_ptr_array_unref(remotes);

    if (!found_remote) {
      spdlog::error("[FlatpakPlugin] Remote {} not found", id.c_str());
      g_object_unref(installation);
      return ErrorOr<flutter::EncodableList>(
          FlutterError("INVALID_REMOTE_GET", "Remote is not found"));
    }

    auto app_refs = flatpak_installation_list_remote_refs_sync(
        installation, found_remote_name.c_str(), nullptr, &error);
    if (error) {
      spdlog::error("[FlatpakPlugin] Failed to get applications for remote :{}",
                    error->message);
      g_clear_error(&error);
      g_object_unref(installation);
      return ErrorOr<flutter::EncodableList>(
          FlutterError("INVALID_REMOTE_GET", "Failed to get remote list"));
    }

    if (!app_refs) {
      spdlog::error("[FlatpakPlugin] No application found in remote {}",
                    id.c_str());
      g_object_unref(installation);
      return application_list;
    }

    for (guint i = 0; i < app_refs->len; i++) {
      auto app_ref =
          static_cast<FlatpakRemoteRef*>(g_ptr_array_index(app_refs, i));
      // only applications
      if (flatpak_ref_get_kind(FLATPAK_REF(app_ref)) != FLATPAK_REF_KIND_APP) {
        continue;
      }

      const auto ref_name = flatpak_ref_get_name(FLATPAK_REF(app_ref));
      const auto ref_arch = flatpak_ref_get_arch(FLATPAK_REF(app_ref));
      const auto ref_branch = flatpak_ref_get_branch(FLATPAK_REF(app_ref));
      const auto download_size = flatpak_remote_ref_get_download_size(app_ref);
      const auto installed_size =
          flatpak_remote_ref_get_installed_size(app_ref);
      const auto metadata = flatpak_remote_ref_get_metadata(app_ref);
      const auto eol = flatpak_remote_ref_get_eol(app_ref);
      const auto eol_rebase = flatpak_remote_ref_get_eol_rebase(app_ref);

      std::string full_ref = std::string("app/") + (ref_name ? ref_name : "") +
                             "/" + (ref_arch ? ref_arch : "") + "/" +
                             (ref_branch ? ref_branch : "");

      std::string metadata_str;
      if (metadata) {
        gsize size;
        const auto* data =
            static_cast<const gchar*>(g_bytes_get_data(metadata, &size));
        if (data && size > 0) {
          metadata_str = std::string(data, size);
        }
      }

      flutter::EncodableList empty_subpaths;
      flutter::EncodableMap empty_content_rating;

      // Convert Application to EncodableMap instead of CustomEncodableValue
      flutter::EncodableMap app_map{
          {"name",
           flutter::EncodableValue(ref_name ? std::string(ref_name) : "")},
          {"id", flutter::EncodableValue(std::string(id))},
          {"summary", flutter::EncodableValue("")},
          {"version", flutter::EncodableValue("")},
          {"origin", flutter::EncodableValue(std::string(found_remote_name))},
          {"license", flutter::EncodableValue("")},
          {"installed_size",
           flutter::EncodableValue(static_cast<int64_t>(installed_size))},
          {"deploy_dir", flutter::EncodableValue("")},
          {"is_current", flutter::EncodableValue(false)},
          {"content_rating_type", flutter::EncodableValue("")},
          {"content_rating", flutter::EncodableValue(empty_content_rating)},
          {"latest_commit", flutter::EncodableValue("")},
          {"eol", flutter::EncodableValue(eol ? std::string(eol) : "")},
          {"eol_rebase",
           flutter::EncodableValue(eol_rebase ? std::string(eol_rebase) : "")},
          {"subpaths", flutter::EncodableValue(empty_subpaths)},
          {"metadata", flutter::EncodableValue(metadata_str)},
          {"appdata", flutter::EncodableValue("")}};

      application_list.emplace_back(app_map);
    }

    g_ptr_array_unref(app_refs);
    g_object_unref(installation);
    spdlog::info("[FlatpakPlugin] Found {} applications in remote {}",
                 application_list.size(), id.c_str());
    return application_list;
  } catch (const std::exception& e) {
    spdlog::error("[FlatpakPlugin] Failed to get applications from remote",
                  e.what());
    return ErrorOr<flutter::EncodableList>(
        FlutterError("INVALID_REMOTE_GET", "Failed to get remote list"));
  }
}

ErrorOr<bool> FlatpakShim::RemoteAdd(const Remote& configuration) {
  try {
    if (configuration.name().empty() || configuration.url().empty()) {
      return ErrorOr<bool>(FlutterError("INVALID_REMOTE_ADD",
                                        "Remote name and URL is required"));
    }

    spdlog::info("[FlatpakPlugin] Adding Remote {}", configuration.name());
    GError* error = nullptr;
    auto installation = flatpak_installation_new_user(nullptr, &error);
    if (error) {
      spdlog::error("[FlatpakPlugin] Failed to add Remote installation: {}",
                    error->message);
      g_clear_error(&error);
      return ErrorOr<bool>(FlutterError("INVALID_REMOTE_ADD",
                                        "Failed to add Remote installation"));
    }

    auto existing_remote = flatpak_installation_get_remote_by_name(
        installation, configuration.name().c_str(), nullptr, nullptr);
    if (existing_remote) {
      spdlog::error("[FlatpakPlugin] Remote installation already exists");
      g_object_unref(installation);
      g_object_unref(existing_remote);
      return ErrorOr<bool>(
          FlutterError("Remote_EXISTS", "Remote already exists"));
    }

    gboolean ifneeded = TRUE;
    auto remote = flatpak_remote_new(configuration.name().c_str());
    if (!remote) {
      spdlog::error("[FlatpakPlugin] Failed to create remote object");
      g_object_unref(installation);
      return ErrorOr<bool>(
          FlutterError("INVALID_REMOTE_ADD", "Failed to create remote object"));
    }

    flatpak_remote_set_url(remote, configuration.url().c_str());

    if (!configuration.title().empty()) {
      flatpak_remote_set_title(remote, configuration.title().c_str());
    }
    if (!configuration.collection_id().empty()) {
      flatpak_remote_set_collection_id(remote,
                                       configuration.collection_id().c_str());
    }
    if (!configuration.comment().empty()) {
      flatpak_remote_set_comment(remote, configuration.comment().c_str());
    }
    if (!configuration.description().empty()) {
      flatpak_remote_set_description(remote,
                                     configuration.description().c_str());
    }
    if (!configuration.default_branch().empty()) {
      flatpak_remote_set_default_branch(remote,
                                        configuration.default_branch().c_str());
    }
    if (!configuration.filter().empty()) {
      flatpak_remote_set_filter(remote, configuration.filter().c_str());
    }
    if (!configuration.homepage().empty()) {
      flatpak_remote_set_homepage(remote, configuration.homepage().c_str());
    }
    if (!configuration.icon().empty()) {
      flatpak_remote_set_icon(remote, configuration.icon().c_str());
    }
    if (!configuration.main_ref().empty()) {
      flatpak_remote_set_main_ref(remote, configuration.main_ref().c_str());
    }

    flatpak_remote_set_nodeps(remote, configuration.no_deps());
    flatpak_remote_set_gpg_verify(remote, configuration.gpg_verify());
    flatpak_remote_set_prio(remote, static_cast<int>(configuration.prio()));
    flatpak_remote_set_disabled(remote, configuration.disabled());

    gboolean result = flatpak_installation_add_remote(
        installation, remote, ifneeded, nullptr, &error);
    if (error) {
      spdlog::error("[FlatpakPlugin] Failed to add remote installation: {}",
                    error->message);
      g_object_unref(installation);
      g_object_unref(remote);
      g_clear_error(&error);
      return ErrorOr<bool>(FlutterError("INVALID_REMOTE_ADD",
                                        "Failed to add remote installation"));
    }
    if (result) {
      spdlog::info("[FlatpakPlugin] Remote {} Added Successfully",
                   configuration.name());
    }
    g_object_unref(installation);
    g_object_unref(remote);
    return static_cast<bool>(result);
  } catch (const std::exception& e) {
    spdlog::error("[FlatpakPlugin] Failed to add remote to flatpak plugin {}",
                  e.what());
    return ErrorOr<bool>(
        FlutterError("INVALID_REMOTE_ADD", "Failed to add remote to flatpak"));
  }
}

ErrorOr<bool> FlatpakShim::RemoteRemove(const std::string& id) {
  try {
    if (id.empty()) {
      return ErrorOr<bool>(
          FlutterError("INVALID_REMOTE_REMOVE", "Remote id is required"));
    }

    spdlog::info("[FlatpakPlugin] Remove remote {}", id.c_str());
    GError* error = nullptr;
    auto installation = flatpak_installation_new_user(nullptr, &error);
    if (error) {
      spdlog::error("[FlatpakPlugin] Failed to remove Remote installation: {}",
                    error->message);
      g_clear_error(&error);
      return ErrorOr<bool>(FlutterError(
          "INVALID_REMOTE_REMOVE", "Failed to remove Remote installation"));
    }
    auto result = flatpak_installation_remove_remote(installation, id.c_str(),
                                                     nullptr, &error);
    if (error) {
      spdlog::error("[FlatpakPlugin] Failed to remove remote installation: {}",
                    error->message);
      g_object_unref(installation);
      g_clear_error(&error);
      return ErrorOr<bool>(FlutterError(
          "INVALID_REMOTE_REMOVE", "Failed to remove remote installation"));
    }
    g_object_unref(installation);
    return result;
  } catch (const std::exception& e) {
    spdlog::error("[FlatpakPlugin] Failed to remove remote from {}", e.what());
    return ErrorOr<bool>(
        FlutterError("INVALID_REMOTE_REMOVE", "Failed to remove remote"));
  }
}

ErrorOr<bool> FlatpakShim::ApplicationInstall(const std::string& id) {
  try {
    if (id.empty()) {
      return ErrorOr<bool>(
          FlutterError("INVALID_APP_ID", "Application ID is required"));
    }

    spdlog::debug("[FlatpakPlugin] Installing application: {}", id);
    GError* error = nullptr;

    auto installation = flatpak_installation_new_user(nullptr, &error);
    if (error) {
      spdlog::error("[FlatpakPlugin] Failed to get user installation: {}",
                    error->message);
      g_clear_error(&error);
      return ErrorOr<bool>(FlutterError("INSTALLATION_ERROR",
                                        "Failed to get user installation"));
    }

    auto app_ref = flatpak_ref_parse(id.c_str(), &error);
    if (!error && app_ref) {
      const char* app_name = flatpak_ref_get_name(app_ref);
      const char* app_arch = flatpak_ref_get_arch(app_ref);
      const char* app_branch = flatpak_ref_get_branch(app_ref);

      spdlog::debug(
          "[FlatpakPlugin] Parsed ref - name :{} , arch:{} , branch: {}",
          app_name, app_arch, app_branch);
      std::string remote_name = FlatpakShim::find_remote_for_app(
          installation, app_name, app_arch, app_branch);

      if (remote_name.empty()) {
        spdlog::error("[FlatpakPlugin] Failed to find remote for app: {}",
                      app_name);
        g_object_unref(app_ref);
        g_object_unref(installation);
        return ErrorOr<bool>(FlutterError("INSTALL_FAILED",
                                          "Failed to find remote for app: {}"));
      }

      FlatpakInstalledRef* installed_ref = flatpak_installation_install(
          installation, remote_name.c_str(), FLATPAK_REF_KIND_APP, app_name,
          app_arch, app_branch, nullptr, nullptr, nullptr, &error);

      if (error) {
        spdlog::error("[FlatpakPlugin] Failed to install '{}': {}", id,
                      error->message);
        g_object_unref(app_ref);
        g_object_unref(installation);
        g_clear_error(&error);
        return ErrorOr<bool>(
            FlutterError("INSTALL_FAILED", "Failed to install application"));
      }

      if (installed_ref) {
        spdlog::info("[FlatpakPlugin] Successfully installed: {}", id);
        g_object_unref(installed_ref);
      }
      g_object_unref(app_ref);
      g_object_unref(installation);
      return true;
    }

    if (error) {
      g_clear_error(&error);
    }
    // search in all remotes for this app ID
    auto remote_and_ref =
        FlatpakShim::find_app_in_remotes(installation, id);

    if (remote_and_ref.first.empty()) {
      spdlog::error("[FlatpakPlugin] Application '{}' not found in any remote",
                    id.c_str());
      g_object_unref(installation);
      return ErrorOr<bool>(
          FlutterError("APP_NOT_FOUND", "Failed to find remote application"));
    }

    auto found_ref = flatpak_ref_parse(remote_and_ref.second.c_str(), &error);
    if (error || !found_ref) {
      spdlog::error("[FlatpakPlugin] Failed to parse found ref: {}",
                    remote_and_ref.second);
      g_object_unref(installation);
      if (error) {
        g_clear_error(&error);
      }
      return ErrorOr<bool>(
          FlutterError("APP_NOT_FOUND", "Failed to parse found ref"));
    }

    const char* app_name = flatpak_ref_get_name(found_ref);
    const char* app_arch = flatpak_ref_get_arch(found_ref);
    const char* app_branch = flatpak_ref_get_branch(found_ref);
    spdlog::info("[FlatpakPlugin] Found App '{}' in remote '{}' as '{}'", id,
                 remote_and_ref.first.c_str(), remote_and_ref.second.c_str());
    FlatpakInstalledRef* installed_ref = flatpak_installation_install(
        installation, remote_and_ref.first.c_str(), FLATPAK_REF_KIND_APP,
        app_name, app_arch, app_branch, nullptr, nullptr, nullptr, &error);

    if (error) {
      spdlog::error("[FlatpakPlugin] Failed to install '{}': {}", id,
                    error->message);
      g_object_unref(found_ref);
      g_object_unref(installation);
      g_clear_error(&error);
      return ErrorOr<bool>(
          FlutterError("INSTALL_FAILED", "Failed to install application"));
    }

    if (installed_ref) {
      spdlog::info("[FlatpakPlugin] Successfully installed: {}", id);
      g_object_unref(installed_ref);
    }

    g_object_unref(found_ref);
    g_object_unref(installation);
    return true;
  } catch (const std::exception& e) {
    spdlog::error("[FlatpakPlugin] Failed to install Application: {}",
                  e.what());
    return ErrorOr<bool>(FlutterError("INVALID_APPLICATION_INSTALLATION",
                                      "Failed to install remotes"));
  }
}

ErrorOr<bool> FlatpakShim::ApplicationUninstall(const std::string& id) {
  try {
    if (id.empty()) {
      return ErrorOr<bool>(
          FlutterError("INVALID_APP_ID", "Application ID is required"));
    }

    spdlog::debug("[FlatpakPlugin] Uninstalling application: {}", id);
    GError* error = nullptr;

    auto installation = flatpak_installation_new_user(nullptr, &error);
    if (error) {
      spdlog::error("[FlatpakPlugin] Failed to get user installation: {}",
                    error->message);
      g_clear_error(&error);
      return ErrorOr<bool>(FlutterError("INSTALLATION_ERROR",
                                        "Failed to get user installation"));
    }

    auto app_ref = flatpak_ref_parse(id.c_str(), &error);
    if (!error && app_ref) {
      const char* app_name = flatpak_ref_get_name(app_ref);
      const char* app_arch = flatpak_ref_get_arch(app_ref);
      const char* app_branch = flatpak_ref_get_branch(app_ref);

      spdlog::debug(
          "[FlatpakPlugin] Parsed ref - name :{} , arch:{} , branch: {}",
          app_name, app_arch, app_branch);
      gboolean result = flatpak_installation_uninstall(
          installation, FLATPAK_REF_KIND_APP, app_name, app_arch, app_branch,
          nullptr, nullptr, nullptr, &error);
      if (error) {
        spdlog::error("[FlatpakPlugin] Failed to uninstall '{}': {}", id,
                      error->message);
        g_object_unref(app_ref);
        g_object_unref(installation);
        g_clear_error(&error);
        return ErrorOr<bool>(FlutterError("UNINSTALL_FAILED",
                                          "Failed to uninstall application"));
      }

      if (result) {
        spdlog::info("[FlatpakPlugin] Successfully uninstalled: {}", id);
      }
      g_object_unref(app_ref);
      g_object_unref(installation);
      return static_cast<bool>(result);
    }

    if (error) {
      g_clear_error(&error);
    }

    // search in installed apps
    spdlog::debug(
        "[FlatpakPlugin] Could not parse as ref, searching installed apps for: "
        "{}",
        id);

    const auto refs =
        flatpak_installation_list_installed_refs(installation, nullptr, &error);
    if (error) {
      spdlog::error("[FlatpakPlugin] Failed to get installed apps: {}",
                    error->message);
      g_clear_error(&error);
      g_object_unref(installation);
      return ErrorOr<bool>(
          FlutterError("UNINSTALL_ERROR", "Failed to get installed apps"));
    }

    std::string found_app_name;
    std::string found_arch;
    std::string found_branch;
    bool app_found = false;

    // Search for exact match first, then partial match
    for (guint i = 0; i < refs->len && !app_found; i++) {
      const auto ref =
          static_cast<FlatpakInstalledRef*>(g_ptr_array_index(refs, i));
      const auto ref_name = flatpak_ref_get_name(FLATPAK_REF(ref));

      if (ref_name) {
        std::string ref_name_str(ref_name);
        // Exact match has priority
        if (ref_name_str == id) {
          found_app_name = ref_name_str;
          found_arch = flatpak_ref_get_arch(FLATPAK_REF(ref));
          found_branch = flatpak_ref_get_branch(FLATPAK_REF(ref));
          app_found = true;
          spdlog::debug("[FlatpakPlugin] Found exact match: {}", ref_name_str);
        }
      }
    }

    // If no exact match, try partial match
    if (!app_found) {
      for (guint i = 0; i < refs->len; i++) {
        const auto ref =
            static_cast<FlatpakInstalledRef*>(g_ptr_array_index(refs, i));
        const auto ref_name = flatpak_ref_get_name(FLATPAK_REF(ref));

        if (ref_name) {
          std::string ref_name_str(ref_name);
          if (ref_name_str.find(id) != std::string::npos) {
            found_app_name = ref_name_str;
            found_arch = flatpak_ref_get_arch(FLATPAK_REF(ref));
            found_branch = flatpak_ref_get_branch(FLATPAK_REF(ref));
            app_found = true;
            spdlog::debug("[FlatpakPlugin] Found partial match: {}",
                          ref_name_str);
            break;
          }
        }
      }
    }

    g_ptr_array_unref(refs);

    if (!app_found) {
      spdlog::error(
          "[FlatpakPlugin] Application '{}' not found in installed "
          "applications",
          id);
      g_object_unref(installation);
      return ErrorOr<bool>(FlutterError(
          "APP_NOT_FOUND", "Application not found in installed applications"));
    }

    spdlog::info(
        "[FlatpakPlugin] Found installed app '{}' -> name: {}, arch: {}, "
        "branch: {}",
        id, found_app_name, found_arch, found_branch);

    gboolean result = flatpak_installation_uninstall(
        installation, FLATPAK_REF_KIND_APP, found_app_name.c_str(),
        found_arch.c_str(), found_branch.c_str(), nullptr, nullptr, nullptr,
        &error);

    if (error) {
      spdlog::error("[FlatpakPlugin] Failed to uninstall '{}': {}", id,
                    error->message);
      g_object_unref(installation);
      g_clear_error(&error);
      return ErrorOr<bool>(
          FlutterError("UNINSTALL_FAILED", "Failed to uninstall application"));
    }

    if (result) {
      spdlog::info("[FlatpakPlugin] Successfully uninstalled: {}", id);
    }
    g_object_unref(installation);
    return static_cast<bool>(result);
  } catch (const std::exception& e) {
    spdlog::error("[FlatpakPlugin] Failed to uninstall application: {}",
                  e.what());
    return ErrorOr<bool>(
        FlutterError("UNINSTALL_FAILED", "Failed to uninstall application"));
  }
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

std::string FlatpakShim::find_remote_for_app(FlatpakInstallation* installation,
                                             const char* app_name,
                                             const char* app_arch,
                                             const char* app_branch) {
  GError* error = nullptr;

  auto remotes =
      flatpak_installation_list_remotes(installation, nullptr, &error);
  if (error) {
    spdlog::error("[FlatpakPlugin] Error occured while getting remotes",
                  error->message);
    g_clear_error(&error);
    return "";
  }

  for (guint i = 0; i < remotes->len; i++) {
    auto remote = static_cast<FlatpakRemote*>(g_ptr_array_index(remotes, i));
    const auto remote_name = flatpak_remote_get_name(remote);

    if (flatpak_remote_get_disabled(remote)) {
      continue;
    }

    std::string ref_string{"app/"};
    ref_string += app_name;
    ref_string += "/";
    ref_string += app_arch;
    ref_string += "/";
    ref_string += app_branch;
    auto remote_ref = flatpak_installation_fetch_remote_ref_sync(
        installation, remote_name, FLATPAK_REF_KIND_APP, app_name, app_arch,
        app_branch, nullptr, &error);
    if (error) {
      spdlog::error("[FlatpakPlugin] Error occured while fetching remote ref",
                    error->message);
      g_clear_error(&error);
      return "";
    }

    if (remote_ref) {
      g_object_unref(remote_ref);
      g_ptr_array_unref(remotes);
      return {remote_name};
    }
  }

  g_ptr_array_unref(remotes);
  return {};
}

std::pair<std::string, std::string> FlatpakShim::find_app_in_remotes(
    FlatpakInstallation* installation,
    const std::string& app_id) {
  GError* error = nullptr;
  std::vector<std::string> priority_remotes = {"flathub", "fedora",
                                               "gnome-nightly"};
  std::vector<std::string> common_branches = {"stable", "beta", "master"};
  std::string default_arch = flatpak_get_default_arch();

  for (const auto& remote_name : priority_remotes) {
    spdlog::debug("[FlatpakPlugin] Trying fetch from remote : {}", remote_name);
    for (const auto& branch_name : common_branches) {
      auto remote_ref = flatpak_installation_fetch_remote_ref_sync(
          installation, remote_name.c_str(), FLATPAK_REF_KIND_APP,
          app_id.c_str(), default_arch.c_str(), branch_name.c_str(), nullptr,
          &error);

      if (remote_ref && !error) {
        std::string ref_string{"app/"};
        ref_string += app_id;
        ref_string += "/";
        ref_string += default_arch;
        ref_string += "/";
        ref_string += branch_name;

        spdlog::info("[FlatpakPlugin] Found '{}' in remote '{}' as '{}'",
                     app_id, remote_name, ref_string);

        g_object_unref(remote_ref);
        return {remote_name, ref_string};
      }

      if (error) {
        g_clear_error(&error);
      }
    }
  }
  spdlog::debug(
      "[FlatpakPlugin] Direct fetch failed, falling back to full search");
  return find_app_in_remotes_fallback(installation, app_id);
}

std::pair<std::string, std::string> FlatpakShim::find_app_in_remotes_fallback(
    FlatpakInstallation* installation,
    const std::string& app_id) {
  GError* error = nullptr;
  auto remotes =
      flatpak_installation_list_remotes(installation, nullptr, &error);
  if (error) {
    spdlog::error("[FlatpakPlugin] Error occured while getting remotes",
                  error->message);
    g_clear_error(&error);
    return {"", ""};
  }

  // sort remotes ,flathub first
  std::vector<FlatpakRemote*> sorted_remotes;
  std::vector<FlatpakRemote*> other_remotes;
  for (guint i = 0; i < remotes->len; i++) {
    auto remote = static_cast<FlatpakRemote*>(g_ptr_array_index(remotes, i));
    if (flatpak_remote_get_disabled(remote)) {
      continue;
    }

    const char* remote_name = flatpak_remote_get_name(remote);
    if (remote_name && (strcmp(remote_name, "flathub") == 0 ||
                        strcmp(remote_name, "fedora") == 0)) {
      sorted_remotes.insert(sorted_remotes.begin(), remote);
    } else {
      other_remotes.push_back(remote);
    }
  }

  // add other remotes
  sorted_remotes.insert(sorted_remotes.end(), other_remotes.begin(),
                        other_remotes.end());

  for (auto remote : sorted_remotes) {
    const auto remote_name = flatpak_remote_get_name(remote);
    spdlog::debug("[FlatpakPlugin] Searching in remote: {}", remote_name);

    auto result = search_in_single_remote(installation, remote_name, app_id);
    if (!result.first.empty()) {
      g_ptr_array_unref(remotes);
      return result;
    }
  }
  g_ptr_array_unref(remotes);
  return {"", ""};
}

std::pair<std::string, std::string> FlatpakShim::search_in_single_remote(
    FlatpakInstallation* installation,
    const char* remote_name,
    const std::string& app_id) {
  GError* error = nullptr;
  auto remote_refs = flatpak_installation_list_remote_refs_sync(
      installation, remote_name, nullptr, &error);
  if (error) {
    spdlog::error("[FlatpakPlugin] Skipping remote '{}': {}", remote_name,
                  error->message);
    g_clear_error(&error);
    return {"", ""};
  }

  // Process in batches & use early exit
  guint batch_size = 100;
  guint processed = 0;
  for (guint j = 0; j < remote_refs->len; j++) {
    auto remote_ref =
        static_cast<FlatpakRemoteRef*>(g_ptr_array_index(remote_refs, j));

    if (flatpak_ref_get_kind(FLATPAK_REF(remote_ref)) != FLATPAK_REF_KIND_APP) {
      continue;
    }
    const char* ref_name = flatpak_ref_get_name(FLATPAK_REF(remote_ref));
    if (ref_name && app_id == std::string(ref_name)) {
      std::string ref_string = std::string("app/") + ref_name + "/" +
                               flatpak_ref_get_arch(FLATPAK_REF(remote_ref)) +
                               "/" +
                               flatpak_ref_get_branch(FLATPAK_REF(remote_ref));

      spdlog::info("[FlatpakPlugin] Found '{}' in remote '{}' as '{}'", app_id,
                   remote_name, ref_string);

      g_ptr_array_unref(remote_refs);
      return {std::string(remote_name), ref_string};
    }

    if (++processed % batch_size == 0) {
      spdlog::debug("[FlatpakPlugin] Searched {} refs in remote '{}'",
                    processed, remote_name);
    }
  }

  g_ptr_array_unref(remote_refs);
  return {"", ""};
}

flutter::EncodableList FlatpakShim::convert_remotes_to_EncodableList(
    const GPtrArray* remotes) {
  flutter::EncodableList result;

  if (!remotes) {
    spdlog::error("[FlatpakPlugin] Received null remotes array");
    return result;
  }

  for (size_t j = 0; j < remotes->len; j++) {
    const auto remote =
        static_cast<FlatpakRemote*>(g_ptr_array_index(remotes, j));
    if (!remote) {
      spdlog::error("[FlatpakPlugin] Null remote at index {}", j);
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