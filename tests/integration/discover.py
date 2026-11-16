# Copyright © 2026 CCP ehf.
"""Prints each case in run_tests.py, one per line, by the name run_tests.py takes on its command line."""
import unittest

import run_tests


def iterate_tests(suite):
    for test in suite:
        if isinstance(test, unittest.TestSuite):
            yield from iterate_tests(test)
        else:
            yield test


for test in iterate_tests(unittest.defaultTestLoader.loadTestsFromModule(run_tests)):
    print(test.id().removeprefix("run_tests."))
