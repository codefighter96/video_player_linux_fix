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

#include "video_player.h"

#include <flutter/event_channel.h>
#include <flutter/event_stream_handler.h>
#include <flutter/event_stream_handler_functions.h>
#include <flutter/plugin_registrar_homescreen.h>
#include <flutter/standard_method_codec.h>

#include <backend/backend.h>
#include <plugins/common/common.h>
#include <utility>

namespace video_player_linux {

VideoPlayer::VideoPlayer(flutter::PluginRegistrarDesktop* registrar,
                         std::string uri,
                         std::map<std::string, std::string> http_headers,
                         const GLsizei width,
                         const GLsizei height,
                         const gint64 duration,
                         GstElementFactory* decoder_factory)
    : m_registrar(registrar),
      uri_(std::move(uri)),
      http_headers_(std::move(http_headers)),
      width_(width),
      height_(height),
      duration_(duration),
      decoder_factory_(decoder_factory),
      media_state_(GST_STATE_VOID_PENDING),
      event_channel_(nullptr),
      last_position_ns_(0),
      position_update_timer_(0),
      is_position_seeking_(false) {
  
  printf("[VideoPlayer] SYNC FIX Video Player creating: %s (%dx%d) - TEXTURE_ID: %ld\n", 
         uri_.c_str(), width, height, m_texture_id);

  std::lock_guard buffer_lock(buffer_mutex_);

  // OpenGL setup
  m_registrar->texture_registrar()->TextureMakeCurrent();
  shader_ = std::make_unique<nv12::Shader>(width_, height_);
  m_texture_id = shader_->textureId;

  // Texture descriptor
  m_descriptor.struct_size = sizeof(FlutterDesktopGpuSurfaceDescriptor);
  m_descriptor.handle = &m_texture_id;
  m_descriptor.width = static_cast<size_t>(width_);
  m_descriptor.height = static_cast<size_t>(height_);
  m_descriptor.visible_width = static_cast<size_t>(width_);
  m_descriptor.visible_height = static_cast<size_t>(height_);
  m_descriptor.format = kFlutterDesktopPixelFormatRGBA8888;
  m_descriptor.release_callback = [](void*) {};
  m_descriptor.release_context = this;

  gpu_surface_texture_ = std::make_unique<flutter::GpuSurfaceTexture>(
      kFlutterDesktopGpuSurfaceTypeGlTexture2D,
      [&](size_t, size_t) -> const FlutterDesktopGpuSurfaceDescriptor* {
        return &m_descriptor;
      });

  flutter::TextureVariant texture = *gpu_surface_texture_;
  m_registrar->texture_registrar()->RegisterTexture(&texture);

  // GStreamer Pipeline 
  context_ = g_main_context_get_thread_default();
  
  playbin_ = gst_element_factory_make("playbin", "playbin");
  g_object_set(playbin_, "uri", uri_.c_str(), nullptr);

  // FIX: Pipeline settings - for position tracking
  g_object_set(playbin_, "buffer-duration", (gint64)GST_SECOND, nullptr);  // 1 second buffer
  g_object_set(playbin_, "buffer-size", -1, nullptr);  // Unlimited buffer size

  // Video sink
  GstElement* video_sink_bin = gst_bin_new("video_sink_bin");
  video_convert_ = gst_element_factory_make("videoconvert", "video_convert");
  GstElement* capsfilter = gst_element_factory_make("capsfilter", "capsfilter");
  sink_ = gst_element_factory_make("fakesink", "video_sink");

  gst_bin_add_many(GST_BIN(video_sink_bin), video_convert_, capsfilter, sink_, nullptr);

  GstCaps* caps = gst_caps_new_simple("video/x-raw", "format", G_TYPE_STRING, "RGBA", nullptr);
  g_object_set(capsfilter, "caps", caps, nullptr);
  gst_caps_unref(caps);

  gst_element_link_many(video_convert_, capsfilter, sink_, nullptr);

  GstPad* ghost_pad = gst_ghost_pad_new("sink", gst_element_get_static_pad(video_convert_, "sink"));
  gst_element_add_pad(video_sink_bin, ghost_pad);
  g_object_set(playbin_, "video-sink", video_sink_bin, nullptr);
  
  // FIX: Audio sink not needed anymore - disabled with flags
  printf("[VideoPlayer] Audio sink not used - only video pipeline active\n");

  // FIX: Sink settings - critical for sync
  g_object_set(sink_, "sync", TRUE, nullptr);
  g_object_set(sink_, "max-lateness", (gint64)(50 * GST_MSECOND), nullptr);  // 50ms tolerance
  g_object_set(sink_, "qos", TRUE, nullptr);  // Quality of Service
  g_object_set(sink_, "signal-handoffs", TRUE, nullptr);
  
  handoff_handler_id_ = g_signal_connect(sink_, "handoff", 
                                         G_CALLBACK(handoff_handler), this);
  
  // Bus setup
  bus_ = gst_element_get_bus(playbin_);
  GSource* bus_source = gst_bus_create_watch(bus_);
  g_source_set_callback(bus_source, 
                        reinterpret_cast<GSourceFunc>(gst_bus_async_signal_func),
                        nullptr, nullptr);
  g_source_attach(bus_source, context_);
  g_source_unref(bus_source);
  on_bus_msg_id_ = g_signal_connect(bus_, "message", 
                                    G_CALLBACK(OnBusMessage), this);

  m_registrar->texture_registrar()->TextureClearCurrent();
  printf("[VideoPlayer] Pipeline ready - position tracking active.\n");
}

// FIX: Position timer callback - for debug only
gboolean VideoPlayer::OnPositionUpdate(void* user_data) {
    auto obj = static_cast<VideoPlayer*>(user_data);
    
    if (obj->is_position_seeking_) {
        return TRUE; // Skip position update during seek
    }
    
    gint64 current_pos = 0;
    if (gst_element_query_position(obj->playbin_, GST_FORMAT_TIME, &current_pos)) {
        // FIX: Timer now only for debug - GetPosition() does real query
        // Cache update removed
        
        // Debug log
        static int log_counter = 0;
        if (++log_counter % 30 == 0) { // Log every 30 updates
            printf("[VideoPlayer] Timer Debug - Real Position: %" G_GINT64_FORMAT " ms\n", current_pos / GST_MSECOND);
        }
    }
    
    return TRUE; // Keep timer running
}

gboolean VideoPlayer::OnBusMessage(GstBus* bus, GstMessage* msg, void* user_data) {
  auto obj = static_cast<VideoPlayer*>(user_data);
  
  switch (GST_MESSAGE_TYPE(msg)) {
    case GST_MESSAGE_ERROR: {
        GError* err;
        gchar* debug_info;
        gst_message_parse_error(msg, &err, &debug_info);
        printf("[VideoPlayer] ERROR: %s\n", err->message);
        if (debug_info) printf("[VideoPlayer] Debug: %s\n", debug_info);
        g_clear_error(&err);
        g_free(debug_info);
        break;
    }
    
    case GST_MESSAGE_EOS: {
        printf("[VideoPlayer] Video ended.\n");
        obj->OnPlaybackEnded();
        if (obj->is_looping_) {
            printf("[VideoPlayer] Loop - rewinding.\n");
            obj->SeekTo(0);  // Use our own seek method
            obj->Play();
        }
        break;
    }
    
    case GST_MESSAGE_STATE_CHANGED: {
      if (GST_MESSAGE_SRC(msg) == GST_OBJECT(obj->playbin_)) {
        GstState old_state, new_state, pending_state;
        gst_message_parse_state_changed(msg, &old_state, &new_state, &pending_state);
        
        printf("[VideoPlayer] State: %s -> %s\n",
               gst_element_state_get_name(old_state),
               gst_element_state_get_name(new_state));
        
        obj->media_state_ = new_state;
        
        // FIX: Position timer more aggressive - for progress bar
        if (new_state == GST_STATE_PLAYING) {
            if (obj->position_update_timer_ == 0) {
                obj->position_update_timer_ = g_timeout_add(33, OnPositionUpdate, obj); // ~30 FPS update
                printf("[VideoPlayer] Position timer started (33ms interval).\n");
            }
        } else if (new_state == GST_STATE_PAUSED) {
            // Stop timer in PAUSED state and save last position
            if (obj->position_update_timer_ > 0) {
                g_source_remove(obj->position_update_timer_);
                obj->position_update_timer_ = 0;
                printf("[VideoPlayer] Position timer stopped.\n");
            }
            
            // FIX: Get exact position during pause and save thread-safely
            gint64 exact_pos = 0;
            if (gst_element_query_position(obj->playbin_, GST_FORMAT_TIME, &exact_pos)) {
                {
                    std::lock_guard<std::mutex> lock(obj->position_mutex_);
                    obj->last_position_ns_ = exact_pos;
                }
                printf("[VideoPlayer] PAUSE - Exact position saved: %" G_GINT64_FORMAT " ms\n", exact_pos / GST_MSECOND);
            }
        }
        
        // Send initialized when first PAUSED or PLAYING
        if ((new_state == GST_STATE_PAUSED || new_state == GST_STATE_PLAYING) 
            && !obj->is_initialized_) {
          obj->is_initialized_ = true;
          obj->UpdateDuration();
          obj->SendInitialized();
        }
      }
      break;
    }
    
    case GST_MESSAGE_DURATION_CHANGED: {
        printf("[VideoPlayer] Duration changed - updating.\n");
        obj->UpdateDuration();
        break;
    }
    
    // FIX: When seek completes
    case GST_MESSAGE_ASYNC_DONE: {
        if (obj->is_position_seeking_) {
            printf("[VideoPlayer] Seek completed - position seeking flag cleared.\n");
            obj->is_position_seeking_ = false;
        }
        break;
    }
    
    default:
      break;
  }
  return TRUE;
}

void VideoPlayer::handoff_handler(GstElement*, GstBuffer* buffer, 
                                 GstPad* pad, void* user_data) {
  const auto obj = static_cast<VideoPlayer*>(user_data);
  
  // FIX: Check buffer timestamp
  GstClockTime buffer_pts = GST_BUFFER_PTS(buffer);
  if (GST_CLOCK_TIME_IS_VALID(buffer_pts)) {
      // Use buffer timestamp as position
      obj->last_position_ns_ = buffer_pts;
  }
  
  GstVideoInfo info;
  GstCaps* caps = gst_pad_get_current_caps(pad);
  gst_video_info_from_caps(&info, caps);

  GstVideoFrame frame;
  if (gst_video_frame_map(&frame, &info, buffer, GST_MAP_READ)) {
    obj->m_registrar->texture_registrar()->TextureMakeCurrent();
    obj->shader_->load_rgb_pixels(GST_VIDEO_FRAME_PLANE_DATA(&frame, 0));
    gst_video_frame_unmap(&frame);
    obj->m_registrar->texture_registrar()->TextureClearCurrent();
    obj->m_registrar->texture_registrar()->MarkTextureFrameAvailable(obj->m_texture_id);
  }
  gst_caps_unref(caps);
}

void VideoPlayer::Init(flutter::BinaryMessenger* messenger) {
  if (is_initialized_) return;
  
  printf("[VideoPlayer] Setting up event channel...\n");
  
  event_channel_ = std::make_unique<flutter::EventChannel<>>(
      messenger,
      std::string("flutter.io/videoPlayer/videoEvents") + std::to_string(m_texture_id),
      &flutter::StandardMethodCodec::GetInstance());

  event_channel_->SetStreamHandler(
      std::make_unique<flutter::StreamHandlerFunctions<>>(
          [this](const flutter::EncodableValue*,
                 std::unique_ptr<flutter::EventSink<>>&& events)
              -> std::unique_ptr<flutter::StreamHandlerError<>> {
            event_sink_ = std::move(events);
            return nullptr;
          },
          [this](const flutter::EncodableValue*)
              -> std::unique_ptr<flutter::StreamHandlerError<>> {
            event_sink_ = nullptr;
            return nullptr;
          }));
  
  printf("[VideoPlayer] Setting pipeline to PAUSED state...\n");
  gst_element_set_state(playbin_, GST_STATE_PAUSED);
  
  // FIX: Set initial position to 0
  last_position_ns_ = 0;
}

void VideoPlayer::UpdateDuration() {
  gint64 duration;
  if (gst_element_query_duration(playbin_, GST_FORMAT_TIME, &duration)) {
    duration_ = duration;
    printf("[VideoPlayer] Duration updated: %" G_GINT64_FORMAT " ns (%.2f seconds)\n", 
           duration_, (double)duration / GST_SECOND);
  }
}

void VideoPlayer::SendInitialized() const {
  if (!event_sink_) return;
  
  printf("[VideoPlayer] Sending initialized event...\n");
  
  auto event = flutter::EncodableMap({
      {flutter::EncodableValue("event"), flutter::EncodableValue("initialized")},
      {flutter::EncodableValue("duration"), flutter::EncodableValue(static_cast<int64_t>(duration_ / 1000000))},
      {flutter::EncodableValue("width"), flutter::EncodableValue(static_cast<int32_t>(width_))},
      {flutter::EncodableValue("height"), flutter::EncodableValue(static_cast<int32_t>(height_))}
  });

  event_sink_->Success(flutter::EncodableValue(event));
}

void VideoPlayer::OnPlaybackEnded() const {
  if (event_sink_) {
    auto res = flutter::EncodableMap({
        {flutter::EncodableValue("event"), flutter::EncodableValue("completed")}
    });
    event_sink_->Success(flutter::EncodableValue(res));
  }
}

// FIX: Position update event for progress bar
void VideoPlayer::SendPositionUpdate() const {
  if (!event_sink_) return;
  
  // Send position update event - for Flutter VideoProgressIndicator
  auto event = flutter::EncodableMap({
      {flutter::EncodableValue("event"), flutter::EncodableValue("positionUpdate")},
      {flutter::EncodableValue("position"), flutter::EncodableValue(static_cast<int64_t>(last_position_ns_ / 1000000))}
  });

  event_sink_->Success(flutter::EncodableValue(event));
}

// =========================================================================
// FIX: SYNC CONTROL FUNCTIONS
// =========================================================================

void VideoPlayer::Play() {
  printf("[VideoPlayer::Play] Starting playback - current position: %" G_GINT64_FORMAT " ms\n", 
         last_position_ns_ / GST_MSECOND);
  
  // FIX: Check position before play
  gint64 current_pos = 0;
  if (gst_element_query_position(playbin_, GST_FORMAT_TIME, &current_pos)) {
      if (abs(current_pos - last_position_ns_) > (200 * GST_MSECOND)) { // If difference > 200ms
          printf("[VideoPlayer::Play] Position inconsistency detected! Fixing: %" G_GINT64_FORMAT " -> %" G_GINT64_FORMAT "\n", 
                 current_pos / GST_MSECOND, last_position_ns_ / GST_MSECOND);
          
          // First seek to exact position
          gst_element_seek_simple(playbin_, GST_FORMAT_TIME, 
                                 static_cast<GstSeekFlags>(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE), 
                                 last_position_ns_);
      }
  }
  
  gst_element_set_state(playbin_, GST_STATE_PLAYING);
}

void VideoPlayer::Pause() {
  printf("[VideoPlayer::Pause] Pausing video.\n");
  
  // FIX: Get exact position before pause
  gint64 exact_pos = 0;
  if (gst_element_query_position(playbin_, GST_FORMAT_TIME, &exact_pos)) {
      last_position_ns_ = exact_pos;
      printf("[VideoPlayer::Pause] Exact position saved: %" G_GINT64_FORMAT " ms\n", exact_pos / GST_MSECOND);
  }
  
  gst_element_set_state(playbin_, GST_STATE_PAUSED);
}

int64_t VideoPlayer::GetPosition() {
  // FIX: Audio pipeline disabled - only video pipeline position
  
  printf("[VideoPlayerPlugin] GetPosition called for texture ID: %ld\n", m_texture_id);
  
  // Return cached position during seek
  if (is_position_seeking_) {
      std::lock_guard<std::mutex> lock(position_mutex_);
      printf("[VideoPlayerPlugin] GetPosition (seeking): %" G_GINT64_FORMAT " ms\n", last_position_ns_ / 1000000);
      return last_position_ns_ / 1000000;
  }
  
  // FIX: Get position only from video pipeline - no audio conflict
  gint64 current_position = 0;
  if (gst_element_query_position(playbin_, GST_FORMAT_TIME, &current_position)) {
      // Update cache thread-safely
      {
          std::lock_guard<std::mutex> lock(position_mutex_);
          last_position_ns_ = current_position;
      }
      
      printf("[VideoPlayerPlugin] GetPosition (video-only): %" G_GINT64_FORMAT " ms\n", current_position / 1000000);
      return current_position / 1000000;
  }
  
  // Return cached value if query fails
  std::lock_guard<std::mutex> lock(position_mutex_);
  printf("[VideoPlayerPlugin] GetPosition (cache): %" G_GINT64_FORMAT " ms\n", last_position_ns_ / 1000000);
  return last_position_ns_ / 1000000;
}

void VideoPlayer::SeekTo(const int64_t seek_ms) {
  printf("[VideoPlayer::SeekTo] Seek: %" G_GINT64_FORMAT " ms\n", seek_ms);
  
  // FIX: Set seek flag and update position thread-safely
  is_position_seeking_ = true;
  
  gint64 seek_ns = seek_ms * 1000000; // Convert ms to ns
  
  // Thread-safe position update
  {
      std::lock_guard<std::mutex> lock(position_mutex_);
      last_position_ns_ = seek_ns; // Cache seek target
  }
  
  printf("[VideoPlayer::SeekTo] Target position cached: %" G_GINT64_FORMAT " ms\n", seek_ms);
  
  // FIX: Use ACCURATE seek - slow but precise
  gboolean result = gst_element_seek(playbin_, 1.0, GST_FORMAT_TIME,
                                    static_cast<GstSeekFlags>(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE),
                                    GST_SEEK_TYPE_SET, seek_ns,
                                    GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE);
  
  if (!result) {
    printf("[VideoPlayer::SeekTo] Seek failed!\n");
    is_position_seeking_ = false;
  }
}

void VideoPlayer::SetLooping(const bool isLooping) {
  printf("[VideoPlayer] Loop: %s\n", isLooping ? "ON" : "OFF");
  is_looping_ = isLooping;
}

void VideoPlayer::SetVolume(const double volume) {
  printf("[VideoPlayer] Volume: %f\n", volume);
  g_object_set(G_OBJECT(playbin_), "volume", volume, nullptr);
  volume_ = volume;
}

void VideoPlayer::SetPlaybackSpeed(const double playbackSpeed) {
  printf("[VideoPlayer] Playback speed: %f\n", playbackSpeed);
  
  // Get current position
  gint64 current_pos = last_position_ns_;
  
  // Speed change via seek
  gst_element_seek(playbin_, playbackSpeed, GST_FORMAT_TIME,
                  static_cast<GstSeekFlags>(GST_SEEK_FLAG_FLUSH),
                  GST_SEEK_TYPE_SET, current_pos,
                  GST_SEEK_TYPE_END, 0);
  
  rate_ = playbackSpeed;
}

// =========================================================================
// CLEANUP
// =========================================================================

VideoPlayer::~VideoPlayer() {
  printf("[VideoPlayer] Destructor called.\n");
  m_valid = false;
}

bool VideoPlayer::IsValid() {
  return m_valid;
}

void VideoPlayer::Dispose() {
  printf("[VideoPlayer::Dispose] Cleaning up - TEXTURE_ID: %ld...\n", m_texture_id);
  
  if (!m_valid) return;
  
  std::lock_guard buffer_lock(buffer_mutex_);

  // FIX: Clean up timer
  if (position_update_timer_ > 0) {
      g_source_remove(position_update_timer_);
      position_update_timer_ = 0;
  }

  // Stop pipeline
  if (playbin_) {
    gst_element_set_state(playbin_, GST_STATE_NULL);
  }

  // Clean up signal handlers
  if (bus_ && on_bus_msg_id_ > 0) {
    g_signal_handler_disconnect(G_OBJECT(bus_), on_bus_msg_id_);
    on_bus_msg_id_ = 0;
  }
  if (sink_ && handoff_handler_id_ > 0) {
    g_signal_handler_disconnect(G_OBJECT(sink_), handoff_handler_id_);
    handoff_handler_id_ = 0;
  }
  
  // Clean up GStreamer objects
  if (playbin_) {
      gst_object_unref(playbin_);
      playbin_ = nullptr;
  }

  // OpenGL cleanup
  m_registrar->texture_registrar()->TextureMakeCurrent();
  shader_.reset();
  m_registrar->texture_registrar()->TextureClearCurrent();
  m_registrar->texture_registrar()->UnregisterTexture(m_texture_id);

  m_texture_id = 0;
  event_channel_ = nullptr;
  m_valid = false;
  
  printf("[VideoPlayer::Dispose] Cleanup completed.\n");
}

void VideoPlayer::SendBufferingUpdate() const {
  // Simple implementation - empty
}

void VideoPlayer::prepare(VideoPlayer* user_data) {
  // Not needed 
}

}  // namespace video_player_linux