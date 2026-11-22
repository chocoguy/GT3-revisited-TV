# Release packaging: application icon, macOS Info.plist and install rules for
# the redistributable archives built by CI (see docs/dev/PLATFORMS.md).
# No game data is ever installed.
#
#   cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release -DGT3R_FETCH_SDL=ON
#   cmake --build build-release --target gt3r gt3tool
#   cmake --install build-release --prefix dist

set(GT3R_PACKAGING_DIR ${CMAKE_SOURCE_DIR}/packaging)

if(TARGET gt3r AND IOS)
  # An asset catalog with the single 1024 px icon (Xcode makes every size from it). It is a
  # directory source, so it is written at configure time; Xcode's actool compiles it.
  set(_xcassets ${CMAKE_BINARY_DIR}/gt3r-ios.xcassets)
  configure_file(${GT3R_PACKAGING_DIR}/icon.png ${_xcassets}/AppIcon.appiconset/icon.png COPYONLY)
  file(WRITE ${_xcassets}/Contents.json "{\"info\": {\"author\": \"xcode\", \"version\": 1}}\n")
  file(WRITE ${_xcassets}/AppIcon.appiconset/Contents.json
    "{\"images\": [{\"filename\": \"icon.png\", \"idiom\": \"universal\", \"platform\": \"ios\",\n"
    "  \"size\": \"1024x1024\"}],\n"
    " \"info\": {\"author\": \"xcode\", \"version\": 1}}\n")
  target_sources(gt3r PRIVATE ${_xcassets})
  set_source_files_properties(${_xcassets} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
  set_target_properties(gt3r PROPERTIES
    MACOSX_BUNDLE_INFO_PLIST ${GT3R_PACKAGING_DIR}/Info-iOS.plist.in
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    XCODE_ATTRIBUTE_ASSETCATALOG_COMPILER_APPICON_NAME AppIcon)
elseif(TARGET gt3r AND GT3R_TVOS)
  # The asset catalog packaging/make_icon.py writes: tvOS takes only a layered icon (an image
  # stack with an opaque back layer) and wants the Top Shelf pictures with it, all in one brand
  # asset. Xcode's actool compiles it.
  set(_xcassets ${GT3R_PACKAGING_DIR}/tvos/gt3r-tvos.xcassets)
  target_sources(gt3r PRIVATE ${_xcassets})
  set_source_files_properties(${_xcassets} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
  set_target_properties(gt3r PROPERTIES
    MACOSX_BUNDLE_INFO_PLIST ${GT3R_PACKAGING_DIR}/Info-tvOS.plist.in
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    XCODE_ATTRIBUTE_ASSETCATALOG_COMPILER_APPICON_NAME AppIcon)
  # The builder's own disc image inside the app (docs/dev/PLATFORMS.md "tvOS"): tvOS has no file
  # picker, and the only folder an app writes there can be emptied by the system. The image is
  # copied from where it is into the built app's disc/ folder (platform/tv_discs.h), never into
  # the source tree, and such an app is for the builder's own Apple TV, not for anyone else.
  set(GT3R_TVOS_DISC "" CACHE FILEPATH "tvOS: a disc image (.iso or .chd) of your own to build into the app")
  if(GT3R_TVOS_DISC)
    if(NOT EXISTS "${GT3R_TVOS_DISC}" OR IS_DIRECTORY "${GT3R_TVOS_DISC}")
      message(FATAL_ERROR "GT3R_TVOS_DISC: ${GT3R_TVOS_DISC} is not a file")
    endif()
    if(NOT GT3R_TVOS_DISC MATCHES "\\.([iI][sS][oO]|[cC][hH][dD])$")
      message(FATAL_ERROR "GT3R_TVOS_DISC: a disc image is a .iso or a .chd")
    endif()
    target_sources(gt3r PRIVATE "${GT3R_TVOS_DISC}")
    set_source_files_properties("${GT3R_TVOS_DISC}" PROPERTIES
      MACOSX_PACKAGE_LOCATION Resources/disc HEADER_FILE_ONLY TRUE)
    message(STATUS "tvOS: building ${GT3R_TVOS_DISC} into the app")
  endif()
elseif(TARGET gt3r AND APPLE)
  # .icns generated from packaging/icon.png with the macOS tools.
  set(_iconset ${CMAKE_BINARY_DIR}/gt3r.iconset)
  set(_icns ${CMAKE_BINARY_DIR}/gt3r.icns)
  set(_icon_cmds)
  foreach(sz 16 32 128 256 512)
    math(EXPR sz2 "${sz} * 2")
    list(APPEND _icon_cmds
      COMMAND sips -z ${sz} ${sz} ${GT3R_PACKAGING_DIR}/icon.png --out ${_iconset}/icon_${sz}x${sz}.png > /dev/null
      COMMAND sips -z ${sz2} ${sz2} ${GT3R_PACKAGING_DIR}/icon.png --out ${_iconset}/icon_${sz}x${sz}@2x.png > /dev/null)
  endforeach()
  add_custom_command(OUTPUT ${_icns}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${_iconset}
    ${_icon_cmds}
    COMMAND iconutil -c icns ${_iconset} -o ${_icns}
    DEPENDS ${GT3R_PACKAGING_DIR}/icon.png
    COMMENT "Building gt3r.icns")
  target_sources(gt3r PRIVATE ${_icns})
  set_source_files_properties(${_icns} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
  set_target_properties(gt3r PROPERTIES
    MACOSX_BUNDLE_INFO_PLIST ${GT3R_PACKAGING_DIR}/Info.plist.in
    MACOSX_BUNDLE_ICON_FILE gt3r.icns
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION})
elseif(TARGET gt3r AND WIN32)
  target_sources(gt3r PRIVATE ${GT3R_PACKAGING_DIR}/gt3r.rc)
endif()

if(ANDROID OR GT3R_APPLE_DEVICE) # (packaged by Gradle, or installed from Xcode by each builder)
  return()
endif()

set(_tools)
if(TARGET gt3tool)
  set(_tools gt3tool)
endif()

if(APPLE)
  install(TARGETS gt3r BUNDLE DESTINATION .)
  if(_tools)
    install(TARGETS ${_tools} RUNTIME DESTINATION .)
  endif()
  install(FILES ${CMAKE_SOURCE_DIR}/README.md ${CMAKE_SOURCE_DIR}/LICENSE
          ${CMAKE_SOURCE_DIR}/THIRD-PARTY-NOTICES.md DESTINATION .)
  install(FILES ${CMAKE_SOURCE_DIR}/third_party/fonts/barlow/OFL.txt DESTINATION . RENAME Barlow-OFL.txt)
  # Seal the finished bundle with an ad-hoc signature (no Apple account needed). The
  # linker signs only the executable; without sealing the bundle's resources the
  # signature is invalid and macOS calls a downloaded copy "damaged" instead of
  # offering Open Anyway (README "Installing").
  install(CODE "execute_process(COMMAND codesign --force --deep --sign - \"\${CMAKE_INSTALL_PREFIX}/gt3r.app\" RESULT_VARIABLE _rc)
    if(NOT _rc EQUAL 0)
      message(FATAL_ERROR \"codesign (ad-hoc) of gt3r.app failed\")
    endif()")
elseif(WIN32)
  install(TARGETS gt3r ${_tools} RUNTIME DESTINATION .)
  # SDL3.dll (GT3R_SDL_SHARED) and any other DLLs the game links.
  install(FILES $<TARGET_RUNTIME_DLLS:gt3r> DESTINATION .)
  if(GT3R_ANGLE_DIR) # ANGLE for the OpenGL ES renderer (CMakeLists.txt)
    install(FILES ${GT3R_ANGLE_FILES} ${GT3R_ANGLE_DIR}/LICENSE-ANGLE.txt DESTINATION .)
  endif()
  if(GT3R_OPENXR_LOADER) # VR builds (cmake/Dependencies.cmake)
    install(FILES ${GT3R_OPENXR_LOADER} DESTINATION .)
    install(FILES ${CMAKE_SOURCE_DIR}/third_party/openxr/LICENSE DESTINATION . RENAME LICENSE-OpenXR.txt)
  endif()
  install(FILES ${CMAKE_SOURCE_DIR}/README.md ${CMAKE_SOURCE_DIR}/LICENSE
          ${CMAKE_SOURCE_DIR}/THIRD-PARTY-NOTICES.md DESTINATION .)
  install(FILES ${CMAKE_SOURCE_DIR}/third_party/fonts/barlow/OFL.txt DESTINATION . RENAME Barlow-OFL.txt)
else()
  include(GNUInstallDirs)
  install(TARGETS gt3r ${_tools} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
  install(FILES ${GT3R_PACKAGING_DIR}/gt3r.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
  install(FILES ${GT3R_PACKAGING_DIR}/icon.png DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/1024x1024/apps
          RENAME gt3r.png)
  install(FILES ${CMAKE_SOURCE_DIR}/README.md ${CMAKE_SOURCE_DIR}/LICENSE
          ${CMAKE_SOURCE_DIR}/THIRD-PARTY-NOTICES.md DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/gt3r)
  install(FILES ${CMAKE_SOURCE_DIR}/third_party/fonts/barlow/OFL.txt DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/gt3r
          RENAME Barlow-OFL.txt)
endif()
