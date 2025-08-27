/*
 * Copyright 2024 Toyota Connected North America
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

#ifndef PLUGINS_FLATPAK_SCREENSHOT_H
#define PLUGINS_FLATPAK_SCREENSHOT_H

#include <libxml/tree.h>
#include <optional>
#include <string>
#include <vector>

class Image {
 public:
  explicit Image(const xmlNode* node);

  [[nodiscard]] const std::optional<std::string>& getType() const;

  [[nodiscard]] const std::optional<int>& getWidth() const;

  [[nodiscard]] const std::optional<int>& getHeight() const;

  [[nodiscard]] const std::optional<std::string>& getUrl() const;

  void printImageDetails() const;

 private:
  std::optional<std::string> type_;
  std::optional<int> width_;
  std::optional<int> height_;
  std::optional<std::string> url_;

  void parseXmlNode(const xmlNode* node);
};

class Video {
 public:
  explicit Video(const xmlNode* node);

  [[nodiscard]] const std::optional<std::string>& getContainer() const;

  [[nodiscard]] const std::optional<std::string>& getCodec() const;

  [[nodiscard]] const std::optional<int>& getWidth() const;

  [[nodiscard]] const std::optional<int>& getHeight() const;

  [[nodiscard]] const std::optional<std::string>& getUrl() const;

  void printVideoDetails() const;

 private:
  std::optional<std::string> container_;
  std::optional<std::string> codec_;
  std::optional<int> width_;
  std::optional<int> height_;
  std::optional<std::string> url_;

  void parseXmlNode(const xmlNode* node);
};

class Screenshot {
 public:
  explicit Screenshot(const xmlNode* node);

  [[nodiscard]] const std::optional<std::string>& getType() const;

  [[nodiscard]] const std::vector<std::string>& getCaptions() const;

  [[nodiscard]] const std::optional<std::vector<Image>>& getImages() const;

  [[nodiscard]] const std::optional<Video>& getVideo() const;

  void printScreenshotDetails() const;

 private:
  std::optional<std::string> type_;
  std::vector<std::string> captions_;
  std::optional<std::vector<Image>> images_;
  std::optional<Video> video_;

  void parseXmlNode(const xmlNode* node);
};

#endif  // PLUGINS_FLATPAK_SCREENSHOT_H
