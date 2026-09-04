find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(RSVP_I18N_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated_i18n")
set(RSVP_I18N_CPP "${RSVP_I18N_DIR}/I18nStrings.cpp")
set(RSVP_I18N_HEADER "${RSVP_I18N_DIR}/I18nStrings.h")
set(RSVP_I18N_KEYS "${RSVP_I18N_DIR}/I18nKeys.h")
file(GLOB RSVP_TRANSLATIONS CONFIGURE_DEPENDS "${REPO_ROOT}/lib/I18n/translations/*.yaml")
FetchContent_Declare(
  ArduinoJson
  GIT_REPOSITORY https://github.com/bblanchon/ArduinoJson.git
  GIT_TAG v7.4.2
)
FetchContent_MakeAvailable(ArduinoJson)
add_custom_command(
  OUTPUT "${RSVP_I18N_CPP}" "${RSVP_I18N_HEADER}" "${RSVP_I18N_KEYS}"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${RSVP_I18N_DIR}"
  COMMAND "${Python3_EXECUTABLE}" "${REPO_ROOT}/scripts/gen_i18n.py"
          "${REPO_ROOT}/lib/I18n/translations" "${RSVP_I18N_DIR}"
  DEPENDS "${REPO_ROOT}/scripts/gen_i18n.py" ${RSVP_TRANSLATIONS}
  VERBATIM
)

add_executable(RsvpTest
  RsvpTest.cpp
  RsvpLexicalCoreTest.cpp
  RsvpCheckpointTest.cpp
  RsvpCheckpointFileTest.cpp
  RsvpResumeLifecycleTest.cpp
  RsvpSessionPlaybackTest.cpp
  RsvpModeSwitchTest.cpp
  RsvpContextLineLayoutTest.cpp
  RsvpContextWindowTest.cpp
  RsvpContextLineSettingsTest.cpp
  RsvpPendingActionsTest.cpp
  EpubCacheUtilsTest.cpp
  ${REPO_ROOT}/lib/Rsvp/EpubVisibleTextSource.cpp
  ${REPO_ROOT}/lib/Rsvp/RsvpModeSwitch.cpp
  ${REPO_ROOT}/lib/Rsvp/RsvpCheckpoint.cpp
  ${REPO_ROOT}/src/activities/reader/RsvpCheckpointFile.cpp
  ${REPO_ROOT}/src/util/EpubCacheUtils.cpp
  ${REPO_ROOT}/lib/Rsvp/RsvpRefreshStats.cpp
  ${REPO_ROOT}/lib/Rsvp/RsvpSettings.cpp
  ${REPO_ROOT}/lib/Rsvp/RsvpSession.cpp
  ${REPO_ROOT}/lib/Rsvp/RsvpLexicalCore.cpp
  ${REPO_ROOT}/lib/Rsvp/RsvpWordLayout.cpp
  ${REPO_ROOT}/lib/Rsvp/VisibleContentStream.cpp
  ${REPO_ROOT}/lib/I18n/I18n.cpp
  ${RSVP_I18N_CPP}
  ${REPO_ROOT}/lib/Epub/Epub/htmlEntities.cpp
  ${REPO_ROOT}/lib/Utf8/Utf8.cpp
)

set(RSVP_FIXTURE_EPUB "${CMAKE_CURRENT_BINARY_DIR}/rsvp-russian.epub")
add_custom_command(
  OUTPUT "${RSVP_FIXTURE_EPUB}"
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/build_fixture.py"
          "${CMAKE_CURRENT_SOURCE_DIR}/fixture" "${RSVP_FIXTURE_EPUB}"
  DEPENDS
    build_fixture.py
    fixture/mimetype
    fixture/META-INF/container.xml
    fixture/OEBPS/content.opf
    fixture/OEBPS/chapter.xhtml
    fixture/OEBPS/chapter2.xhtml
    fixture/OEBPS/test.png
  VERBATIM
)
add_custom_target(RsvpFixture DEPENDS "${RSVP_FIXTURE_EPUB}")
add_dependencies(RsvpTest RsvpFixture)

target_include_directories(RsvpTest PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/stubs
  ${RSVP_I18N_DIR}
  ${REPO_ROOT}/src/activities/reader
  ${REPO_ROOT}/src
  ${REPO_ROOT}/lib/I18n
  ${REPO_ROOT}/lib/Serialization
  ${REPO_ROOT}/lib/Rsvp
  ${REPO_ROOT}/lib/Epub
  ${REPO_ROOT}/lib/Memory
  ${REPO_ROOT}/lib/Utf8
  ${REPO_ROOT}/src/util
)

target_link_libraries(RsvpTest PRIVATE
  crosspoint_test_common
  ArduinoJson
  GTest::gtest_main
)

target_compile_definitions(RsvpTest PRIVATE
  RSVP_FIXTURE_EPUB="${RSVP_FIXTURE_EPUB}"
  RSVP_SETTINGS_LIST_SOURCE="${REPO_ROOT}/src/SettingsList.h"
)

gtest_discover_tests(RsvpTest)
