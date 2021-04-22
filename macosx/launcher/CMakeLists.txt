set(resource_dirs
    ${CMAKE_SOURCE_DIR}/base
    ${CMAKE_SOURCE_DIR}/docs
    ${CMAKE_SOURCE_DIR}/user
)
set(resource_files
    ${CMAKE_SOURCE_DIR}/AUTHORS
    ${CMAKE_SOURCE_DIR}/COPYING
    Eternity.icns
    DumpConsole.xib
    English.lproj/About.xib
    English.lproj/Launcher.xib
)

set(resource_common ${resource_dirs} ${resource_files})
set_source_files_properties(${resource_common} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)

add_executable(
    EternityLauncher MACOSX_BUNDLE
    ${resource_common}
    ELAboutController.h
    ELAboutController.m
    ELCommandLineArgument.h
    ELCommandLineArgument.m
    ELCommandLineArray.h
    ELCommandLineArray.m
    ELDumpConsole.h
    ELDumpConsole.m
    ELFileViewDataSource.h
    ELFileViewDataSource.m
    ELTextFieldDelegate.h
    ELTextFieldDelegate.m
    ${CMAKE_CURRENT_SOURCE_DIR}/EternityLaunch_Prefix.pch
    LauncherController.h
    LauncherController.m
    MainStart.m
)

add_custom_command(
    TARGET EternityLauncher PRE_BUILD
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/prepare_game_binaries.sh
    $<TARGET_FILE_DIR:eternity>
    ${CMAKE_CURRENT_BINARY_DIR}
    VERBATIM
)

set(binary_resources
    ${CMAKE_CURRENT_BINARY_DIR}/eternity
    ${CMAKE_CURRENT_BINARY_DIR}/SDL2.framework
    ${CMAKE_CURRENT_BINARY_DIR}/SDL2_mixer.framework
    ${CMAKE_CURRENT_BINARY_DIR}/SDL2_net.framework
)

target_sources(
    EternityLauncher PRIVATE
    ${binary_resources}
)

set_source_files_properties(
    ${binary_resources} PROPERTIES
    MACOSX_PACKAGE_LOCATION Resources
    GENERATED TRUE
)

set(app_display_name "Eternity Engine")
set_target_properties(
    EternityLauncher PROPERTIES

    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/EternityLaunch-Info.plist.in"

    MACOSX_BUNDLE_BUNDLE_NAME ${app_display_name}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_EXECUTABLE_NAME ${app_display_name}
    MACOSX_BUNDLE_GUI_IDENTIFIER "$(PRODUCT_BUNDLE_IDENTIFIER)"
    MACOSX_BUNDLE_LONG_VERSION_STRING ${PROJECT_VERSION}
    MACOSX_BUNDLE_ICON_FILE "Eternity"
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}

    OUTPUT_NAME ${app_display_name}

    XCODE_ATTRIBUTE_GCC_PREFIX_HEADER ${CMAKE_CURRENT_SOURCE_DIR}/EternityLaunch_Prefix.pch
    XCODE_ATTRIBUTE_GCC_PRECOMPILE_PREFIX_HEADER YES

    XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "net.mancubus.Eternity-Engine"

    XCODE_ATTRIBUTE_INSTALL_PATH "/Applications"
    XCODE_ATTRIBUTE_SKIP_INSTALL "No"
    XCODE_ATTRIBUTE_ENABLE_HARDENED_RUNTIME "YES"
)

find_library(COCOA_FRAMEWORK Cocoa)
target_link_libraries(EternityLauncher PUBLIC ${COCOA_FRAMEWORK})
