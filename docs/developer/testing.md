# Testing

Tests are standalone Qt Core console executables that return 0 on success.
They live in `tests/` and build out-of-source.

## Run

From the repository root, in an MSVC shell with Qt's `bin` and ArrayFire's
`lib` on `PATH` (for `qmake`, and so the test exes find their DLLs):

```bat
mkdir tests\build-tests
cd tests\build-tests
qmake ..\tests.pro -spec win32-msvc CONFIG+=debug
nmake check
```

Use the `build-tests\` directory name — it stays git-ignored via the `build-*`
rule in `.gitignore`.

## Add a test

1. Copy an existing test `.pro` and `.cpp` (e.g. `test_differentialevolution.*`).
2. Keep `CONFIG += testcase` and `TEST_TARGET_DIR = .` in the `.pro`
   (the latter lets `nmake check` launch the exe from the build directory).
3. Add the new `.pro` filename to `tests/tests.pro`.
4. Prefer Qt Core only; add GUI dependencies solely when unavoidable.
5. Tests that exercise ArrayFire code: `include(../pri/arrayfire.pri)` in the
   `.pro`, and list `Q_OBJECT` headers in `HEADERS` so moc runs.
6. For a regression whose failure mode is a hang, start a watchdog thread that
   exits non-zero on timeout, so the hang registers as a test failure.
