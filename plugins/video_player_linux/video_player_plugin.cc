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

#include "video_player_plugin.h"

#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <cstdio> // For printf
#include <inttypes.h> // For PRId64

extern "C" {
//#include <libavutil/avutil.h>
#include <gst/gst.h> // For GStreamer initialization
//#include <libavutil/error.h>   // For av_strerror
}

#include "messages.g.h"
#include "plugins/common/glib/main_loop.h"
#include "video_player.h"

// Conditional SPDLOG support with printf fallback if library not available
#if __has_include("spdlog/spdlog.h")
#include "spdlog/spdlog.h"
#else
// Define dummy SPDLOG macros if spdlog is not available, to avoid compilation errors
#define SPDLOG_DEBUG(...) printf("[DEBUG] " __VA_ARGS__); printf("\n")
namespace spdlog {
    void error(const char* fmt, ...) {
        va_list args;
        va_start(args, fmt);
        printf("[ERROR] ");
        vprintf(fmt, args);
        printf("\n");
        va_end(args);
    }
}
#endif

namespace video_player_linux {

// static
void VideoPlayerPlugin::RegisterWithRegistrar(
    flutter::PluginRegistrarDesktop* registrar) {
  printf("[VideoPlayerPlugin] RegisterWithRegistrar called.\n");
  auto plugin = std::make_unique<VideoPlayerPlugin>(registrar);
  SetUp(registrar->messenger(), plugin.get());
  registrar->AddPlugin(std::move(plugin));
  printf("[VideoPlayerPlugin] Plugin registered.\n");
}

VideoPlayerPlugin::~VideoPlayerPlugin() {
  printf("[VideoPlayerPlugin] Destructor called.\n");
}

VideoPlayerPlugin::VideoPlayerPlugin(flutter::PluginRegistrarDesktop* registrar)
    : registrar_(registrar) {
  printf("[VideoPlayerPlugin] Constructor called.\n");
  // GStreamer lib only needs to be initialized once.  Calling it multiple times
  // is fine.
  gst_init(nullptr, nullptr);
  printf("[VideoPlayerPlugin] GStreamer initialized.\n");

  // start the main loop if not already running
  plugin_common_glib::MainLoop::GetInstance();
  printf("[VideoPlayerPlugin] MainLoop instance obtained/started.\n");

  // suppress libavformat logging
  // av_log_set_callback([](void* /* avcl */, int level,
  //                        const char* fmt, va_list vl) {
  //   // Only log critical errors from libavformat, or adjust level as needed
  //   if (level < AV_LOG_ERROR) { // AV_LOG_ERROR, AV_LOG_FATAL, AV_LOG_PANIC
  //       // vprintf("[libav] ", vl); // This would print all suppressed logs
  //   }
  // });
  // printf("[VideoPlayerPlugin] libavformat logging suppressed.\n");
}

std::optional<FlutterError> VideoPlayerPlugin::Initialize() {
  printf("[VideoPlayerPlugin] Initialize called.\n");
  for (auto& [fst, snd] : videoPlayers) {
    printf("[VideoPlayerPlugin] Disposing existing player with texture ID: %" PRId64 ".\n", fst);
    snd->Dispose();
  }
  videoPlayers.clear();
  printf("[VideoPlayerPlugin] All video players cleared.\n");
  return std::nullopt;
}

ErrorOr<int64_t> VideoPlayerPlugin::Create(
    const std::string* asset,
    const std::string* uri,
    const flutter::EncodableMap& http_headers) {
  printf("[VideoPlayerPlugin] Create called.\n");
  std::string asset_to_load;
  std::map<std::string, std::string> http_headers_;
  std::unique_ptr<VideoPlayer> player;

  // Determine asset or URI path to load
  if (asset && !asset->empty()) {
    asset_to_load = "file://";
    std::filesystem::path path;
    if (asset->c_str()[0] == '/') {
      path /= asset->c_str();
    } else {
      path = registrar_->flutter_asset_folder();
      path /= asset->c_str();
    }
    if (!exists(path)) {
      spdlog::error("[VideoPlayer] Asset path does not exist. {}", path.c_str());
      return FlutterError("asset_load_failed", "Asset path does not exist.");
    }
    asset_to_load += path.c_str();
  } else if (uri && !uri->empty()) {
    asset_to_load = *uri;
    for (const auto& [key, value] : http_headers) {
      if (std::holds_alternative<std::string>(key) &&
          std::holds_alternative<std::string>(value)) {
        http_headers_[std::get<std::string>(key)] =
            std::get<std::string>(value);
      }
    }
  } else {
    return FlutterError("not_implemented", "Set either an asset or a uri");
  }

  SPDLOG_DEBUG("[VideoPlayer] Asset to load: {}", asset_to_load);

  // CHANGE: Use ffprobe-based function instead of direct FFmpeg library calls
  // This avoids library conflicts and provides more stable metadata extraction
  int width = 0, height = 0;
  gint64 duration_ns = 0;
  std::string codec_name;

  printf("[VideoPlayerPlugin] Video info will be extracted using ffprobe...\n");
  if (!get_video_info_ffprobe_no_json(asset_to_load.c_str(), width, height, duration_ns, codec_name)) {
      spdlog::error("[VideoPlayerPlugin] ERROR: Could not extract video info with ffprobe: {}", asset_to_load);
      return FlutterError("video_info_failed_ffprobe", "Could not extract video info from source using ffprobe.");
  }
   printf("[VideoPlayerPlugin] Info extracted from ffprobe: Width=%d, Height=%d, Duration (ns)=%" PRId64 ", Codec=%s\n", 
          width, height, duration_ns, codec_name.c_str());

  // CHANGE: Use automatic decoder 'decodebin' instead of codec-specific decoders
  // This provides universal codec support and eliminates the need for codec mapping
  printf("[VideoPlayerPlugin] Using 'decodebin' decoder since ffprobe is used.\n");
  GstElementFactory* decoder_factory = gst_element_factory_find("decodebin");
  if (!decoder_factory) {
    return FlutterError("decoder_not_found", "'decodebin' GStreamer element not found. Check GStreamer installation.");
  }
  
  // Create VideoPlayer instance with dynamically extracted info from ffprobe
  try {
    printf("[VideoPlayerPlugin] Creating VideoPlayer instance...\n");
    player = std::make_unique<VideoPlayer>(registrar_, asset_to_load.c_str(),
                                         std::move(http_headers_), width,
                                         height, duration_ns, decoder_factory);
    
    printf("[VideoPlayerPlugin] Calling VideoPlayer Init...\n");
    player->Init(registrar_->messenger());
    printf("[VideoPlayerPlugin] VideoPlayer successfully initialized.\n");

  } catch (const std::exception& e) {
    printf("[VideoPlayerPlugin] ERROR: Exception during VideoPlayer creation/initialization: %s\n", e.what());
    return FlutterError("player_creation_failed", e.what());
  } catch (...) {
    printf("[VideoPlayerPlugin] ERROR: Unknown exception during VideoPlayer creation/initialization.\n");
    return FlutterError("player_creation_failed", "Unknown exception");
  }

  auto texture_id = player->GetTextureId();
  videoPlayers.insert(std::make_pair(texture_id, std::move(player)));

  printf("[VideoPlayerPlugin] Create method completed successfully, returning texture_id: %" PRId64 "\n", texture_id);
  return texture_id;
}

std::optional<FlutterError> VideoPlayerPlugin::Dispose(
    const int64_t texture_id) {
  printf("[VideoPlayerPlugin] Dispose called for texture ID: %" PRId64 "\n", texture_id);
  const auto searchPlayer = videoPlayers.find(texture_id);
  if (searchPlayer == videoPlayers.end()) {
    printf("[VideoPlayerPlugin] ERROR: Player with texture ID %" PRId64 " not found for dispose.\n", texture_id);
    return FlutterError("player_not_found", "This player ID was not found");
  }
  if (searchPlayer->second->IsValid()) {
    printf("[VideoPlayerPlugin] Player with texture ID %" PRId64 " is valid. Disposing...\n", texture_id);
    searchPlayer->second->Dispose();
    videoPlayers.erase(texture_id);
    printf("[VideoPlayerPlugin] Player with texture ID %" PRId64 " disposed and removed from map.\n", texture_id);
  } else {
    printf("[VideoPlayerPlugin] Player with texture ID %" PRId64 " is not valid. Skipping dispose.\n", texture_id);
  }

  return {};
}

std::optional<FlutterError> VideoPlayerPlugin::SetLooping(
    const int64_t texture_id,
    const bool is_looping) {
  printf("[VideoPlayerPlugin] SetLooping called for texture ID: %" PRId64 ", looping: %s\n", texture_id, is_looping ? "true" : "false");
  const auto searchPlayer = videoPlayers.find(texture_id);
  if (searchPlayer == videoPlayers.end()) {
    printf("[VideoPlayerPlugin] ERROR: Player with texture ID %" PRId64 " not found for SetLooping.\n", texture_id);
    return FlutterError("player_not_found", "This player ID was not found");
  }
  if (searchPlayer->second->IsValid()) {
    printf("[VideoPlayerPlugin] Player with texture ID %" PRId64 " is valid. Setting looping.\n", texture_id);
    searchPlayer->second->SetLooping(is_looping);
  } else {
    printf("[VideoPlayerPlugin] Player with texture ID %" PRId64 " is not valid. Skipping SetLooping.\n", texture_id);
  }

  return {};
}

std::optional<FlutterError> VideoPlayerPlugin::SetVolume(
    const int64_t texture_id,
    const double volume) {
  printf("[VideoPlayerPlugin] SetVolume called for texture ID: %" PRId64 ", volume: %f\n", texture_id, volume);
  const auto searchPlayer = videoPlayers.find(texture_id);
  if (searchPlayer == videoPlayers.end()) {
    printf("[VideoPlayerPlugin] ERROR: Player with texture ID %" PRId64 " not found for SetVolume.\n", texture_id);
    return FlutterError("player_not_found", "This player ID was not found");
  }
  if (searchPlayer->second->IsValid()) {
    printf("[VideoPlayerPlugin] Player with texture ID %" PRId64 " is valid. Setting volume.\n", texture_id);
    searchPlayer->second->SetVolume(volume);
  } else {
    printf("[VideoPlayerPlugin] Player with texture ID %" PRId64 " is not valid. Skipping SetVolume.\n", texture_id);
  }

  return {};
}

std::optional<FlutterError> VideoPlayerPlugin::SetPlaybackSpeed(
    const int64_t texture_id,
    const double speed) {
  printf("[VideoPlayerPlugin] SetPlaybackSpeed called for texture ID: %" PRId64 ", speed: %f\n", texture_id, speed);
  const auto searchPlayer = videoPlayers.find(texture_id);
  if (searchPlayer == videoPlayers.end()) {
    printf("[VideoPlayerPlugin] ERROR: Player with texture ID %" PRId64 " not found for SetPlaybackSpeed.\n", texture_id);
    return FlutterError("player_not_found", "This player ID was not found");
  }
  if (searchPlayer->second->IsValid()) {
    printf("[VideoPlayerPlugin] Player with texture ID %" PRId64 " is valid. Setting playback speed.\n", texture_id);
    searchPlayer->second->SetPlaybackSpeed(speed);
  } else {
    printf("[VideoPlayerPlugin] Player with texture ID %" PRId64 " is not valid. Skipping SetPlaybackSpeed.\n", texture_id);
  }

  return {};
}

std::optional<FlutterError> VideoPlayerPlugin::Play(const int64_t texture_id) {
  printf("[VideoPlayerPlugin] Play called for texture ID: %" PRId64 "\n", texture_id);
  const auto searchPlayer = videoPlayers.find(texture_id);
  if (searchPlayer == videoPlayers.end()) {
    printf("[VideoPlayerPlugin] ERROR: Player with texture ID %" PRId64 " not found for Play.\n", texture_id);
    return FlutterError("player_not_found", "This player ID was not found");
  }
  if (searchPlayer->second->IsValid()) {
    printf("[VideoPlayerPlugin] Player with texture ID %" PRId64 " is valid. Calling Play().\n", texture_id);
    searchPlayer->second->Play();
  } else {
    printf("[VideoPlayerPlugin] Player with texture ID %" PRId64 " is not valid. Skipping Play().\n", texture_id);
  }

  return {};
}

ErrorOr<int64_t> VideoPlayerPlugin::GetPosition(const int64_t texture_id) {
  printf("[VideoPlayerPlugin] GetPosition called for texture ID: %" PRId64 "\n", texture_id);
  const auto searchPlayer = videoPlayers.find(texture_id);
  int64_t position = 0;
  if (searchPlayer != videoPlayers.end()) {
    if (const std::unique_ptr<VideoPlayer>& player = searchPlayer->second;
        player->IsValid()) {
      position = player->GetPosition();
      printf("[VideoPlayerPlugin] Player with texture ID %" PRId64 " is valid. Current position: %" PRId64 "\n", texture_id, position);
      //      player->SendBufferingUpdate(); // Commented out in original
    } else {
      printf("[VideoPlayerPlugin] Player with texture ID %" PRId64 " is not valid. Returning position 0.\n", texture_id);
    }
  } else {
    printf("[VideoPlayerPlugin] ERROR: Player with texture ID %" PRId64 " not found for GetPosition. Returning position 0.\n", texture_id);
  }
  return position;
}

std::optional<FlutterError> VideoPlayerPlugin::SeekTo(const int64_t texture_id,
                                                      const int64_t position) {
  printf("[VideoPlayerPlugin] SeekTo called for texture ID: %" PRId64 ", position: %" PRId64 "\n", texture_id, position);
  const auto searchPlayer = videoPlayers.find(texture_id);
  if (searchPlayer == videoPlayers.end()) {
    printf("[VideoPlayerPlugin] ERROR: Player with texture ID %" PRId64 " not found for SeekTo.\n", texture_id);
    return FlutterError("player_not_found", "This player ID was not found");
  }
  if (searchPlayer->second->IsValid()) {
    printf("[VideoPlayerPlugin] Player with texture ID %" PRId64 " is valid. Seeking to position.\n", texture_id);
    searchPlayer->second->SeekTo(position);
  } else {
    printf("[VideoPlayerPlugin] Player with texture ID %" PRId64 " is not valid. Skipping SeekTo.\n", texture_id);
  }

  return std::nullopt;
}

std::optional<FlutterError> VideoPlayerPlugin::Pause(const int64_t texture_id) {
  printf("[VideoPlayerPlugin] Pause called for texture ID: %" PRId64 "\n", texture_id);
  const auto searchPlayer = videoPlayers.find(texture_id);
  if (searchPlayer == videoPlayers.end()) {
    printf("[VideoPlayerPlugin] ERROR: Player with texture ID %" PRId64 " not found for Pause.\n", texture_id);
    return FlutterError("player_not_found", "This player ID was not found");
  }
  if (searchPlayer->second->IsValid()) {
    printf("[VideoPlayerPlugin] Player with texture ID %" PRId64 " is valid. Calling Pause().\n", texture_id);
    searchPlayer->second->Pause();
  } else {
    printf("[VideoPlayerPlugin] Player with texture ID %" PRId64 " is not valid. Skipping Pause().\n", texture_id);
  }

  return std::nullopt;
}

// LEGACY FUNCTION: Maintained for API compatibility but now uses FFprobe internally
// This function was originally used for direct FFmpeg library calls but has been
// replaced with subprocess-based FFprobe extraction for stability
// bool VideoPlayerPlugin::get_video_info(const char* url,
//                                        int& width,
//                                        int& height,
//                                        gint64& duration,
//                                        AVCodecID& codec_id) {
//     printf("\n--- [DEBUG] get_video_info STARTED ---\n");
//     printf("[DEBUG] STEP 0: URL received: %s\n", url);
//     fflush(stdout);

//     AVFormatContext* fmt_ctx = nullptr;

//     printf("[DEBUG] STEP 1: Calling avformat_open_input...\n");
//     fflush(stdout);

//     int ret = avformat_open_input(&fmt_ctx, url, nullptr, nullptr);

//     printf("[DEBUG] STEP 2: avformat_open_input called. Return code (ret): %d\n", ret);
//     fflush(stdout);

//     if (ret < 0) {
//         char errbuf[AV_ERROR_MAX_STRING_SIZE];
//         av_strerror(ret, errbuf, sizeof(errbuf));
//         fprintf(stderr, "[ERROR] avformat_open_input failed: %s\n", errbuf);
//         fflush(stderr);
//         // fmt_ctx should be null or invalid here, close_input handles this safely
//         avformat_close_input(&fmt_ctx);
//         printf("--- [DEBUG] get_video_info FAILED (open_input) ---\n\n");
//         fflush(stdout);
//         return false;
//     }

//     printf("[DEBUG] STEP 3: avformat_open_input successful. AVFormatContext address: %p\n", (void*)fmt_ctx);
//     fflush(stdout);

//     printf("[DEBUG] STEP 4: Calling avformat_find_stream_info...\n");
//     fflush(stdout);
    
//     ret = avformat_find_stream_info(fmt_ctx, nullptr);
    
//     printf("[DEBUG] STEP 5: avformat_find_stream_info called. Return code (ret): %d\n", ret);
//     fflush(stdout);

//     if (ret < 0) {
//         char errbuf[AV_ERROR_MAX_STRING_SIZE];
//         av_strerror(ret, errbuf, sizeof(errbuf));
//         fprintf(stderr, "[ERROR] avformat_find_stream_info failed: %s\n", errbuf);
//         fflush(stderr);
//         avformat_close_input(&fmt_ctx);
//         printf("--- [DEBUG] get_video_info FAILED (find_stream_info) ---\n\n");
//         fflush(stdout);
//         return false;
//     }

//     printf("[DEBUG] STEP 6: avformat_find_stream_info successful. Stream count: %u\n", fmt_ctx->nb_streams);
//     fflush(stdout);

//     printf("[DEBUG] STEP 7: Calling av_find_best_stream (video)...\n");
//     fflush(stdout);

//     const int video_stream_index = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);

//     printf("[DEBUG] STEP 8: av_find_best_stream called. Found index: %d\n", video_stream_index);
//     fflush(stdout);

//     if (video_stream_index < 0) {
//         fprintf(stderr, "[ERROR] No video stream found.\n");
//         fflush(stderr);
//         avformat_close_input(&fmt_ctx);
//         printf("--- [DEBUG] get_video_info FAILED (best_stream) ---\n\n");
//         fflush(stdout);
//         return false;
//     }
    
//     printf("[DEBUG] STEP 9: Accessing fmt_ctx->streams[%d]...\n", video_stream_index);
//     fflush(stdout);

//     const AVStream* stream = fmt_ctx->streams[video_stream_index];
//     printf("[DEBUG] STEP 10: AVStream address obtained: %p\n", (void*)stream);
//     fflush(stdout);

//     if (!stream) {
//         fprintf(stderr, "[ERROR] Stream pointer is NULL!\n");
//         fflush(stderr);
//         avformat_close_input(&fmt_ctx);
//         return false;
//     }

//     printf("[DEBUG] STEP 11: Accessing stream->codecpar...\n");
//     fflush(stdout);
    
//     const AVCodecParameters* par = stream->codecpar;
//     printf("[DEBUG] STEP 12: AVCodecParameters address obtained: %p\n", (void*)par);
//     fflush(stdout);

//     if (!par) {
//         fprintf(stderr, "[ERROR] Codec parameters pointer is NULL!\n");
//         fflush(stderr);
//         avformat_close_input(&fmt_ctx);
//         return false;
//     }

//     printf("[DEBUG] STEP 13: Reading codec parameters (width, height, codec_id)...\n");
//     fflush(stdout);

//     width = par->width;
//     height = par->height;
//     codec_id = par->codec_id;
//     duration = fmt_ctx->duration;

//     printf("[DEBUG] STEP 14: Values assigned: width=%d, height=%d, codec_id=%d, duration=%" PRId64 "\n", width, height, static_cast<int>(codec_id), duration);
//     fflush(stdout);

//     av_dump_format(fmt_ctx, 0, url, 0);
    
//     printf("[DEBUG] STEP 15: Calling avformat_close_input...\n");
//     fflush(stdout);

//     avformat_close_input(&fmt_ctx);

//     printf("[DEBUG] STEP 16: Function completed successfully.\n");
//     printf("--- [DEBUG] get_video_info COMPLETED SUCCESSFULLY ---\n\n");
//     fflush(stdout);

//     return true;
// }

// IMPROVED FUNCTION: FFprobe-based video info extraction
// This function replaces direct FFmpeg library calls with ffprobe subprocess
// execution to avoid library conflicts and provide more stable metadata extraction
bool VideoPlayerPlugin::get_video_info_ffprobe_no_json(const char* url,
                                    int& width,
                                    int& height,
                                    gint64& duration_ns,
                                    std::string& codec_name) {
    
    std::string url_str = url;
    std::string scheme = "file://";
    if (url_str.rfind(scheme, 0) == 0) {
        url_str.erase(0, scheme.length());
    }

    // IMPORTANT: The field order here (codec_name,width,height,duration)
    // must exactly match the parsing order below
    char command[2048];
    snprintf(command, sizeof(command),
             "ffprobe -v error -select_streams v:0 -show_entries stream=codec_name,width,height,duration -of default=noprint_wrappers=1:nokey=1 \"%s\"",
             url_str.c_str());

    std::vector<std::string> lines;
    std::array<char, 256> buffer;
    
    printf("[DEBUG] FFprobe command to execute: %s\n", command);

    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(command, "r"), pclose);
    if (!pipe) {
        fprintf(stderr, "[ERROR] popen() failed!\n");
        return false;
    }

    // Read each line of output into a vector
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        std::string line = buffer.data();
        // fgets also gets the newline character, let's clean it
        line.erase(line.find_last_not_of("\n\r") + 1);
        if (!line.empty()) {
            lines.push_back(line);
        }
    }

    // Check if we have the expected number of lines
    if (lines.size() < 4) {
        fprintf(stderr, "[ERROR] Expected number of lines not received from ffprobe. Lines received: %zu\n", lines.size());
        return false;
    }

    printf("[DEBUG] Lines received from ffprobe:\n1: %s\n2: %s\n3: %s\n4: %s\n",
           lines[0].c_str(), lines[1].c_str(), lines[2].c_str(), lines[3].c_str());

    try {
        // Assign lines to variables in order
        // This order must match the command order above!
        codec_name = lines[0];
        width = std::stoi(lines[1]);
        height = std::stoi(lines[2]);
        
        // Convert duration from seconds to nanoseconds
        double duration_sec = std::stod(lines[3]);
        duration_ns = static_cast<gint64>(duration_sec * 1e9);

        printf("[DEBUG] Parsed values: width=%d, height=%d, duration_ns=%ld, codec_name=%s\n",
               width, height, duration_ns, codec_name.c_str());

        return true;
    } catch (const std::invalid_argument& ia) {
        fprintf(stderr, "[ERROR] Value conversion error (invalid_argument): %s\n", ia.what());
        return false;
    } catch (const std::out_of_range& oor) {
        fprintf(stderr, "[ERROR] Value conversion error (out_of_range): %s\n", oor.what());
        return false;
    }

    return false;
}

// LEGACY FUNCTION: This large switch statement was used by the original `get_video_info` 
// to map an FFmpeg AVCodecID to a GStreamer decoder element name. 
// It is now "dead code" because the new implementation uses `decodebin`, 
// which handles codec detection automatically. Kept for reference/compatibility.
// const char* VideoPlayerPlugin::map_ffmpeg_plugin(const AVCodecID codec_id) {
//   switch (codec_id) {
//     // ... (cases for all codecs - keeping original implementation)
//     default:
//       SPDLOG_DEBUG("Codec not supported - using decodebin for automatic detection");
//       return "";
//   }
// }

}  // namespace video_player_linux