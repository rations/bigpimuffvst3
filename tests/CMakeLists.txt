# BigBubbleMuff test suite — JUCE UnitTest based console runner.
# No third-party test framework (supply-chain minimalism).

juce_add_console_app(bbm_tests PRODUCT_NAME "bbm_tests")

juce_generate_juce_header(bbm_tests)

set(BBM_TEST_SOURCES
  main.cpp
  dsp_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../src/dsp/BigMuffPi.cpp)

target_sources(bbm_tests PRIVATE ${BBM_TEST_SOURCES})

set_source_files_properties(${BBM_TEST_SOURCES} TARGET_DIRECTORY bbm_tests
  PROPERTIES COMPILE_OPTIONS "${BBM_WARNING_FLAGS}")

target_include_directories(bbm_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)

# NB: do NOT define JUCE_UNIT_TESTS=1 — that registers JUCE's own internal test
# suite into the runner. We only want our tests (registered via static globals).
target_compile_definitions(bbm_tests PUBLIC
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  JUCE_STANDALONE_APPLICATION=1)

target_link_libraries(bbm_tests
  PRIVATE
    juce::juce_dsp
    chowdsp::chowdsp_wdf
    BigBubbleMuff::hardening
    BigBubbleMuff::sanitize
  PUBLIC
    juce::juce_recommended_config_flags)

add_test(NAME bbm_unit_tests COMMAND bbm_tests)
set_tests_properties(bbm_unit_tests PROPERTIES
  FAIL_REGULAR_EXPRESSION "FAILED|!!! Test")
