set(libwallet_performance_tests_sources
  main.cpp
)

set(libwallet_performance_tests_headers
  )

add_executable(libwallet_performance_tests
  ${libwallet_performance_tests_sources}
  ${libwallet_performance_tests_headers})

target_link_libraries(libwallet_performance_tests
  PRIVATE
    wallet
    epee
    ${GTEST_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
    ${EXTRA_LIBRARIES})

set_property(TARGET libwallet_performance_tests
  PROPERTY
    FOLDER "tests")

if (NOT MSVC)
  set_property(TARGET libwallet_performance_tests
    APPEND_STRING
    PROPERTY
      COMPILE_FLAGS " -Wno-undef -Wno-sign-compare")
endif ()

add_test(
  NAME    libwallet_performance_tests
  COMMAND libwallet_performance_tests)
