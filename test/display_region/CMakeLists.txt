add_executable(DisplayRegionTest
  DisplayRegionTest.cpp
  ${REPO_ROOT}/lib/GfxRenderer/DisplayRegion.cpp
)

target_include_directories(DisplayRegionTest PRIVATE
  ${REPO_ROOT}/lib/GfxRenderer
)

target_link_libraries(DisplayRegionTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(DisplayRegionTest)
