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

#define GSTREAMER_DEBUG 0

namespace video_player_linux {

typedef enum {
  GST_PLAY_FLAG_AUDIO = 1 << 0,
  GST_PLAY_FLAG_VIDEO = 1 << 1,
  GST_PLAY_FLAG_TEXT = 1 << 2
} GstPlayFlags;

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
      event_channel_(nullptr) {
  SPDLOG_DEBUG(
      "[VideoPlayer] uri: {}, http_headers: {}, size: {} x {}, duration: {}",
      uri.c_str(), http_headers_.size(), width, height, duration);

  std::lock_guard buffer_lock(buffer_mutex_);

  /// Setup OpenGL

  m_registrar->texture_registrar()->TextureMakeCurrent();
  shader_ = std::make_unique<nv12::Shader>(width_, height_);
  m_texture_id = shader_->textureId;

  /// Setup GL Texture 2D

  m_descriptor.struct_size = sizeof(FlutterDesktopGpuSurfaceDescriptor);
  m_descriptor.handle = &m_texture_id;
  m_descriptor.width = static_cast<size_t>(width_);
  m_descriptor.height = static_cast<size_t>(height_);
  m_descriptor.visible_width = static_cast<size_t>(width_);
  m_descriptor.visible_height = static_cast<size_t>(height_);
  m_descriptor.format = kFlutterDesktopPixelFormatRGBA8888;
  m_descriptor.release_callback = [](void* /* release_context */) {};
  m_descriptor.release_context = this;

  gpu_surface_texture_ = std::make_unique<flutter::GpuSurfaceTexture>(
      kFlutterDesktopGpuSurfaceTypeGlTexture2D,
      [&](size_t /* width */,
          size_t /* height */) -> const FlutterDesktopGpuSurfaceDescriptor* {
        return &m_descriptor;
      });

  flutter::TextureVariant texture = *gpu_surface_texture_;
  m_registrar->texture_registrar()->RegisterTexture(&texture);

  /// Setup GST Pipeline

  context_ = g_main_context_get_thread_default();

  playbin_ = gst_element_factory_make("playbin", nullptr);
  assert(playbin_);
  g_object_set(playbin_, "uri", uri_.c_str(), nullptr);

  if (!http_headers_.empty()) {
    std::stringstream ss;
    for (auto& [key, value] : http_headers_) {
      ss << key << ":" << value << " ";
    }
    SPDLOG_DEBUG("extra-headers: {}", ss.str().c_str());
    GstStructure* extraHeaders =
        gst_structure_from_string(ss.str().c_str(), nullptr);
    if (extraHeaders != nullptr) {
      g_object_set(playbin_, "extra-headers", extraHeaders, nullptr);
      gst_structure_free(extraHeaders);
    }
  }

  gint flags = 0;
  g_object_get(playbin_, "flags", &flags, nullptr);
  flags |= GST_PLAY_FLAG_VIDEO | GST_PLAY_FLAG_AUDIO;
  flags &= ~GST_PLAY_FLAG_TEXT;
  g_object_set(playbin_, "flags", flags, nullptr);
  g_object_set(playbin_, "connection-speed", 56, nullptr);

  sink_ = gst_element_factory_make("fakesink", nullptr);
  assert(sink_);
  g_object_set(sink_, "sync", TRUE, nullptr);
  g_object_set(sink_, "signal-handoffs", TRUE, nullptr);
  g_object_set(sink_, "can-activate-pull", TRUE, nullptr);
  handoff_handler_id_ = g_signal_connect(
      sink_, "handoff", reinterpret_cast<GCallback>(handoff_handler), this);

  decoder_ = gst_element_factory_create(decoder_factory, "decoder");
  assert(decoder_);

  video_convert_ = gst_element_factory_make("videoconvert", nullptr);
  assert(video_convert_);

  GstCaps* caps = gst_caps_new_simple("video/x-raw", "format", G_TYPE_STRING,
                                      "NV12", nullptr);

  video_scale_ = gst_element_factory_make("videoscale", nullptr);
  assert(video_scale_);

  GstCaps* scale =
      gst_caps_new_simple("video/x-raw", "width", G_TYPE_INT, width_, "height",
                          G_TYPE_INT, height_, nullptr);

  pipeline_ = gst_bin_new(nullptr);

  gst_bin_add_many(reinterpret_cast<GstBin*>(pipeline_), decoder_,
                   video_convert_, video_scale_, sink_, nullptr);
  if (!gst_element_link(decoder_, video_convert_)) {
    SPDLOG_ERROR("[VideoPlayer] Failed to link decoder with videoconvert");
  }

  if (!gst_element_link_filtered(video_convert_, video_scale_, caps)) {
    SPDLOG_ERROR(
        "[VideoPlayer] Failed to link videoconvert with videoscale using "
        "filter");
  }

  gst_caps_unref(caps);

  GstPad* pad = gst_element_get_static_pad(decoder_, "sink");
  if (gst_pad_is_linked(pad)) {
    SPDLOG_ERROR("[VideoPlayer] already linked, ignore");
    return;
  }
  GstPad* ghost_pad = gst_ghost_pad_new("sink", pad);
  gst_pad_set_active(ghost_pad, TRUE);
  gst_element_add_pad(pipeline_, ghost_pad);
  gst_object_unref(pad);

  if (!gst_element_link_filtered(video_scale_, sink_, scale)) {
    SPDLOG_ERROR(
        "[VideoPlayer] Failed to link videoscale with fakesink using filter");
  }
  gst_caps_unref(scale);

  g_object_set(playbin_, "video-sink", pipeline_, nullptr);

  bus_ = gst_element_get_bus(playbin_);
  GSource* bus_source = gst_bus_create_watch(bus_);
  g_source_set_callback(
      bus_source, reinterpret_cast<GSourceFunc>(gst_bus_async_signal_func),
      nullptr, nullptr);
  g_source_attach(bus_source, context_);
  g_source_unref(bus_source);
  on_bus_msg_id_ = g_signal_connect(
      bus_, "message", reinterpret_cast<GCallback>(OnBusMessage), this);

  m_registrar->texture_registrar()->TextureClearCurrent();
}

void VideoPlayer::OnMediaError(GstMessage* msg) {
  GError* err;
  gchar* debug_info;
  gst_message_parse_error(msg, &err, &debug_info);
  spdlog::error("[VideoPlayer] Error: {}:{}", GST_OBJECT_NAME(msg->src),
                err->message);
  if (debug_info) {
    spdlog::error("[VideoPlayer] {}", debug_info);
    g_free(debug_info);
  }
  g_clear_error(&err);
}

void VideoPlayer::OnMediaDurationChange() {
  GstQuery* query = gst_query_new_duration(GST_FORMAT_TIME);
  if (gst_element_query(playbin_, query)) {
    gst_query_parse_duration(query, nullptr, &duration_);
  }
  gst_query_unref(query);
}

gboolean VideoPlayer::OnBusMessage(GstBus* bus,
                                   GstMessage* msg,
                                   void* user_data) {
  auto obj = static_cast<VideoPlayer*>(user_data);
  switch (GST_MESSAGE_TYPE(msg)) {
    case GST_MESSAGE_ERROR:
      OnMediaError(msg);
      gst_object_unref(bus);
      return FALSE;
    case GST_MESSAGE_EOS: {
      SPDLOG_DEBUG("[VideoPlayer] EOS: texture_id: {}", obj->m_texture_id);
      obj->OnPlaybackEnded();
      break;
    }
    case GST_MESSAGE_STATE_CHANGED: {
      GstState old_state, new_state, pending_state;
      gst_message_parse_state_changed(msg, &old_state, &new_state,
                                      &pending_state);
      if (GST_MESSAGE_SRC(msg) == GST_OBJECT(obj->playbin_)) {
        obj->OnMediaStateChange(new_state);
      }
      break;
    }
    case GST_MESSAGE_DURATION_CHANGED: {
      obj->OnMediaDurationChange();
      break;
    }
#if 0
    case GST_MESSAGE_LATENCY: {
      auto src = GST_MESSAGE_SRC(msg);
      SPDLOG_DEBUG("[VideoPlayer] Latency: {}", src->name);
      GstQuery* query;
      gboolean res;
      query = gst_query_new_latency();
      res = gst_element_query(obj->playbin_, query);
      if (res) {
        GstClockTime min_latency;
        GstClockTime max_latency;
        gst_query_parse_latency(query, &obj->is_live_, &min_latency,
                                &max_latency);
      }
      gst_query_unref(query);
      break;
    }
#endif
    case GST_MESSAGE_WARNING: {
      spdlog::warn("[VideoPlayer] Warning");
      break;
    }
    case GST_MESSAGE_ASYNC_DONE: {
      SPDLOG_DEBUG("[VideoPlayer] Async Done");
      // bufferingEnd
      break;
    }
    case GST_MESSAGE_NEW_CLOCK: {
      GstClock* clock;
      gst_message_parse_new_clock(msg, &clock);
      const GstClockTime time = gst_clock_get_time(clock);
      (void)time;
      SPDLOG_DEBUG("[VideoPlayer] New Clock: {}", time);
      break;
    }
    case GST_MESSAGE_BUFFERING: {
      // no state management needed for live pipelines
      if (obj->is_live_)
        break;

      gint percent;
      gst_message_parse_buffering(msg, &percent);
      // SPDLOG_DEBUG("Buffering: {}%", percent);

      // TODO - bufferingUpdate

      if (percent == 100) {
        // a 100% message means buffering is done
        if (obj->is_buffering_) {
          obj->is_buffering_ = false;
          obj->SetBuffering(obj->is_buffering_);
        }
        // if the desired state is playing, go back
        //        if (obj->target_state_ == GST_STATE_PLAYING) {
        //          gst_element_set_state(obj->playbin_, GST_STATE_PLAYING);
        //        }
      } else {
        // buffering busy
        if (!obj->is_buffering_ && obj->target_state_ == GST_STATE_PLAYING) {
          // we were not buffering but PLAYING, PAUSE the pipeline
          //          gst_element_set_state(obj->playbin_, GST_STATE_PAUSED);
        }
        // if (!obj->is_buffering_) {
        obj->is_buffering_ = true;
        obj->SetBuffering(obj->is_buffering_);
        //}
      }
      break;
    }
#if GSTREAMER_DEBUG
    case GST_MESSAGE_STREAM_STATUS: {
      GstStreamStatusType type;
      GstElement* owner;
      const GValue* val;
      gchar* path;
      GstTask* task = nullptr;

      SPDLOG_DEBUG("STREAM_STATUS:");
      gst_message_parse_stream_status(msg, &type, &owner);

      val = gst_message_get_stream_status_object(msg);

      SPDLOG_DEBUG("\ttype:   {}", static_cast<guint>(type));
      path = gst_object_get_path_string(GST_MESSAGE_SRC(msg));
      SPDLOG_DEBUG("\tsource: {}", path);
      g_free(path);
      path = gst_object_get_path_string(GST_OBJECT(owner));
      SPDLOG_DEBUG("\towner:  {}", path);
      g_free(path);
      SPDLOG_DEBUG("\tobject: type {}, value {}", G_VALUE_TYPE_NAME(val),
                   g_value_get_object(val));

      /* see if we know how to deal with this object */
      if (G_VALUE_TYPE(val) == GST_TYPE_TASK) {
        task = GST_TASK(g_value_get_object(val));
      }

      switch (type) {
        case GST_STREAM_STATUS_TYPE_CREATE:
          SPDLOG_DEBUG("Created task: {}", fmt::ptr(task));
          break;
        case GST_STREAM_STATUS_TYPE_ENTER:
          SPDLOG_DEBUG("raising task priority");
          // setpriority (PRIO_PROCESS, 0, -10);
          break;
        case GST_STREAM_STATUS_TYPE_LEAVE:
        default:
          break;
      }
      break;
    }
    case GST_MESSAGE_RESET_TIME: {
      GstClockTime running_time;
      gst_message_parse_reset_time(msg, &running_time);
      if (running_time > 0) {
        g_print("reset-time: %" GST_TIME_FORMAT, GST_TIME_ARGS(running_time));
      }
      break;
    }
    // element specific message
    case GST_MESSAGE_ELEMENT: {
      SPDLOG_DEBUG("message-element: {}",
                   gst_structure_get_name(gst_message_get_structure(msg)));
      break;
    }
    case GST_MESSAGE_TAG: {
#if GSTREAMER_DEBUG
      GstTagList* tags = nullptr;
      gst_message_parse_tag(msg, &tags);
      gst_tag_list_foreach(tags, VideoPlayer::OnTag, obj);
      gst_tag_list_unref(tags);
#endif
      break;
    }
#endif
    default:
#if GSTREAMER_DEBUG
    {
      SPDLOG_DEBUG("GST Message Type: {}",
                   gst_message_type_get_name(GST_MESSAGE_TYPE(msg)));
      break;
    }
#else
        ;
#endif
  }
  return TRUE;
}

void VideoPlayer::OnTag(const GstTagList* list,
                        const gchar* tag,
                        gpointer /* user_data */) {
  const std::string tag_str = tag;
  if (const auto type = gst_tag_get_type(tag);
      tag_str == "audio-codec" && type == 64) {
    gchar* value;
    gst_tag_list_get_string(list, tag, &value);
    spdlog::debug("[VideoPlayer] audio-codec: {}", value);
  } else if (tag_str == "video-codec" && type == 64) {
    gchar* value;
    gst_tag_list_get_string(list, tag, &value);
    spdlog::debug("[VideoPlayer] video-codec: {}", value);
  } else if (tag_str == "maximum-bitrate" && type == 28) {
    guint value;
    gst_tag_list_get_uint(list, tag, &value);
    spdlog::debug("[VideoPlayer] maximum-bitrate: {}", value);
  } else if (tag_str == "minimum-bitrate" && type == 28) {
    guint value;
    gst_tag_list_get_uint(list, tag, &value);
    spdlog::debug("[VideoPlayer] minimum-bitrate: {}", value);
  } else if (tag_str == "bitrate" && type == 28) {
    guint value;
    gst_tag_list_get_uint(list, tag, &value);
    spdlog::debug("[VideoPlayer] bitrate: {}", value);
  }
}

void VideoPlayer::handoff_handler(GstElement* /* fakesink */,
                                  GstBuffer* buffer,
                                  GstPad* /* pad */,
                                  void* user_data) {
  const auto obj = static_cast<VideoPlayer*>(user_data);
  if (!obj->is_initialized_ || obj->info_.finfo == nullptr) {
    return;
  }

  std::lock_guard lock(obj->gst_mutex_);
  GstVideoFrame frame;
  if (gst_video_frame_map(&frame, &obj->info_, buffer, GST_MAP_READ)) {
    obj->m_registrar->texture_registrar()->TextureMakeCurrent();
    glBindVertexArray(obj->shader_->vertex_arr_id_);
    glClear(GL_COLOR_BUFFER_BIT);

    if (const guint n_planes = GST_VIDEO_INFO_N_PLANES(&obj->info_);
        n_planes == 2) {
      // Assume NV12
      obj->shader_->load_pixels(GST_VIDEO_FRAME_PLANE_DATA(&frame, 0),
                                GST_VIDEO_FRAME_PLANE_DATA(&frame, 1),
                                GST_VIDEO_FRAME_COMP_PSTRIDE(&frame, 0),
                                GST_VIDEO_FRAME_PLANE_STRIDE(&frame, 0),
                                GST_VIDEO_FRAME_COMP_PSTRIDE(&frame, 1),
                                GST_VIDEO_FRAME_PLANE_STRIDE(&frame, 1));
    } else {
      // Assume RGB
      obj->shader_->load_rgb_pixels(GST_VIDEO_FRAME_PLANE_DATA(&frame, 0));
    }
    gst_video_frame_unmap(&frame);

    glBindFramebuffer(GL_FRAMEBUFFER, obj->shader_->framebuffer);
    obj->shader_->draw_core();

    obj->m_registrar->texture_registrar()->TextureClearCurrent();
    obj->m_registrar->texture_registrar()->MarkTextureFrameAvailable(
        obj->m_texture_id);
    SPDLOG_TRACE("[VideoPlayer] frame");
  } else {
    SPDLOG_ERROR("[VideoPlayer] Cannot read video frame out from buffer");
  }
}

void VideoPlayer::Init(flutter::BinaryMessenger* messenger) {
  if (is_initialized_) {
    return;
  }

  SPDLOG_DEBUG("[VideoPlayer] Init");
  event_channel_ = std::make_unique<flutter::EventChannel<>>(
      messenger,
      std::string("flutter.io/videoPlayer/videoEvents") +
          std::to_string(m_texture_id),
      &flutter::StandardMethodCodec::GetInstance());

  event_channel_->SetStreamHandler(
      std::make_unique<flutter::StreamHandlerFunctions<>>(
          [this](const flutter::EncodableValue* /* arguments */,
                 std::unique_ptr<flutter::EventSink<>>&& events)
              -> std::unique_ptr<flutter::StreamHandlerError<>> {
            event_sink_ = std::move(events);
            return nullptr;
          },
          [this](const flutter::EncodableValue* /* arguments */)
              -> std::unique_ptr<flutter::StreamHandlerError<>> {
            event_sink_ = nullptr;
            return nullptr;
          }));
}

VideoPlayer::~VideoPlayer() {
  m_valid = false;
}

bool VideoPlayer::IsValid() {
  return m_valid;
}

void VideoPlayer::SendInitialized() const {
  if (!event_sink_) {
    return;
  }
  auto event = flutter::EncodableMap(
      {{flutter::EncodableValue("event"),
        flutter::EncodableValue("initialized")},
       {flutter::EncodableValue("duration"),
        flutter::EncodableValue(static_cast<int64_t>(duration_))}});

  event.insert({flutter::EncodableValue("width"),
                flutter::EncodableValue(static_cast<int32_t>(width_))});
  event.insert({flutter::EncodableValue("height"),
                flutter::EncodableValue(static_cast<int32_t>(height_))});

  event_sink_->Success(flutter::EncodableValue(event));
}

void VideoPlayer::OnPlaybackEnded() const {
  if (this->event_sink_) {
    SPDLOG_DEBUG("[VideoPlayer] OnPlaybackEnded");
    auto res = flutter::EncodableMap({{flutter::EncodableValue("event"),
                                       flutter::EncodableValue("completed")}});
    this->event_sink_->Success(flutter::EncodableValue(res));
  }
}

void VideoPlayer::OnMediaStateChange(const GstState state) {
  if (state == GST_STATE_NULL) {
    SetBuffering(true);
    SendBufferingUpdate();
  } else {
    if (!is_initialized_) {
      is_initialized_ = true;
      SendInitialized();
    }
    SetBuffering(false);

    if (state == GST_STATE_PLAYING) {
      SPDLOG_DEBUG("[VideoPlayer] message state changed, start playing {}",
                   m_texture_id);
      prepare(this);
    } else if (state == GST_STATE_READY) {
      SPDLOG_DEBUG("[VideoPlayer] message state changed, ready {}",
                   m_texture_id);
    }
  }
}

void VideoPlayer::SetBuffering(const bool buffering) const {
  if (this->event_sink_) {
    SPDLOG_DEBUG("[VideoPlayer] SetBuffering: {}", buffering);
    auto res = flutter::EncodableMap(
        {{flutter::EncodableValue("event"),
          flutter::EncodableValue(buffering ? "bufferingStart"
                                            : "bufferingEnd")}});
    event_sink_->Success(flutter::EncodableValue(res));
  }
}

void VideoPlayer::Dispose() {
  SPDLOG_DEBUG("[VideoPlayer] Dispose");
  std::lock_guard buffer_lock(buffer_mutex_);

  if (is_initialized_) {
    Pause();
  }

  g_signal_handler_disconnect(G_OBJECT(bus_), on_bus_msg_id_);
  g_signal_handler_disconnect(G_OBJECT(sink_), handoff_handler_id_);

  m_registrar->texture_registrar()->TextureMakeCurrent();
  shader_.reset();
  m_registrar->texture_registrar()->TextureClearCurrent();

  m_registrar->texture_registrar()->UnregisterTexture(m_texture_id);

  m_texture_id = 0;
  event_channel_ = nullptr;
  m_valid = false;
}

void VideoPlayer::SetLooping(const bool isLooping) {
  SPDLOG_DEBUG("[VideoPlayer] SetLooping: {}", is_looping_);
  is_looping_ = isLooping;
}

void VideoPlayer::SetVolume(const double volume) {
  SPDLOG_DEBUG("[VideoPlayer] SetVolume: {}", volume_);
  volume_ = volume;
}

void VideoPlayer::SetPlaybackSpeed(const double playbackSpeed) {
  GstEvent* seek_event;
  if (playbackSpeed > 0) {
    seek_event = gst_event_new_seek(
        playbackSpeed, GST_FORMAT_TIME,
        static_cast<GstSeekFlags>(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE),
        GST_SEEK_TYPE_SET, position_, GST_SEEK_TYPE_END, 0);
  } else {
    seek_event = gst_event_new_seek(
        playbackSpeed, GST_FORMAT_TIME,
        static_cast<GstSeekFlags>(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE),
        GST_SEEK_TYPE_SET, 0, GST_SEEK_TYPE_SET, position_);
  }

  gst_element_send_event(sink_, seek_event);
  rate_ = playbackSpeed;

  SPDLOG_DEBUG("[VideoPlayer] Playback speed: {}", rate_);
}

void VideoPlayer::Play() {
  target_state_ = GST_STATE_PLAYING;
  const GstStateChangeReturn ret =
      gst_element_set_state(playbin_, GST_STATE_PLAYING);
  if (ret == GST_STATE_CHANGE_FAILURE) {
    spdlog::error("[VideoPlayer] Failed to set state GST_STATE_PLAYING.");
  }
}

void VideoPlayer::Pause() {
  GstState state;
  gst_element_get_state(playbin_, &state, nullptr, GST_CLOCK_TIME_NONE);
  if (state != GST_STATE_NULL) {
    target_state_ = GST_STATE_PAUSED;
    const GstStateChangeReturn ret =
        gst_element_set_state(playbin_, GST_STATE_PAUSED);
    if (ret == GST_STATE_CHANGE_FAILURE) {
      event_sink_->Error("[VideoPlayer] Unable to Pause Transport.");
      return;
    }
    SPDLOG_DEBUG("[VideoPlayer] Transport Paused.");
  }
}

int64_t VideoPlayer::GetPosition() {
  if (gst_element_query_position(playbin_, GST_FORMAT_TIME, &position_)) {
    SPDLOG_TRACE("[VideoPlayer] Position: {}", position_);
  }
  return position_ >= 0 ? position_ / AV_TIME_BASE : 0;
}

void VideoPlayer::SendBufferingUpdate() const {
  if (!event_sink_) {
    return;
  }
  auto values = flutter::EncodableList();
  // TODO ranges = player GetBufferedRanges();
  const std::vector<std::tuple<uint64_t, uint64_t>> ranges;
  for (auto& it : ranges) {
    values.emplace_back(flutter::EncodableList(
        {flutter::EncodableValue(static_cast<int64_t>(std::get<0>(it))),
         flutter::EncodableValue(static_cast<int64_t>(std::get<1>(it)))}));
  }

  auto res = flutter::EncodableMap(
      {{flutter::EncodableValue("event"),
        flutter::EncodableValue("bufferingUpdate")},
       {flutter::EncodableValue("values"), flutter::EncodableValue(values)}});
  event_sink_->Success(flutter::EncodableValue(res));
}

void VideoPlayer::SeekTo(const int64_t seek) {
  const gint64 position = seek * AV_TIME_BASE;
  SPDLOG_DEBUG("[VideoPlayer] SeekTo: {} -> {}", seek, position);

  if (!gst_element_seek_simple(
          playbin_, GST_FORMAT_TIME,
          static_cast<GstSeekFlags>(GST_SEEK_FLAG_FLUSH |
                                    GST_SEEK_FLAG_KEY_UNIT),
          position)) {
    SPDLOG_ERROR("[VideoPlayer] Seek Failed");
  }
  gst_element_query_position(playbin_, GST_FORMAT_TIME, &position_);
  SPDLOG_DEBUG("[VideoPlayer] SeekTo: {} -> {}", seek, position_);
}

void VideoPlayer::prepare(VideoPlayer* user_data) {
  SPDLOG_DEBUG("[VideoPlayer] prepare");
  g_object_get(user_data->playbin_, "n-video", &user_data->n_video_, nullptr);
  SPDLOG_DEBUG("[VideoPlayer] {} video streams", user_data->n_video_);
  g_object_get(user_data->playbin_, "current-video", &user_data->current_video_,
               nullptr);
  GstPad* pad = nullptr;
  g_signal_emit_by_name(user_data->playbin_, "get-video-pad",
                        user_data->current_video_, &pad);
  if (!pad) {
    SPDLOG_ERROR(
        "[VideoPlayer] Failed to get video pad, stream number might not exist");
    // TODO g_main_loop_quit(obj->main_loop_);
    return;
  }
  const GstCaps* caps = gst_pad_get_current_caps(pad);
  assert(caps);
  std::lock_guard lock(user_data->gst_mutex_);
  if (!gst_video_info_from_caps(&user_data->info_, caps)) {
    SPDLOG_ERROR("[VideoPlayer] Fail to get video info from the cap");
    // TODO g_main_loop_quit(data->main_loop);
    // return;
  }
  SPDLOG_DEBUG("[VideoPlayer] original video width: {}, height: {}",
               user_data->info_.width, user_data->info_.height);
  // set to the target
  if (!gst_video_info_set_format(&user_data->info_, GST_VIDEO_FORMAT_NV12,
                                 static_cast<guint>(user_data->width_),
                                 static_cast<guint>(user_data->height_))) {
    SPDLOG_ERROR("[VideoPlayer] Failed to set the video info to target NV12");
  }
  user_data->is_initialized_ = true;
}

}  // namespace video_player_linux
