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

#include <encodable_value.h>
#include <event_channel.h>
#include <functional>
#include <mutex>
#include <queue>
#include <unordered_map>
#include <vector>

#include <core/systems/messages/ecs_message.h>
#include <core/systems/messages/ecs_message_types.h>
#include <core/utils/identifiable_type.h>

namespace flutter {
class PluginRegistrar;
class EncodableValue;
}  // namespace flutter

namespace plugin_filament_view {

using ECSMessageHandler = std::function<void(const ECSMessage&)>;

class ECSystem : public IdentifiableType {
 public:
  virtual ~ECSystem() = default;

  // Send a message to the system
  void vSendMessage(const ECSMessage& msg);

  // Register a message handler for a specific message type
  void vRegisterMessageHandler(ECSMessageType type,
                               const ECSMessageHandler& handler);

  // Unregister all handlers for a specific message type
  void vUnregisterMessageHandler(ECSMessageType type);

  // Clear all message handlers
  void vClearMessageHandlers();

  // Process incoming messages
  virtual void vProcessMessages();

  virtual void vInitSystem() = 0;

  virtual void vUpdate(float /*deltaTime*/) = 0;

  virtual void vShutdownSystem() = 0;

  virtual void DebugPrint() = 0;

  void vSetupMessageChannels(flutter::PluginRegistrar* poPluginRegistrar,
                             const std::string& szChannelName);

  void vSendDataToEventChannel(const flutter::EncodableMap& oDataMap) const;

 protected:
  // Handle a specific message type by invoking the registered handlers
  virtual void vHandleMessage(const ECSMessage& msg);

 private:
  std::queue<ECSMessage> messageQueue_;  // Queue of incoming messages
  std::unordered_map<ECSMessageType,
                     std::vector<ECSMessageHandler>,
                     EnumClassHash>
      handlers_;  // Registered handlers

  std::mutex messagesMutex;
  std::mutex handlersMutex;

  std::unique_ptr<flutter::EventChannel<flutter::EncodableValue>>
      event_channel_;

  // The internal Flutter event sink instance, used to send events to the Dart
  // side.
  std::unique_ptr<flutter::EventSink<flutter::EncodableValue>> event_sink_;
};

}  // namespace plugin_filament_view
