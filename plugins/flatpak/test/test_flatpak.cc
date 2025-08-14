#include <chrono>
#include <fstream>
#include <utility>

#include <gtest/gtest.h>

#include <flutter/encodable_value.h>

#include "flatpak/flatpak_shim.h"

using namespace flatpak_plugin;

class FlatpakPluginTest : public ::testing::Test {
 protected:
  void SetUp() override {}
  void TearDown() override {}
};

TEST_F(FlatpakPluginTest, GetUserInstallationsTest) {
  const auto result = FlatpakShim::GetUserInstallation();

  EXPECT_FALSE(result.value().id().empty());
  EXPECT_FALSE(result.value().display_name().empty());
}

TEST_F(FlatpakPluginTest, GetSystemInstallationsTest) {
  const auto result = FlatpakShim::GetSystemInstallations();
  auto& system_installations = result.value();
  for (auto& system_installation : system_installations) {
    EXPECT_TRUE(std::holds_alternative<flutter::CustomEncodableValue>(
        system_installation));
  }
  EXPECT_GT(result.value().size(), 0);
}

TEST_F(FlatpakPluginTest, AddRemoteTest) {
  const auto remote = Remote(
      "full-remote", "https://full.example.com/repo", "", "Full Test Remote",
      "Comprehensive test comment", "Detailed test description",
      "https://full.example.com", "https://full.example.com/icon", "22.08",
      "org.example.App", "oci", "runtime/*", "2024-01-01T00:00:00Z",
      "/var/lib/flatpak/appstream", false, true, true, false, 10);

  auto result = FlatpakShim::RemoteAdd(remote);
  ASSERT_FALSE(result.has_error());
  EXPECT_TRUE(result.value());
  if (result.value()) {
    auto cleanremote = FlatpakShim::RemoteRemove("full-remote");
  }
}

TEST_F(FlatpakPluginTest, AddEmptyRemoteTest) {
  const auto remote = Remote("", "", "", "", "", "", "", "", "", "", "", "", "",
                             "", false, false, false, false, 1);

  const auto result = FlatpakShim::RemoteAdd(remote);
  EXPECT_TRUE(result.has_error());
}

// Install a real application and uninstall it in another test to clean
// environment.
TEST_F(FlatpakPluginTest, ApplicationInstallTest) {
  if (const auto result =
          FlatpakShim::ApplicationInstall("org.gnome.Calculator");
      !result.has_error()) {
    EXPECT_EQ(result.value(), true);
  }
}

TEST_F(FlatpakPluginTest, ApplicationInstallInvalidTest) {
  const auto result = FlatpakShim::ApplicationInstall("invalid.app.test");
  EXPECT_TRUE(result.has_error());
}

TEST_F(FlatpakPluginTest, ApplicationUninstallTest) {
  const auto result = FlatpakShim::ApplicationUninstall("org.gnome.Calculator");
  EXPECT_TRUE(result.value());
}

TEST_F(FlatpakPluginTest, ApplicationUninstallInvalidTest) {
  const auto result = FlatpakShim::ApplicationUninstall("invalid.app.test");
  EXPECT_TRUE(result.has_error());
}

TEST_F(FlatpakPluginTest, GetRemoteAppsTest) {
  const auto apps = FlatpakShim::GetApplicationsRemote("flathub");

  EXPECT_GT(apps.value().size(), 0);
}

TEST_F(FlatpakPluginTest, GetApplicationsInstalledTest) {
  const auto apps = FlatpakShim::GetApplicationsInstalled();

  EXPECT_GT(apps.value().size(), 0);
}

TEST_F(FlatpakPluginTest, FindAppInRemoteSearchTest) {
  GError* error = nullptr;
  FlatpakInstallation* user_installation =
      flatpak_installation_new_user(nullptr, &error);
  auto [key, value] =
      FlatpakShim::find_app_in_remotes(user_installation, "com.spotify.Client");
  // assuming flathub remote is added
  EXPECT_FALSE(value.empty());
  EXPECT_EQ(key, "flathub");
}
