add_executable(TextBlockTest
  TextBlockTest.cpp
  ${REPO_ROOT}/lib/Epub/Epub/blocks/TextBlock.cpp
)

target_include_directories(TextBlockTest PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/stubs
  ${REPO_ROOT}/lib/Epub/Epub/blocks
  ${REPO_ROOT}/lib/Epub
  ${REPO_ROOT}/lib/Memory
  ${REPO_ROOT}/lib/Serialization
)

target_link_libraries(TextBlockTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(TextBlockTest)
