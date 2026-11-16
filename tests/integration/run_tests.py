# Copyright © 2026 CCP ehf.
"""Integration tests for ExeFile's interpreter mode.

Every case runs `<EXEFILE> /lib=<EXEFILE_LIB> /buildflavor=<EXEFILE_BUILD_FLAVOR> /py [options] <script> [args]`
with EXEFILE_INHERIT=1, and with the dynamic loader searching EXEFILE_LIBRARY_DIR.

The tests read these environment variables, which tests/integration/CMakeLists.txt sets for CTest:

EXEFILE               the ExeFile executable
EXEFILE_BUILD_FLAVOR  the /buildflavor value, which picks the Blue that ExeFile loads
EXEFILE_LIBRARY_DIR   the folder that holds Blue and ExeFile's shared libraries
EXEFILE_LIB           the /lib value: the standard library and the extension modules of the Python that ExeFile links
"""
import functools
import json
import os
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
PROBE_PY = HERE / "probe.py"
TRIGGER_BUFFER_OVERRUN_PY = HERE / "trigger_buffer_overrun.py"

# The variable that lists folders the dynamic loader searches for shared libraries, e.g. Blue.
LIBRARY_PATH_VARIABLE = "PATH" if sys.platform == "win32" else "DYLD_LIBRARY_PATH"


def get_required_variable(name: str) -> str:
    try:
        return os.environ[name]
    except KeyError:
        raise RuntimeError(f"{name} is not set; see this module's docstring") from None


@functools.cache
def get_exefile_path() -> Path:
    return Path(get_required_variable("EXEFILE")).resolve()


@functools.cache
def get_exefile_lib() -> str:
    return get_required_variable("EXEFILE_LIB")


@functools.cache
def get_exefile_build_flavor() -> str:
    return get_required_variable("EXEFILE_BUILD_FLAVOR")


@functools.cache
def get_exefile_library_dir() -> str:
    return get_required_variable("EXEFILE_LIBRARY_DIR")


def run_exefile(
    *python_args: str | os.PathLike,
    env: dict[str, str] | None = None,
    exe: Path | None = None,
    cwd: Path | None = None,
    timeout: float = 60,
) -> subprocess.CompletedProcess[str]:
    """Runs ExeFile in interpreter mode.

    Args:
        python_args: The arguments after /py.
        env: The process's environment, besides EXEFILE_INHERIT and the loader's library path. Nothing is inherited
            from this process.
        exe: The executable to run instead of EXEFILE.
        cwd: The working directory, by default this process's.
        timeout: Seconds to wait before giving up, since a Blue error dialog blocks until someone closes it. Windows
            sometimes takes 10 s to start an executable it has not run before, such as the copy a venv makes.
    """
    environment = {
        # Without it, ExeFile on Windows opens a console of its own instead of using the stdout and stderr it inherits.
        "EXEFILE_INHERIT": "1",
        LIBRARY_PATH_VARIABLE: get_exefile_library_dir(),
        **(env or {}),
    }
    command = [
        str(exe or get_exefile_path()),
        "/lib=" + get_exefile_lib(),
        "/buildflavor=" + get_exefile_build_flavor(),
        "/py",
        *map(str, python_args),
    ]
    result = subprocess.run(
        command,
        cwd=cwd,
        env=environment,
        capture_output=True,
        encoding="utf-8",
        errors="replace",
        timeout=timeout,
    )
    print(f"command: {result.args}\nexit code: {result.returncode}\nstdout:\n{result.stdout}\nstderr:\n{result.stderr}")
    return result


def read_probe_output(result: subprocess.CompletedProcess[str]) -> dict:
    """Fails the test unless ExeFile exited with 0, and returns the state probe.py printed."""
    if result.returncode != 0:
        raise AssertionError(f"ExeFile exited with {result.returncode}")
    return json.loads(result.stdout)


class InterpreterModeTests(unittest.TestCase):
    def test_runs_script_with_arguments(self):
        state = read_probe_output(run_exefile(PROBE_PY, "a", "b c"))
        self.assertEqual(state["argv"], [str(PROBE_PY), "a", "b c"])
        self.assertEqual(Path(state["path"][0]), PROBE_PY.parent)

    def test_maintains_exefile_configuration(self):
        # Where ExeFile's configuration departs from the vanilla interpreter's defaults: see ConfigurePython().
        state = read_probe_output(run_exefile(PROBE_PY))
        self.assertIn("d", state["warnoptions"])  # -Wd
        self.assertEqual(state["flags"]["no_user_site"], 1)

    def test_x_options_apply_at_pre_initialization(self):
        state = read_probe_output(run_exefile("-X", "utf8", "-X", "dev", PROBE_PY))
        self.assertEqual(state["flags"]["utf8_mode"], 1)
        self.assertTrue(state["flags"]["dev_mode"])
        self.assertEqual(state["xoptions"], {"utf8": True, "dev": True})

    def test_environment_applies_at_pre_initialization(self):
        state = read_probe_output(run_exefile(PROBE_PY, env={"PYTHONUTF8": "1", "PYTHONDEVMODE": "1"}))
        self.assertEqual(state["flags"]["utf8_mode"], 1)
        self.assertTrue(state["flags"]["dev_mode"])

    def test_ignore_environment_applies_at_pre_initialization(self):
        state = read_probe_output(run_exefile("-E", PROBE_PY, env={"PYTHONUTF8": "1", "PYTHONDEVMODE": "1"}))
        self.assertEqual(state["flags"]["ignore_environment"], 1)
        self.assertEqual(state["flags"]["utf8_mode"], 0)
        self.assertFalse(state["flags"]["dev_mode"])

    def test_isolated_mode_ignores_environment(self):
        state = read_probe_output(run_exefile("-I", PROBE_PY, env={"PYTHONUTF8": "1"}))
        self.assertEqual(state["flags"]["isolated"], 1)
        self.assertEqual(state["flags"]["utf8_mode"], 0)

    def test_detects_venv(self):
        exefile = get_exefile_path()
        with tempfile.TemporaryDirectory(prefix="exefile-test-venv-") as venv:
            venv = Path(venv)
            self.assertEqual(run_exefile("-m", "venv", "--without-pip", venv).returncode, 0)
            venv_exefile, = venv.glob(f"*/{exefile.name}")
            site_packages, = venv.glob("**/site-packages")
            installed_probe = site_packages / "installed_probe.py"
            shutil.copy(PROBE_PY, installed_probe)
            state = read_probe_output(run_exefile("-m", installed_probe.stem, exe=venv_exefile))
        self.assertEqual(Path(state["prefix"]), venv)
        self.assertNotEqual(Path(state["base_prefix"]), venv)
        self.assertEqual(Path(state["argv"][0]), installed_probe)


class DevModeTests(unittest.TestCase):
    def test_reports_buffer_overrun(self):
        result = run_exefile("-X", "dev", TRIGGER_BUFFER_OVERRUN_PY)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Fatal Python error", result.stderr)
        self.assertIn("bad trailing pad byte", result.stderr)


class NonAsciiTests(unittest.TestCase):
    # ExeFile narrows the interpreter's arguments with CW2A, which does not produce the UTF-8 that CPython decodes them
    # as. Drop expectedFailure once that is fixed; an unexpected success fails the run.

    @unittest.expectedFailure
    def test_non_ascii_arguments(self):
        arguments = ["\u00de\u00f3rsm\u00f6rk", "\u65e5\u672c\u8a9e"]
        state = read_probe_output(run_exefile(PROBE_PY, *arguments))
        self.assertEqual(state["argv"][1:], arguments)

    @unittest.expectedFailure
    def test_non_ascii_script_path(self):
        with tempfile.TemporaryDirectory(prefix="exefile-test-") as root:
            script = Path(root) / "\u00de\u00f3r" / PROBE_PY.name
            script.parent.mkdir()
            shutil.copy(PROBE_PY, script)
            state = read_probe_output(run_exefile(script))
        self.assertEqual(state["argv"], [str(script)])


if __name__ == "__main__":
    unittest.main(verbosity=2, buffer=True)
