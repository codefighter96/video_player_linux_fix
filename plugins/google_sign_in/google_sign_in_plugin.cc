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

#include "google_sign_in_plugin.h"

#include <filesystem>

#include "messages.g.h"

#include "config/plugins.h"

namespace google_sign_in_plugin {

// static
void GoogleSignInPlugin::RegisterWithRegistrar(
    flutter::PluginRegistrar* registrar) {
  auto plugin = std::make_unique<GoogleSignInPlugin>();

  SetUp(registrar->messenger(), plugin.get());

  registrar->AddPlugin(std::move(plugin));
}

std::optional<FlutterError> GoogleSignInPlugin::Init(const InitParams& params) {
  spdlog::info("[GoogleSignInPlugin] Init");
  const auto& scopes = params.scopes();
  for (const auto& scope : scopes) {
    plugin_common::Encodable::PrintFlutterEncodableValue("scope", scope);
  }

  switch (params.sign_in_type()) {
    case SignInType::kStandard:
      spdlog::info("\tSignInType::kStandard");
      break;
    case SignInType::kGames:
      spdlog::info("\tSignInType::kGames");
      break;
  }

  if (const auto hosted_domain = params.hosted_domain();
      !hosted_domain->empty()) {
    spdlog::info("\thosted_domain: {}", hosted_domain->c_str());
  }
  if (const auto client_id = params.client_id(); !client_id->empty()) {
    spdlog::info("\tclient_id: {}", client_id->c_str());
  }
  if (const auto server_client_id = params.server_client_id();
      !server_client_id->empty()) {
    spdlog::info("\tserver_client_id: {}", server_client_id->c_str());
  }
  auto force_code_for_refresh_token = params.force_code_for_refresh_token();
  spdlog::info("\tforce_code_for_refresh_token: {}",
               force_code_for_refresh_token);

  return {};
}

void GoogleSignInPlugin::SignInSilently(
    std::function<void(ErrorOr<UserData> reply)> result) {
  (void)result;
  spdlog::info("[GoogleSignInPlugin] SignInSilently");
}

void GoogleSignInPlugin::SignIn(
    std::function<void(ErrorOr<UserData> reply)> result) {
  (void)result;
  spdlog::info("[GoogleSignInPlugin] SignIn");
}

void GoogleSignInPlugin::GetAccessToken(
    const std::string& email,
    bool should_recover_auth,
    std::function<void(ErrorOr<std::string> reply)> result) {
  (void)result;
  spdlog::info(
      "[GoogleSignInPlugin] GetAccessToken: email={}, should_recover_auth={}",
      email, should_recover_auth);
}

void GoogleSignInPlugin::SignOut(
    std::function<void(std::optional<FlutterError> reply)> result) {
  (void)result;
  spdlog::info("[GoogleSignInPlugin] SignOut");
}

void GoogleSignInPlugin::Disconnect(
    std::function<void(std::optional<FlutterError> reply)> result) {
  (void)result;
  spdlog::info("[GoogleSignInPlugin] Disconnect");
}

ErrorOr<bool> GoogleSignInPlugin::IsSignedIn() {
  spdlog::info("[GoogleSignInPlugin] IsSignedIn");
  return false;
}

void GoogleSignInPlugin::ClearAuthCache(
    const std::string& token,
    std::function<void(std::optional<FlutterError> reply)> result) {
  (void)result;
  spdlog::info("[GoogleSignInPlugin] ClearAuthCache: token={}", token);
}

void GoogleSignInPlugin::RequestScopes(
    const flutter::EncodableList& scopes,
    std::function<void(ErrorOr<bool> reply)> result) {
  (void)scopes;
  (void)result;
}

#if 0  // TODO
rapidjson::Document GoogleSignInPlugin::GetClientSecret() {
  std::string path;
  if (const auto env_var = getenv(kClientSecretPathEnvironmentVariable)) {
    path.assign(env_var);
  }
  return plugin_common::JsonUtils::GetJsonDocumentFromFile(path);
}

rapidjson::Document GoogleSignInPlugin::GetClientCredentials() {
  std::string path;
  if (const auto env_var = getenv(kClientCredentialsPathEnvironmentVariable)) {
    path.assign(env_var);
  }
  return plugin_common::JsonUtils::GetJsonDocumentFromFile(path);
}

bool GoogleSignInPlugin::UpdateClientCredentialFile(
    const rapidjson::Document& client_credential_doc) {
  const auto env_var = getenv(kClientCredentialsPathEnvironmentVariable);
  std::string path;
  if (env_var) {
    path.assign(env_var);
    if (path.empty()) {
      spdlog::error("Missing File Path: {}", path);
      return false;
    }

    const std::filesystem::path p(path);
    if (!std::filesystem::exists(path)) {
      create_directories(p.parent_path());
    } else {
      std::filesystem::remove(path);
    }
  }

  return plugin_common::JsonUtils::WriteJsonDocumentToFile(
      path, client_credential_doc);
}

rapidjson::Document GoogleSignInPlugin::SwapAuthCodeForToken(
    rapidjson::Document& client_secret_doc,
    rapidjson::Document& client_credential_doc) {
  rapidjson::Document doc;
  auto secret_obj = client_secret_doc.GetObject();
  auto installed_secret_obj = secret_obj[kKeyInstalled].GetObject();
  auto creds_obj = client_credential_doc.GetObject();

  std::string auth_code = creds_obj[kKeyAuthCode].GetString();
  std::string token_uri = installed_secret_obj[kKeyTokenUri].GetString();
  std::string client_id = installed_secret_obj[kKeyClientId].GetString();
  std::string client_secret =
      installed_secret_obj[kKeyClientSecret].GetString();

  if (auth_code.empty() || token_uri.empty() || client_id.empty() ||
      client_secret.empty()) {
    doc.Parse("{}");
    return std::move(doc);
  }

  plugin_common_curl::CurlClient client;
  std::vector<std::string> headers{};
  std::vector<std::pair<std::string, std::string>> url_form;
  url_form.emplace_back(std::move(std::make_pair(kKeyCode, auth_code)));
  url_form.emplace_back(
      std::move(std::make_pair(kKeyClientId, std::move(client_id))));
  url_form.emplace_back(
      std::move(std::make_pair(kKeyClientSecret, std::move(client_secret))));
  url_form.emplace_back(
      std::move(std::make_pair(kKeyRedirectUri, kValueRedirectUri)));
  url_form.emplace_back(
      std::move(std::make_pair(kKeyGrantType, kValueAuthorizationCode)));

  client.Init(token_uri, headers, url_form);
  auto response = client.RetrieveContentAsString();

  if (client.GetCode() != CURLE_OK) {
    doc.Parse("{}");
    return std::move(doc);
  }

  doc.Parse(response.c_str());
  if (doc.GetParseError() != rapidjson::kParseErrorNone) {
    doc.Parse("{}");
    return std::move(doc);
  }

  auto resp = doc.GetObject();
  auto& allocator = doc.GetAllocator();

  // change expires_in to expires_at
  auto expires_in = resp["expires_in"].GetInt64();
  if (!resp.RemoveMember("expires_in")) {
    spdlog::error("Failed to remove token: expires_in");
  }
  int64_t expires_at =
      plugin_common::TimeTools::GetEpochTimeInSeconds() + expires_in;
  resp.AddMember("expires_at", expires_at, allocator);

  // preserve auth_code
  resp.AddMember(rapidjson::Value(kKeyAuthCode, allocator).Move(),
                 rapidjson::Value(auth_code.c_str(), allocator).Move(),
                 allocator);

  if (!UpdateClientCredentialFile(doc)) {
    doc.Parse("{}");
    return std::move(doc);
  }

  return std::move(doc);
}

rapidjson::Document GoogleSignInPlugin::RefreshToken(
    rapidjson::Document& client_secret_doc,
    rapidjson::Document& client_credential_doc) {
  rapidjson::Document doc;
  auto secret_obj = client_secret_doc.GetObject();
  auto installed_secret_obj = secret_obj[kKeyInstalled].GetObject();
  auto creds_obj = client_credential_doc.GetObject();

  std::string refresh_token = creds_obj[kKeyRefreshToken].GetString();
  std::string auth_code = creds_obj[kKeyAuthCode].GetString();
  int64_t expires_at = creds_obj[kKeyExpiresAt].GetInt64();

  // Has token expired?
  auto now = plugin_common::TimeTools::GetEpochTimeInSeconds();
  spdlog::trace("[google_sign_in] Now: {}", now);
  spdlog::trace("[google_sign_in] Token Expires At: {}", expires_at);
  if (expires_at > now) {
    spdlog::debug("[Google Sign In] Token Expires In {} Seconds",
                  now - expires_at);
    return std::move(client_credential_doc);
  }
  SPDLOG_DEBUG("[Google Sign In] Refreshing Token");

  std::string token_uri = installed_secret_obj[kKeyTokenUri].GetString();
  std::string client_id = installed_secret_obj[kKeyClientId].GetString();
  std::string client_secret =
      installed_secret_obj[kKeyClientSecret].GetString();

  if (token_uri.empty() || client_id.empty() || client_secret.empty() ||
      refresh_token.empty() || auth_code.empty()) {
    doc.Parse("{}");
    return std::move(doc);
  }

  plugin_common_curl::CurlClient client;
  std::vector<std::string> headers{};
  std::vector<std::pair<std::string, std::string>> url_form;
  url_form.emplace_back(
      std::move(std::make_pair(kKeyRefreshToken, refresh_token)));
  url_form.emplace_back(
      std::move(std::make_pair(kKeyClientId, std::move(client_id))));
  url_form.emplace_back(
      std::move(std::make_pair(kKeyClientSecret, std::move(client_secret))));
  url_form.emplace_back(
      std::move(std::make_pair(kKeyGrantType, kValueRefreshToken)));

  client.Init(token_uri, headers, url_form);
  auto response = client.RetrieveContentAsString();

  if (client.GetCode() != CURLE_OK) {
    doc.Parse("{}");
    return std::move(doc);
  }

  doc.Parse(response.c_str());
  if (doc.GetParseError() != rapidjson::kParseErrorNone) {
    spdlog::error("[google_sign_in] Failure Parsing Refresh Token Response: {}",
                  static_cast<int>(doc.GetParseError()));
    doc.Parse("{}");
    return std::move(doc);
  }

  auto resp = doc.GetObject();
  auto& allocator = doc.GetAllocator();

  if (resp.HasMember("error") && resp["error"].IsString() &&
      resp.HasMember("error_description") &&
      resp["error_description"].IsString()) {
    spdlog::error("[google_sign_in] Refresh Token Error: {} - {}",
                  resp["error"].GetString(),
                  resp["error_description"].GetString());
    return std::move(doc);
  }

  // change expires_in to expires_at
  auto expires_in = resp[kKeyExpiresIn].GetInt64();
  if (!resp.RemoveMember(kKeyExpiresIn)) {
    spdlog::error("Failed to remove token: {}", kKeyExpiresIn);
  }
  expires_at = plugin_common::TimeTools::GetEpochTimeInSeconds() + expires_in;
  resp.AddMember("expires_at", expires_at, allocator);

  // preserve refresh_token
  resp.AddMember(rapidjson::Value(kKeyRefreshToken, allocator).Move(),
                 rapidjson::Value(refresh_token.c_str(), allocator).Move(),
                 allocator);

  // preserve auth_code
  resp.AddMember(rapidjson::Value(kKeyAuthCode, allocator).Move(),
                 rapidjson::Value(auth_code.c_str(), allocator).Move(),
                 allocator);

  if (!UpdateClientCredentialFile(doc)) {
    doc.Parse("{}");
    return std::move(doc);
  }

  return std::move(doc);
}

bool GoogleSignInPlugin::CreateDefaultClientCredentialFile() {
  const auto env_var = getenv(kClientCredentialsPathEnvironmentVariable);
  std::string path;
  if (env_var) {
    path.assign(env_var);
    if (path.empty()) {
      spdlog::error("Missing File Path: {}", path);
      return false;
    }

    const std::filesystem::path p(path);
    if (!std::filesystem::exists(path)) {
      create_directories(p.parent_path());
    } else {
      std::filesystem::remove(path);
    }
  }

  return plugin_common::JsonUtils::AddEmptyKeyToFile(path, kKeyAuthCode);
}

std::string GoogleSignInPlugin::GetAuthUrl(
    rapidjson::Document& secret_doc,
    const std::vector<std::string>& scopes) {
  std::string res;
  if (const auto secret_obj = secret_doc.GetObject();
      secret_obj.HasMember(kKeyInstalled)) {
    const auto obj = secret_obj[kKeyInstalled].GetObject();
    if (!obj.HasMember(kKeyClientId) || !obj.HasMember(kKeyAuthUri) ||
        !obj[kKeyClientId].IsString() || !obj[kKeyAuthUri].IsString()) {
      spdlog::error("Invalid client_secret object");
      return res;
    }
    const std::string client_id_ = obj[kKeyClientId].GetString();
    const std::string auth_uri_ = obj[kKeyAuthUri].GetString();
    std::stringstream ss;
    ss << auth_uri_ << "?client_id=" << client_id_;
    ss << "&redirect_uri=urn:ietf:wg:oauth:2.0:oob";
    ss << "&scope=";
    for (auto& scope : scopes) {
      ss << scope << "%20";
    }
    ss << "https://www.googleapis.com/auth/userinfo.profile";
    ss << "&response_type=code";
    res.assign(ss.str());
  }
  return res;
}

bool GoogleSignInPlugin::AuthCodeValuePresent(
    rapidjson::Document& credentials_doc) {
  if (const auto obj = credentials_doc.GetObject();
      obj.HasMember(kKeyAuthCode) && obj[kKeyAuthCode].IsString()) {
    if (const std::string auth_code = obj[kKeyAuthCode].GetString();
        !auth_code.empty()) {
      return true;
    }
  }
  return false;
}

bool GoogleSignInPlugin::SecretJsonPopulated(rapidjson::Document& secret_doc) {
  if (const auto obj = secret_doc.GetObject(); obj.HasMember(kKeyInstalled)) {
    const auto installed_obj = obj[kKeyInstalled].GetObject();
    const bool client_id = installed_obj.HasMember(kKeyClientId) &&
                           installed_obj[kKeyClientId].IsString();
    const bool project_id = installed_obj.HasMember(kKeyProjectId) &&
                            installed_obj[kKeyProjectId].IsString();
    const bool auth_uri = installed_obj.HasMember(kKeyAuthUri) &&
                          installed_obj[kKeyAuthUri].IsString();
    const bool token_uri = installed_obj.HasMember(kKeyTokenUri) &&
                           installed_obj[kKeyTokenUri].IsString();
    const bool auth_provider_x509_cert_url =
        installed_obj.HasMember(kKeyAuthProviderX509CertUrl) &&
        installed_obj[kKeyAuthProviderX509CertUrl].IsString();
    const bool client_secret = installed_obj.HasMember(kKeyClientSecret) &&
                               installed_obj[kKeyClientSecret].IsString();
    const bool redirect_uris = installed_obj.HasMember(kKeyRedirectUris) &&
                               installed_obj[kKeyRedirectUris].IsArray();

    if (client_id && project_id && auth_uri && token_uri &&
        auth_provider_x509_cert_url && client_secret && redirect_uris) {
      return true;
    }
  }
  return false;
}

bool GoogleSignInPlugin::CredentialsJsonPopulated(
    rapidjson::Document& credentials_doc) {
  if (const auto obj = credentials_doc.GetObject();
      obj.HasMember(kKeyAccessToken) && obj[kKeyAccessToken].IsString() &&
      obj.HasMember(kKeyIdToken) && obj[kKeyIdToken].IsString() &&
      obj.HasMember(kKeyScope) && obj[kKeyScope].IsString() &&
      obj.HasMember(kKeyTokenType) && obj[kKeyTokenType].IsString() &&
      obj.HasMember(kKeyExpiresAt) && obj[kKeyExpiresAt].IsNumber() &&
      obj.HasMember(kKeyRefreshToken) && obj[kKeyRefreshToken].IsString() &&
      obj.HasMember(kKeyAuthCode) && obj[kKeyAuthCode].IsString()) {
    return true;
  }
  return false;
}

void GoogleSignInPlugin::Init(const std::vector<std::string>& requestedScopes,
                              std::string hostedDomain,
                              std::string signInOption,
                              std::string clientId,
                              std::string serverClientId,
                              bool forceCodeForRefreshToken) {
#if !defined(NDEBUG)
  std::stringstream ss;
  for (auto& scope : requestedScopes) {
    ss << "\n\t" << scope;
  }
  SPDLOG_DEBUG("\trequestedScopes: {}", ss.str().c_str());
  SPDLOG_DEBUG("\thostedDomain: [{}]", hostedDomain);
  SPDLOG_DEBUG("\tsignInOption: [{}]", signInOption);
  SPDLOG_DEBUG("\tclientId: [{}]", clientId);
  SPDLOG_DEBUG("\tserverClientId: [{}]", serverClientId);
  SPDLOG_DEBUG("\tforceCodeForRefreshToken: {}", forceCodeForRefreshToken);
#else
  (void)requestedScopes;
  (void)hostedDomain;
  (void)signInOption;
  (void)clientId;
  (void)serverClientId;
  (void)forceCodeForRefreshToken;
#endif

  auto secret_doc = GetClientSecret();
  if (!SecretJsonPopulated(secret_doc)) {
    spdlog::error(
        "Confirm client_secret JSON file has been downloaded from the Google "
        "cloud console");
  }
  rapidjson::Document credentials_doc;
  if (credentials_doc = GetClientCredentials();
      !CredentialsJsonPopulated(credentials_doc)) {
    if (AuthCodeValuePresent(credentials_doc)) {
      credentials_doc = SwapAuthCodeForToken(secret_doc, credentials_doc);
    } else {
      CreateDefaultClientCredentialFile();
      secret_doc = GetClientSecret();
      std::string auth_uri = GetAuthUrl(secret_doc, requestedScopes);
      spdlog::error(
          "\tUpdate auth_code key in GOOGLE_API_OAUTH2_CLIENT_CREDENTIALS "
          "file with response from:\n\t{}",
          auth_uri);
    }
  } else {
    credentials_doc = RefreshToken(secret_doc, credentials_doc);
  }
  (void)credentials_doc;
}

flutter::EncodableValue GoogleSignInPlugin::GetUserData() {
  std::unique_ptr<std::vector<uint8_t>> result;

  if (auto credentials_doc = GetClientCredentials();
      CredentialsJsonPopulated(credentials_doc)) {
    auto o = credentials_doc.GetObject();
    std::string id_token = o[kKeyIdToken].GetString();
    std::string auth_code = o[kKeyAuthCode].GetString();
    std::string access_token = o[kKeyAccessToken].GetString();
    std::string token_type = o[kKeyTokenType].GetString();

    std::string auth_header =
        "Authorization: " + token_type + " " + access_token;
    std::vector<std::string> headers{"Content-Type: application/json",
                                     std::move(auth_header)};
    std::vector<std::pair<std::string, std::string>> url_form{};
    plugin_common_curl::CurlClient client;
    std::string url = kPeopleUrl;
    client.Init(url, headers, url_form);
    auto response = client.RetrieveContentAsString();

    if (client.GetCode() != CURLE_OK) {
      spdlog::error("[google_sign_in] curl failure {} - {}",
                    static_cast<int>(client.GetCode()), response);
      result = GetCodec().EncodeErrorEnvelope("http_client_failure", "");
      return flutter::EncodableValue(std::move(result));
    }

    rapidjson::Document doc;
    doc.Parse(response.c_str());
    if (doc.GetParseError() != rapidjson::kParseErrorNone) {
      spdlog::error("[google_sign_in] curl response parse failure: {} - {}",
                    static_cast<int>(doc.GetParseError()), response);
      result = GetCodec().EncodeErrorEnvelope("parse_error", "");
      return flutter::EncodableValue(std::move(result));
    }

    auto resp = doc.GetObject();

    // check for error
    if (resp.HasMember("error") && resp["error"].IsObject()) {
      if (auto obj = resp["error"].GetObject();
          obj.HasMember("code") && obj["code"].IsNumber() &&
          obj.HasMember("message") && obj["message"].IsString() &&
          obj.HasMember("status") && obj["status"].IsString()) {
        int code = obj["code"].GetInt();
        std::string message = obj["message"].GetString();
        std::string status = obj["status"].GetString();
        spdlog::error("[google_sign_in] [{}] {}: {}", code, status, message);
        result = GetCodec().EncodeErrorEnvelope(status, message);
        return flutter::EncodableValue(std::move(result));
      }
    }

    std::string id;
    if (resp.HasMember(kKeyResourceName) && resp[kKeyResourceName].IsString()) {
      id = resp[kKeyResourceName].GetString();
    }

    std::string email;
    if (resp.HasMember(kKeyEmailAddresses) &&
        resp[kKeyEmailAddresses].IsArray()) {
      auto arr = resp[kKeyEmailAddresses].GetArray();
      if (auto index0 = arr[0].GetObject();
          index0.HasMember(kKeyMetadata) && index0[kKeyMetadata].IsObject() &&
          index0.HasMember(kKeyValue) && index0[kKeyValue].IsString()) {
        if (auto meta_obj = index0[kKeyMetadata].GetObject();
            meta_obj.HasMember(kKeySourcePrimary) &&
            meta_obj[kKeySourcePrimary].IsBool() &&
            meta_obj[kKeySourcePrimary].GetBool()) {
          email = index0[kKeyValue].GetString();
        }
      }
    }

    std::string display_name;
    if (resp.HasMember(kKeyNames) && resp[kKeyNames].IsArray()) {
      auto arr = resp[kKeyNames].GetArray();
      if (auto index0 = arr[0].GetObject();
          index0.HasMember(kKeyMetadata) && index0[kKeyMetadata].IsObject() &&
          index0.HasMember(kKeyDisplayName) &&
          index0[kKeyDisplayName].IsString()) {
        if (auto meta_obj = index0[kKeyMetadata].GetObject();
            meta_obj.HasMember(kKeySourcePrimary) &&
            meta_obj[kKeySourcePrimary].IsBool() &&
            meta_obj[kKeySourcePrimary].GetBool()) {
          display_name = index0[kKeyDisplayName].GetString();
        }
      }
    }

    std::string photo_url;
    if (resp.HasMember(kKeyPhotos) && resp[kKeyPhotos].IsArray()) {
      auto arr = resp[kKeyPhotos].GetArray();
      if (auto index0 = arr[0].GetObject();
          index0.HasMember(kKeyMetadata) && index0[kKeyMetadata].IsObject() &&
          index0.HasMember(kKeyUrl) && index0[kKeyUrl].IsString()) {
        if (auto meta_obj = index0[kKeyMetadata].GetObject();
            meta_obj.HasMember(kKeyPrimary) && meta_obj[kKeyPrimary].IsBool() &&
            meta_obj[kKeyPrimary].GetBool()) {
          photo_url = index0[kKeyUrl].GetString();
        }
      }
    }
    SPDLOG_DEBUG("id: {}", id);
    SPDLOG_DEBUG("email: {}", email);
    SPDLOG_DEBUG("display_name: {}", display_name);
    SPDLOG_DEBUG("photo_url: {}", photo_url);

    auto res = flutter::EncodableValue(flutter::EncodableMap{
        {flutter::EncodableValue(kMethodResponseKeyId),
         flutter::EncodableValue(id)},
        {flutter::EncodableValue(kMethodResponseKeyEmail),
         flutter::EncodableValue(email)},
        {flutter::EncodableValue(kKeyDisplayName),
         flutter::EncodableValue(display_name)},
        {flutter::EncodableValue(kMethodResponseKeyPhotoUrl),
         flutter::EncodableValue(photo_url)},
        {flutter::EncodableValue(kMethodResponseKeyServerAuthCode),
         flutter::EncodableValue(auth_code.c_str())},
        {flutter::EncodableValue(kMethodResponseKeyIdToken),
         flutter::EncodableValue(id_token.c_str())},
    });
    result = GetCodec().EncodeSuccessEnvelope(&res);
  } else {
    result = GetCodec().EncodeErrorEnvelope("authentication_failure", "");
  }
  return flutter::EncodableValue(std::move(result));
}

flutter::EncodableValue GoogleSignInPlugin::GetTokens(
    const std::string& /* email */,
    bool /* shouldRecoverAuth */) {
  if (auto credentials_doc = GetClientCredentials();
      CredentialsJsonPopulated(credentials_doc)) {
    const auto o = credentials_doc.GetObject();
    const std::string access_token = o[kKeyAccessToken].GetString();
    const std::string id_token = o[kKeyIdToken].GetString();
    const std::string auth_code = o[kKeyAuthCode].GetString();

    return flutter::EncodableValue(std::move(flutter::EncodableMap{
        {flutter::EncodableValue(kMethodResponseKeyServerAuthCode),
         flutter::EncodableValue(auth_code.c_str())},
        {flutter::EncodableValue(kMethodResponseKeyIdToken),
         flutter::EncodableValue(id_token.c_str())},
        {flutter::EncodableValue(kMethodResponseKeyAccessToken),
         flutter::EncodableValue(access_token.c_str())}}));
  }
  std::unique_ptr<std::vector<uint8_t>> result =
      GetCodec().EncodeErrorEnvelope("authentication_failure", "");
  return flutter::EncodableValue(std::move(result));
}
#endif  // TODO

}  // namespace google_sign_in_plugin