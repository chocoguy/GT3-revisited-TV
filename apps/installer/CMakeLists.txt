# GT3 Revisited Installer: the Mac app players download (signed with a Developer ID and notarized by
# tools/release.sh). It builds the game from their own disc image at a GitHub release tag and installs
# it on their Apple TVs, through tools/tvos/port. It doesn't update itself: a new major version is a
# new installer (docs/release.md).
#
# A project of its own (the game's CMakeLists.txt is not involved):
#   cmake -S apps/installer -B build-installer -G Xcode            the app (Xcode only)
#   cmake -S apps/installer -B build-installertest -G Ninja
#   cmake --build build-installertest --target gt3r_installertest  its logic, without the UI (any generator)
cmake_minimum_required(VERSION 3.21)
project(GT3RevisitedInstaller LANGUAGES Swift)

set(GT3R_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../..")
if(NOT CMAKE_OSX_DEPLOYMENT_TARGET)
  set(CMAKE_OSX_DEPLOYMENT_TARGET 14.0)
endif()

# The installer's version is the release it was built for (port.json, or -DGT3R_INSTALLER_VERSION from
# release.sh, which builds it only for a new major version, N.0.0). It builds the releases of its major.
set(GT3R_INSTALLER_VERSION "" CACHE STRING "The release version (the tag without its v; empty: port.json's)")
set(GT3R_INSTALLER_COMMIT "" CACHE STRING "The commit built (short hash)")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${GT3R_ROOT}/port.json")
if(GT3R_INSTALLER_VERSION STREQUAL "")
  file(READ "${GT3R_ROOT}/port.json" _port_json)
  string(JSON GT3R_INSTALLER_VERSION GET "${_port_json}" version)
endif()
if(NOT GT3R_INSTALLER_VERSION MATCHES "^([0-9]+)\\.([0-9]+)\\.([0-9]+)(-[A-Za-z0-9.]+)?$")
  message(FATAL_ERROR "GT3R_INSTALLER_VERSION ${GT3R_INSTALLER_VERSION} isn't a version like 1.2.0")
endif()
set(GT3R_INSTALLER_VERSION_NUMBER "${CMAKE_MATCH_1}.${CMAKE_MATCH_2}.${CMAKE_MATCH_3}")
math(EXPR GT3R_INSTALLER_BUILD "${CMAKE_MATCH_1} * 10000 + ${CMAKE_MATCH_2} * 100 + ${CMAKE_MATCH_3}")

# The logic without the UI, checked by tests/installertest with any generator.
set(GT3R_INSTALLER_LOGIC_SOURCES
  Sources/PortEvent.swift
  Sources/Releases.swift
  Sources/SemVer.swift
  Sources/Workspace.swift)
add_executable(gt3r_installertest EXCLUDE_FROM_ALL "${GT3R_ROOT}/tests/installertest/main.swift"
               ${GT3R_INSTALLER_LOGIC_SOURCES})
set_target_properties(gt3r_installertest PROPERTIES Swift_LANGUAGE_VERSION 5)

if(NOT CMAKE_GENERATOR STREQUAL "Xcode")
  return()
endif()

set(GT3R_INSTALLER_BUNDLE_ID "com.chocoguy.gt3r.installer" CACHE STRING "Bundle identifier for GT3 Revisited Installer")
# Ad hoc for local builds; tools/release.sh signs with "Developer ID Application".
set(GT3R_INSTALLER_SIGN_IDENTITY "-" CACHE STRING "Code signing identity for GT3 Revisited Installer")
set(GT3R_INSTALLER_TEAM "" CACHE STRING "The Developer ID's team (release builds)")

set(_sources
  Sources/AppModel.swift
  Sources/Doctor.swift
  Sources/InstallerApp.swift
  Sources/PortEvent.swift
  Sources/Releases.swift
  Sources/SemVer.swift
  Sources/Shell.swift
  Sources/Source.swift
  Sources/Views.swift
  Sources/Workspace.swift)
# Assets.xcassets holds AccentColor (#5F8DD3), the app's accent.
add_executable(GT3RevisitedInstaller MACOSX_BUNDLE ${_sources} AppIcon.icns Assets.xcassets)
set_source_files_properties(AppIcon.icns Assets.xcassets PROPERTIES MACOSX_PACKAGE_LOCATION Resources)

set_target_properties(GT3RevisitedInstaller PROPERTIES
  OUTPUT_NAME "GT3 Revisited Installer"
  MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in"
  MACOSX_BUNDLE_GUI_IDENTIFIER "${GT3R_INSTALLER_BUNDLE_ID}"
  MACOSX_BUNDLE_BUNDLE_NAME "GT3 Revisited Installer"
  MACOSX_BUNDLE_BUNDLE_VERSION "${GT3R_INSTALLER_BUILD}"
  MACOSX_BUNDLE_SHORT_VERSION_STRING "${GT3R_INSTALLER_VERSION_NUMBER}"
  MACOSX_BUNDLE_ICON_FILE AppIcon
  XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "${GT3R_INSTALLER_BUNDLE_ID}"
  XCODE_ATTRIBUTE_SWIFT_VERSION 5.0
  XCODE_ATTRIBUTE_ASSETCATALOG_COMPILER_GLOBAL_ACCENT_COLOR_NAME AccentColor
  XCODE_ATTRIBUTE_CODE_SIGN_STYLE Manual
  XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "${GT3R_INSTALLER_SIGN_IDENTITY}"
  XCODE_ATTRIBUTE_DEVELOPMENT_TEAM "${GT3R_INSTALLER_TEAM}"
  XCODE_ATTRIBUTE_ENABLE_HARDENED_RUNTIME YES
  XCODE_ATTRIBUTE_OTHER_CODE_SIGN_FLAGS "--timestamp"
  XCODE_GENERATE_SCHEME ON)
if(GT3R_INSTALLER_SIGN_IDENTITY STREQUAL "-")
  # No timestamp server for ad hoc signatures.
  set_target_properties(GT3RevisitedInstaller PROPERTIES XCODE_ATTRIBUTE_OTHER_CODE_SIGN_FLAGS "")
endif()

target_link_libraries(GT3RevisitedInstaller PRIVATE
  "-framework AppKit"
  "-framework SwiftUI")
