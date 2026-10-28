cmake_minimum_required(VERSION 3.22)
project(FluidScope VERSION 0.4.12 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_OSX_DEPLOYMENT_TARGET 12.0)
set(JUCE_PATH "${CMAKE_CURRENT_LIST_DIR}/vendor/JUCE" CACHE PATH "JUCE source directory")
add_subdirectory("${JUCE_PATH}" juce)
juce_add_plugin(FluidScope COMPANY_NAME "verstaerker" BUNDLE_ID "audio.tino.fluidscope"
# Keep existing four-character IDs so saved sessions still recognise the plugin.
    PLUGIN_MANUFACTURER_CODE Tino PLUGIN_CODE FlSc PRODUCT_NAME "FluidScope"
    ICON_BIG "${CMAKE_CURRENT_LIST_DIR}/assets/FluidScope.png"
    PLIST_TO_MERGE "<plist version=\"1.0\"><dict><key>CFBundleIconName</key><string>FluidScope</string></dict></plist>"
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "FluidScope analyses the audio input you select. Audio stays on this Mac."
    FORMATS VST3 AU Standalone IS_SYNTH FALSE NEEDS_MIDI_INPUT FALSE COPY_PLUGIN_AFTER_BUILD FALSE)
# Keep the verified native ICNS fallback and modern icon stack together. The
# compiled catalog is included so ordinary builds do not require Icon Composer.
get_target_property(fluid_icon_path FluidScope JUCE_ICON_FILE)
configure_file("${CMAKE_CURRENT_LIST_DIR}/assets/FluidScope.icns" "${fluid_icon_path}" COPYONLY)
set_source_files_properties("${CMAKE_CURRENT_LIST_DIR}/assets/Assets.car" PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
foreach(fluid_bundle FluidScope_Standalone FluidScope_VST3 FluidScope_AU)
    target_sources(${fluid_bundle} PRIVATE "${CMAKE_CURRENT_LIST_DIR}/assets/Assets.car")
endforeach()
# Finder caches app icons by bundle path/mtime; refresh after resource changes.
add_custom_command(TARGET FluidScope_Standalone POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E touch "$<TARGET_BUNDLE_DIR:FluidScope_Standalone>"
    VERBATIM)
juce_generate_juce_header(FluidScope)
target_sources(FluidScope PRIVATE Source/Plugin.cpp Source/WindowOrder.mm)
target_compile_definitions(FluidScope PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0 JUCE_REPORT_APP_USAGE=0)
target_link_libraries(FluidScope PRIVATE juce::juce_audio_utils juce::juce_gui_extra juce::juce_dsp juce::juce_opengl PUBLIC juce::juce_recommended_config_flags)
enable_testing()
add_executable(ScopeTests tests/ScopeTests.cpp)
add_test(NAME ScopeTests COMMAND ScopeTests)
add_executable(MeterTests tests/MeterTests.cpp)
add_test(NAME MeterTests COMMAND MeterTests)
juce_add_console_app(FluidScopeCheck PRODUCT_NAME "FluidScopeCheck")
target_sources(FluidScopeCheck PRIVATE tests/Integration.cpp)
target_include_directories(FluidScopeCheck PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/FluidScope_artefacts/JuceLibraryCode")
target_link_libraries(FluidScopeCheck PRIVATE FluidScope juce::juce_audio_utils juce::juce_gui_extra)
add_test(NAME Integration COMMAND FluidScopeCheck)
target_compile_definitions(FluidScopeCheck PRIVATE JUCE_MODAL_LOOPS_PERMITTED=1)
