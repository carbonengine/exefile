# Copyright © 2026 CCP ehf.
"""Prints the interpreter state the integration tests assert on, as JSON, to stdout."""
import json
import os
import sys

state = {
    "argv": sys.argv,
    "executable": sys.executable,
    "prefix": sys.prefix,
    "base_prefix": sys.base_prefix,
    "path": sys.path,
    "cwd": os.getcwd(),
    "flags": {
        "utf8_mode": sys.flags.utf8_mode,
        "dev_mode": sys.flags.dev_mode,
        "ignore_environment": sys.flags.ignore_environment,
        "isolated": sys.flags.isolated,
        "no_user_site": sys.flags.no_user_site,
    },
    "xoptions": sys._xoptions,
    "warnoptions": sys.warnoptions,
}

# json.dumps escapes non-ASCII text, so the output survives whatever encoding stdout has.
print(json.dumps(state))
