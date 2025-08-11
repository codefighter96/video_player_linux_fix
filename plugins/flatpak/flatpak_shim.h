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

#ifndef FLUTTER_PLUGIN_FLATPAK_FLATPAK_SHIM_H
#define FLUTTER_PLUGIN_FLATPAK_FLATPAK_SHIM_H

#include <filesystem>
#include <optional>
#include <string>

#define FLATPAK_EXTERN extern "C"
#include <flatpak/flatpak.h>
#include <glib/garray.h>

#include "component.h"
#include "messages.g.h"

namespace flatpak_plugin {

/**
 * \brief A utility class providing various helper functions for interacting
 * with Flatpak installations, remotes, and applications.
 */
struct FlatpakShim {
  static constexpr size_t BUFFER_SIZE =
      32768;  ///< Buffer size used for decompressing.

  /**
   * \brief Retrieves an optional attribute from an XML node.
   * \param node Pointer to the XML node.
   * \param attrName Name of the attribute to retrieve.
   * \return The attribute value as an optional string, or std::nullopt if not
   * found.
   */
  static std::optional<std::string> getOptionalAttribute(const xmlNode* node,
                                                         const char* attrName);

  /**
   * \brief Retrieves a required attribute from an XML node.
   * \param node Pointer to the XML node.
   * \param attrName Name of the attribute to retrieve.
   * \return The attribute value as a string.
   * \throws std::runtime_error if the attribute is not found.
   */
  static std::string getAttribute(const xmlNode* node, const char* attrName);

  /**
   * \brief Prints the details of a Component object to the standard output.
   * \param component The Component object to print.
   */
  static void PrintComponent(const Component& component);

  /**
   * \brief Retrieves the system installations available on the host.
   * \return A GPtrArray containing pointers to FlatpakInstallation objects.
   */
  static GPtrArray* get_system_installations();

  /**
   * \brief Retrieves the remotes associated with a given Flatpak installation.
   * \param installation Pointer to the FlatpakInstallation object.
   * \return A GPtrArray containing pointers to FlatpakRemote objects.
   */
  static GPtrArray* get_remotes(FlatpakInstallation* installation);

  /**
   * \brief Retrieves the AppStream timestamp from a given file path.
   * \param timestamp_filepath The path to the timestamp file.
   * \return The timestamp as a std::time_t value.
   */
  static std::time_t get_appstream_timestamp(
      const std::filesystem::path& timestamp_filepath);

  /**
   * \brief Formats a time value into an ISO 8601 string.
   * \param raw_time The raw time value to format.
   * \param buffer The buffer to store the formatted string.
   * \param buffer_size The size of the buffer.
   */
  static void format_time_iso8601(time_t raw_time,
                                  char* buffer,
                                  size_t buffer_size);

  /**
   * \brief Retrieves the default languages for a Flatpak installation.
   * \param installation Pointer to the FlatpakInstallation object.
   * \return A flutter::EncodableList containing the default languages.
   */
  static flutter::EncodableList installation_get_default_languages(
      FlatpakInstallation* installation);

  /**
   * \brief Retrieves the default locales for a Flatpak installation.
   * \param installation Pointer to the FlatpakInstallation object.
   * \return A flutter::EncodableList containing the default locales.
   */
  static flutter::EncodableList installation_get_default_locales(
      FlatpakInstallation* installation);

  /**
   * \brief Converts a FlatpakInstallation object into an Installation object.
   * \param installation Pointer to the FlatpakInstallation object.
   * \return The corresponding Installation object.
   */
  static Installation get_installation(FlatpakInstallation* installation);

  /**
   * \brief Retrieves the content rating map for a FlatpakInstalledRef object.
   * \param ref Pointer to the FlatpakInstalledRef object.
   * \return A flutter::EncodableMap containing the content rating information.
   */
  static flutter::EncodableMap get_content_rating_map(FlatpakInstalledRef* ref);

  /**
   * \brief Populates a list of applications installed in a Flatpak
   * installation. \param installation Pointer to the FlatpakInstallation
   * object. \param application_list The list to populate with application
   * details.
   */
  static void get_application_list(FlatpakInstallation* installation,
                                   flutter::EncodableList& application_list);

  /**
   * \brief Retrieves the user installation.
   * \return An ErrorOr object containing the Installation or an error.
   */
  static ErrorOr<Installation> GetUserInstallation();

  /**
   * \brief Retrieves the system installations as an EncodableList.
   * \return An ErrorOr object containing the EncodableList or an error.
   */
  static ErrorOr<flutter::EncodableList> GetSystemInstallations();

  /**
   * \brief Retrieves the list of installed applications.
   * \return An ErrorOr object containing the EncodableList or an error.
   */
  static ErrorOr<flutter::EncodableList> GetApplicationsInstalled();

  /**
   * \brief Retrieves the remotes for a given installation ID.
   * \param installation_id The ID of the installation.
   * \return An ErrorOr object containing the EncodableList or an error.
   */
  static ErrorOr<flutter::EncodableList> get_remotes_by_installation_id(
      const std::string& installation_id);

  /**
   * \brief Converts a FlatpakRemoteType to its string representation.
   * \param type The FlatpakRemoteType to convert.
   * \return The string representation of the remote type.
   */
  static std::string FlatpakRemoteTypeToString(FlatpakRemoteType type);

  /**
   * \brief Converts a list of remotes into an EncodableList.
   * \param remotes Pointer to a GPtrArray containing the remotes.
   * \return A flutter::EncodableList containing the remotes.
   */
  static flutter::EncodableList convert_remotes_to_EncodableList(
      const GPtrArray* remotes);

  /**
   * \brief Retrieves the metadata of a FlatpakInstalledRef as a string.
   * \param installed_ref Pointer to the FlatpakInstalledRef object.
   * \return The metadata as a string.
   */
  static std::string get_metadata_as_string(FlatpakInstalledRef* installed_ref);

  /**
   * \brief Retrieves the appdata of a FlatpakInstalledRef as a string.
   * \param installed_ref Pointer to the FlatpakInstalledRef object.
   * \return The appdata as a string.
   */
  static std::string get_appdata_as_string(FlatpakInstalledRef* installed_ref);

  /**
   * \brief Decompresses a GZip-compressed data buffer.
   * \param compressedData The compressed data buffer.
   * \param decompressedData The buffer to store the decompressed data.
   * \return A vector containing the decompressed data.
   */
  static std::vector<char> decompress_gzip(
      const std::vector<char>& compressedData,
      std::vector<char>& decompressedData);
};

}  // namespace flatpak_plugin

#endif  // FLUTTER_PLUGIN_FLATPAK_FLATPAK_SHIM_H
