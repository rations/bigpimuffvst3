// BigBubbleMuff — test runner entry point.
// Copyright (C) 2026  BigBubbleMuff contributors. GPL-3.0-or-later (see COPYING).
//
// Runs every registered juce::UnitTest and returns non-zero if any fail, so
// CTest reports a clean pass/fail.
#include <juce_core/juce_core.h>

int main() {
  juce::UnitTestRunner runner;
  runner.setAssertOnFailure(false);
  runner.runAllTests();

  int failures = 0;
  for (int i = 0; i < runner.getNumResults(); ++i) {
    if (auto *r = runner.getResult(i))
      failures += r->failures;
  }

  if (failures > 0)
    std::cerr << "FAILED: " << failures << " unit-test assertion(s)\n";
  else
    std::cout << "All BigBubbleMuff unit tests passed.\n";

  return failures > 0 ? 1 : 0;
}
