# Build guide - Moderated LMS v1.0

## 1. Requirements
| Item | Version |
|---|---|
| C++17 compiler | GCC 9+, Clang 10+ or MSVC 2019+ |
| CMake | 3.14+ |
| SQLite 3 (development files) | any 3.x |
| libsodium (development files) | 1.0.18+ |
| Git | any |

Ubuntu/Debian: `sudo apt install build-essential cmake libsqlite3-dev libsodium-dev git`
Windows: install Visual Studio Build Tools, then `vcpkg install sqlite3 libsodium` and pass `-DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake` to CMake.

## 2. Build
```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```
Produces `build/lms` (application), `build/lms_seed` (demo data) and `build/lms_tests`.
GoogleTest is used if installed, otherwise downloaded automatically (needs internet for the first configure only). Use `-DLMS_BUILD_TESTS=OFF` to skip tests.

## 3. Run the tests and coverage
```
ctest --test-dir build --output-on-failure
# coverage (GCC/Clang):
cmake -S . -B build-cov -DCMAKE_BUILD_TYPE=Debug -DLMS_COVERAGE=ON
cmake --build build-cov -j && ctest --test-dir build-cov
gcovr -r . build-cov --filter 'src/.*Service\.cpp' --print-summary
```

## 4. Run the application
```
./build/lms --db lms.db --storage submissions --log lms.log
```
On the first run the program asks you to create the administrator account (there is no default password). Then log in as the administrator and create departments, courses, users, lecturer assignments and enrolments.

Try it with demo data (800 students, every demo account has password `Demo1234`; demo only):
```
./build/lms_seed --db demo.db --students 800
./build/lms --db demo.db
# accounts: admin, lecturer, hod, student1 ... student800
```

## 5. Backup and recovery (NFR-06)
Close the program, then copy `lms.db` and the `submissions/` folder to a safe location (for example a USB drive) daily. To restore, replace both with the backup copies.

## 6. Command-line options
`--db FILE` database path (default `lms.db`), `--storage DIR` folder for submitted files (default `submissions`), `--log FILE` error log (default `lms.log`).
