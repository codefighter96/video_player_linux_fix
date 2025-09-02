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

#pragma once

#include <functional>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>

#include <flutter/event_channel.h>
#include <flutter/event_stream_handler.h>
#include <flutter/event_stream_handler_functions.h>
#include <flutter/plugin_registrar_homescreen.h>
#include <flutter/standard_method_codec.h>

#include "nv12.h"

extern "C" {
#include <gst/gst.h>
#include <gst/video/video.h>
//#include <libavformat/avformat.h>
}

#include "messages.g.h"

class Backend;

namespace video_player_linux {

class VideoPlayer {
 public:
  VideoPlayer(flutter::PluginRegistrarDesktop* registrar,
              std::string uri,
              std::map<std::string, std::string> http_headers,
              GLsizei width,
              GLsizei height,
              gint64 duration,
              GstElementFactory* decoder_factory);
  ~VideoPlayer();

  void Dispose();
  void SetLooping(bool isLooping);
  void SetVolume(double volume);
  void SetPlaybackSpeed(double playbackSpeed);
  void Play();
  void Pause();
  int64_t GetPosition();
  void SendBufferingUpdate() const;
  void SeekTo(int64_t seek);
  int64_t GetTextureId() const { return m_texture_id; };
  bool IsValid();

  // Initializes the video player.
  void Init(flutter::BinaryMessenger* messenger);

 private:
  flutter::PluginRegistrarDesktop* m_registrar;
  std::string uri_;
  std::map<std::string, std::string> http_headers_;
  GLsizei width_{};
  GLsizei height_{};
  gint64 duration_{};
  GstElementFactory* decoder_factory_;

  GLuint m_texture_id{};
  std::atomic<bool> m_valid = true;
  flutter::TextureRegistrar* m_texture_registry{};
  std::unique_ptr<flutter::GpuSurfaceTexture> gpu_surface_texture_;

  GMainContext* context_;
  GstState media_state_;

  // GStreamer components
  GstElement* playbin_{};
  GstElement* sink_{};
  GstElement* video_convert_{};
  GstBus* bus_{};

  gulong handoff_handler_id_;
  gulong on_bus_msg_id_;

  // Player state
  std::unique_ptr<nv12::Shader> shader_;
  bool is_looping_{};
  double volume_ = 1.0;
  gdouble rate_ = 1.0;
  bool is_initialized_ = false;

  // FIX: Position tracking için yeni değişkenler
  gint64 last_position_ns_ = 0;        // Son bilinen kesin pozisyon (nanosaniye)
  guint position_update_timer_ = 0;    // Position güncelleme timer'ı
  bool is_position_seeking_ = false;   // Seek işlemi sırasında true
  std::mutex position_mutex_;          // Position thread safety için

  std::mutex gst_mutex_;

  // Flutter integration
  FlutterDesktopGpuSurfaceDescriptor m_descriptor{};
  std::mutex buffer_mutex_;
  std::unique_ptr<flutter::EventChannel<flutter::EncodableValue>> event_channel_;
  std::unique_ptr<flutter::EventSink<flutter::EncodableValue>> event_sink_;

  // Helper methods
  void UpdateDuration();
  void SendInitialized() const;
  void OnPlaybackEnded() const;
  void SendPositionUpdate() const;  // FIX: Progress bar için position update

  // FIX: Position tracking callback
  static gboolean OnPositionUpdate(void* user_data);

  // Static callbacks
  static void handoff_handler(GstElement* fakesink,
                              GstBuffer* buffer,
                              GstPad* pad,
                              void* user_data);

  static gboolean OnBusMessage(GstBus* bus, GstMessage* msg, void* user_data);

  // Unused in simple version
  static void prepare(VideoPlayer* user_data);
};

}  // namespace video_player_linux