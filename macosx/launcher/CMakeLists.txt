#
# Add path as resource (also used in Eureka when deploying its paths)
# https://stackoverflow.com/a/66401708/11738219
#
function(add_path_resource target folder)
    file(GLOB_RECURSE resources "${CMAKE_CURRENT_SOURCE_DIR}/${folder}/*")
    target_sources(${target} PRIVATE ${resources})
    foreach(resfile ${resources})
        #Get the relative path from the data-folder to the particular file
        file(RELATIVE_PATH newfile "${CMAKE_CURRENT_SOURCE_DIR}/${folder}" ${resfile})

        get_filename_component(newfilepath ${newfile} DIRECTORY)
        #Set its location inside the app package (under Resources)
        set_property(SOURCE ${resfile} PROPERTY MACOSX_PACKAGE_LOCATION "Resources/${folder}/${newfilepath}")

    endforeach()
    source_group("eureka-res-clutter" FILES ${resources})
endfunction()

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
    TARGET EternityLauncher POST_BUILD
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/copy_game_to_bundle.sh
    $<TARGET_FILE_DIR:eternity>
    $<TARGET_FILE_DIR:EternityLauncher>
    VERBATIM
)

foreach(dir ${resource_dirs})
    add_path_resource(EternityLauncher ${dir})
endforeach()

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
