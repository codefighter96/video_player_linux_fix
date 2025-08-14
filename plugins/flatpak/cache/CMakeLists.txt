find_package(SQLite3 REQUIRED)

add_library(plugin_flatpak_cache STATIC
        cache_manager.cc
        network/curl_network_fetcher.cc
        observers/observers.cc
        storage/sqlite_cache_storage.cc
        operations/encodablelist_cache_operation.h
)
add_sanitizers(plugin_flatpak_cache)

target_include_directories(plugin_flatpak_cache PUBLIC
        .
        interfaces
        network
        observers
        operations
        storage
        utils
)

target_link_libraries(plugin_flatpak_cache PUBLIC
        flutter
        plugin_common
        plugin_common_curl
        PkgConfig::FLATPAK_DEPS
        SQLite::SQLite3
        plugin_flatpak
)

if (BUILD_UNIT_TESTS)
    add_subdirectory(test)
endif ()