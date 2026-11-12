# Carbon ExeFile
> Elements used to build the final exefile executable

## 🛠️ Building

Building can be done using the usual CMake work flow.

## 🧪 Testing

`tests/integration` holds integration tests for interpreter mode (`/py`). `BUILD_TESTING=ON` registers them with
CTest.

ExeFile needs [Blue](https://github.com/carbonengine/blue) to run, so `BUILD_TESTING=ON` also builds Blue through
vcpkg for the tests. On Windows:

```
cmake --preset x64-windows-release -DBUILD_TESTING=ON
cmake --build .cmake-build-x64-windows-release --config Release
ctest --test-dir .cmake-build-x64-windows-release -C Release --output-on-failure
```

`run_tests.py` is a plain unittest module configured through the environment variables its docstring describes, which
`ctest -V` prints. With them set, run single cases with the build's Python, for example
`python tests/integration/run_tests.py InterpreterModeTests.test_detects_venv`.

## 🤝 Contributing
Contribution follows the standard GIT PR model.

By submitting a pull request or otherwise contributing to this project, you agree to license your contribution under the MIT License, and you confirm that you have the right to do so.

## 📄 License and Legal Notices

© 2026 CCP Games 

This software is provided by CCP Games and does not include or distribute any third-party libraries or frameworks. 

This software contains elements used to build the final exefile executable

Trademark Notice: CCP Games is a trademark of CCP ehf. 

This project is licensed under the [MIT License](LICENSE.md). Nothing in the [MIT License](LICENSE.md) grants any rights to CCP Games' trademarks or game content.
