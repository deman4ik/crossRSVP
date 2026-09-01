add_executable(RsvpTest
  RsvpTest.cpp
  ${REPO_ROOT}/lib/Rsvp/EpubVisibleTextSource.cpp
  ${REPO_ROOT}/lib/Rsvp/RsvpRefreshStats.cpp
  ${REPO_ROOT}/lib/Rsvp/RsvpSession.cpp
  ${REPO_ROOT}/lib/Rsvp/VisibleContentStream.cpp
  ${REPO_ROOT}/lib/Epub/Epub/htmlEntities.cpp
)

find_package(Python3 REQUIRED COMPONENTS Interpreter)
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
  VERBATIM
)
add_custom_target(RsvpFixture DEPENDS "${RSVP_FIXTURE_EPUB}")
add_dependencies(RsvpTest RsvpFixture)

target_include_directories(RsvpTest PRIVATE
  ${REPO_ROOT}/lib/Rsvp
  ${REPO_ROOT}/lib/Epub
  ${REPO_ROOT}/lib/Memory
)

target_link_libraries(RsvpTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

target_compile_definitions(RsvpTest PRIVATE
  RSVP_FIXTURE_EPUB="${RSVP_FIXTURE_EPUB}"
)

gtest_discover_tests(RsvpTest)
