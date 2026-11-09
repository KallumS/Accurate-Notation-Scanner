# The app, in JUCE (decision 0005), pinned to the version the family builds with.

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.15
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(JUCE)

juce_add_gui_app(AccurateNotationScanner
    PRODUCT_NAME          "Accurate Notation Scanner"
    COMPANY_NAME          "kallums"
    BUNDLE_ID             "com.kallums.accuratenotationscanner"
    VERSION               ${PROJECT_VERSION}
    DOCUMENT_EXTENSIONS   pdf
    ICON_BIG              ${CMAKE_CURRENT_SOURCE_DIR}/Resources/Icon.png)

target_include_directories(AccurateNotationScanner PRIVATE Source/App)
target_sources(AccurateNotationScanner PRIVATE
    Source/App/Main.cpp
    Source/App/MainComponent.cpp
    Source/App/Player.cpp
    Source/App/Theme.cpp
    Source/App/SystemSettings.cpp)

target_compile_definitions(AccurateNotationScanner PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:AccurateNotationScanner,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:AccurateNotationScanner,JUCE_VERSION>"
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
    JUCE_PLUGINHOST_AU=1
    JUCE_PLUGINHOST_VST3=0
    JUCE_USE_FLAC=0
    JUCE_USE_OGGVORBIS=0)

target_link_libraries(AccurateNotationScanner
    PRIVATE
        ScannerCore
        juce::juce_gui_extra
        juce::juce_audio_utils
        juce::juce_audio_processors
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
