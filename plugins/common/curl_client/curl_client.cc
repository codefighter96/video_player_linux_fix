/*
 * Copyright 2023-2024 Toyota Connected North America
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

#include "curl_client.h"

#include <curl/curl.h>
#include <curl/easy.h>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "../common/logging.h"

namespace plugin_common_curl {

CurlClient::CurlClient() : mCode(CURLE_OK) {
  curl_global_init(CURL_GLOBAL_DEFAULT);
  mErrorBuffer = std::make_unique<char[]>(CURL_ERROR_SIZE);
  mErrorBuffer[0] = '\0';
}

CurlClient::~CurlClient() {
  if (mConn) {
    curl_easy_cleanup(mConn);
  }

  if (mHeadersList) {
    curl_slist_free_all(mHeadersList);
  }

  curl_global_cleanup();
  mErrorBuffer.reset();
}

int CurlClient::StringWriter(const char* data,
                             const size_t size,
                             const size_t num_mem_block,
                             std::string* writerData) {
  writerData->append(data, size * num_mem_block);
  return static_cast<int>(size * num_mem_block);
}

int CurlClient::VectorWriter(char* data,
                             const size_t size,
                             const size_t num_mem_block,
                             std::vector<uint8_t>* writerData) {
  auto* u_data = reinterpret_cast<uint8_t*>(data);
  writerData->insert(writerData->end(), u_data, u_data + size * num_mem_block);
  return static_cast<int>(size * num_mem_block);
}

void CurlClient::SetBearerToken(const std::string& token) {
  if (token.empty()) {
    mAuthHeader.clear();
  } else {
    mAuthHeader = "Authorization: Bearer " + token;
  }
}

bool CurlClient::Init(
    const std::string& url,
    const std::vector<std::string>& headers,
    const std::vector<std::pair<std::string, std::string>>& url_form,
    const bool follow_location) {
  mCode = CURLE_OK;

  mConn = curl_easy_init();
  if (mConn == nullptr) {
    spdlog::error("[CurlClient] Failed to create CURL connection");
    return false;
  }

  mStringBuffer.clear();
  mVectorBuffer.clear();
  mPostFields.clear();

  if (mHeadersList) {
    curl_slist_free_all(mHeadersList);
    mHeadersList = nullptr;
  }

  mCode = curl_easy_setopt(mConn, CURLOPT_ERRORBUFFER, mErrorBuffer.get());
  if (mCode != CURLE_OK) {
    spdlog::error("[CurlClient] Failed to set error buffer [{}]",
                  static_cast<int>(mCode));
    return false;
  }

  mUrl = url;
  spdlog::trace("[CurlClient] URL: {}", mUrl);

  mCode = curl_easy_setopt(mConn, CURLOPT_URL, mUrl.c_str());
  if (mCode != CURLE_OK) {
    spdlog::error("[CurlClient] Failed to set URL [{}]", mErrorBuffer.get());
    return false;
  }

  if (!url_form.empty()) {
    for (const auto& [key, value] : url_form) {
      if (!mPostFields.empty()) {
        mPostFields += "&";
      }
      char* encoded_Key = curl_easy_escape(mConn, key.c_str(), key.length());
      char* encoded_value =
          curl_easy_escape(mConn, value.c_str(), value.length());
      if (encoded_Key && encoded_value) {
        mPostFields += encoded_Key;
        mPostFields += "=";
        mPostFields += encoded_value;
      }
      curl_free(encoded_Key);
      curl_free(encoded_value);
    }
    spdlog::trace("[CurlClient] PostFields: {}", mPostFields);
    curl_easy_setopt(mConn, CURLOPT_POSTFIELDSIZE, mPostFields.length());
    // libcurl does not copy
    curl_easy_setopt(mConn, CURLOPT_POSTFIELDS, mPostFields.c_str());
  }

  if (!mAuthHeader.empty()) {
    mHeadersList = curl_slist_append(mHeadersList, mAuthHeader.c_str());
  }
  for (const auto& header : headers) {
    spdlog::trace("[CurlClient] Header: {}", header);
    mHeadersList = curl_slist_append(mHeadersList, header.c_str());
  }
  if (mHeadersList) {
    mCode = curl_easy_setopt(mConn, CURLOPT_HTTPHEADER, mHeadersList);
  }
  if (mCode != CURLE_OK) {
    spdlog::error("[CurlClient] Failed to set headers option [{}]",
                  mErrorBuffer.get());
    return false;
  }

  mCode = curl_easy_setopt(mConn, CURLOPT_FOLLOWLOCATION,
                           follow_location ? 1L : 0L);
  if (mCode != CURLE_OK) {
    spdlog::error("[CurlClient] Failed to set redirect option [{}]",
                  mErrorBuffer.get());
    return false;
  }

  return true;
}

std::string CurlClient::Get(
    const std::string& url,
    const std::vector<std::string>& additional_headers) {
  if (Init(url, additional_headers, {})) {
    return RetrieveContentAsString();
  }
  return "";
}

std::string CurlClient::Post(
    const std::string& url,
    const std::vector<std::pair<std::string, std::string>>& form_data,
    const std::vector<std::string>& additional_headers) {
  if (Init(url, additional_headers, form_data)) {
    return RetrieveContentAsString();
  }
  return "";
}

std::string CurlClient::RetrieveContentAsString(const bool verbose) {
  if (mConn == nullptr) {
    return "";
  }

  curl_easy_setopt(mConn, CURLOPT_VERBOSE, verbose ? 1L : 0L);
  if (mCode != CURLE_OK) {
    spdlog::error("[CurlClient] Failed to set 'CURLOPT_VERBOSE' [{}]\n",
                  mErrorBuffer.get());
    return {};
  }

  mCode = curl_easy_setopt(mConn, CURLOPT_WRITEFUNCTION, StringWriter);
  if (mCode != CURLE_OK) {
    spdlog::error("[CurlClient] Failed to set writer [{}]", mErrorBuffer.get());
    return {};
  }

  mCode = curl_easy_setopt(mConn, CURLOPT_WRITEDATA, &mStringBuffer);
  if (mCode != CURLE_OK) {
    spdlog::error("[CurlClient] Failed to set write data [{}]",
                  mErrorBuffer.get());
    return {};
  }

  mCode = curl_easy_perform(mConn);
  if (mCode != CURLE_OK) {
    spdlog::error("[CurlClient] Failed to perform request : {}",
                  curl_easy_strerror(mCode));
    return "";
  }
  return mStringBuffer;
}

const std::vector<uint8_t>& CurlClient::RetrieveContentAsVector(
    const bool verbose) {
  if (mConn == nullptr) {
    mVectorBuffer.clear();
    return mVectorBuffer;
  }

  curl_easy_setopt(mConn, CURLOPT_VERBOSE, verbose ? 1L : 0L);
  if (mCode != CURLE_OK) {
    spdlog::error("[CurlClient] Failed to set 'CURLOPT_VERBOSE' [{}]\n",
                  mErrorBuffer.get());
    mVectorBuffer.clear();
    return mVectorBuffer;
  }

  mCode = curl_easy_setopt(mConn, CURLOPT_WRITEFUNCTION, VectorWriter);
  if (mCode != CURLE_OK) {
    spdlog::error("[CurlClient] Failed to set writer [{}]", mErrorBuffer.get());
    mVectorBuffer.clear();
    return mVectorBuffer;
  }

  mCode = curl_easy_setopt(mConn, CURLOPT_WRITEDATA, &mVectorBuffer);
  if (mCode != CURLE_OK) {
    spdlog::error("[CurlClient] Failed to set write data [{}]",
                  mErrorBuffer.get());
    mVectorBuffer.clear();
    return mVectorBuffer;
  }

  std::vector<uint8_t>().swap(mVectorBuffer);

  mCode = curl_easy_perform(mConn);
  if (mCode != CURLE_OK) {
    spdlog::error("[CurlClient] Failed to get '{}' [{}]\n", mUrl,
                  mErrorBuffer.get());
    mVectorBuffer.clear();
    return mVectorBuffer;
  }
  return mVectorBuffer;
}
}  // namespace plugin_common_curl