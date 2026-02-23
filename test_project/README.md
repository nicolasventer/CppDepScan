# CppDepScan test project

Small C++ project used to exercise CppDepScan.

## Layout

- `src/` — sources: `main.cpp`, `foo.cpp`, `foo.hpp`, `bar.hpp`
- `include/` — extra include path: `common.hpp`, `extra.hpp` (included as `<common.hpp>`, `<extra.hpp>`)
- `build/` — build output (excluded from scan in examples)

Include chain: `main.cpp` → `foo.hpp`, `bar.hpp`; `bar.hpp` → `foo.hpp`, `<string>`; `foo.hpp` → `<common.hpp>`, `<extra.hpp>`.

## Quick test (Windows)

From the **CppDepScan** repo root, after building with `build.bat`:

```powershell
.\CppDepScan.exe -I test_project\include -e test_project\build test_project
```

## Running CppDepScan

From the **CppDepScan** repo root.

**Windows (PowerShell):**

```powershell
# D2 to stdout (exclude build, add include path)
.\CppDepScan.exe -I test_project\include -e test_project\build test_project

# JSON to file
.\CppDepScan.exe -I test_project\include -e test_project\build --json -o test_project\deps.json test_project

# D2 to file
.\CppDepScan.exe -I test_project\include -e test_project\build -o test_project\deps.d2 test_project
```

**Unix / Git Bash / WSL:**

```bash
# D2 to stdout
./CppDepScan -I test_project/include -e test_project/build test_project

# JSON to file
./CppDepScan -I test_project/include -e test_project/build --json -o test_project/deps.json test_project

# D2 to file
./CppDepScan -I test_project/include -e test_project/build -o test_project/deps.d2 test_project
```

## Testing `-A` / `--allowed` (allowedIncludeListMap)

`foo.hpp` includes both `<common.hpp>` and `<extra.hpp>` from the `-I` path. Use `-A <from> <to>` to declare that file `<from>` may only include `<to>` (repeat for multiple). Any include from `-I` that is not in the list is reported as **forbidden**.

The `<from>` path must match how the scanner sees the file (path prefix). On Windows use backslashes in `-A` paths; on Unix use forward slashes.

**No allow list — all allowed:**

```powershell
.\CppDepScan.exe -I test_project\include -e test_project\build --json test_project
```

**Only `common.hpp` allowed from `foo.hpp` — `extra.hpp` forbidden (Windows):**

```powershell
.\CppDepScan.exe -I test_project\include -e test_project\build -A test_project\src\foo.hpp test_project\include\common.hpp --json test_project
```

Check JSON: `specifiedIncludeList` entry for `foo.hpp` should have `common.hpp` in `allowedToList` and `extra.hpp` in `forbiddenToList`.

**Both allowed from `foo.hpp` (Windows):**

```powershell
.\CppDepScan.exe -I test_project\include -e test_project\build -A test_project\src\foo.hpp test_project\include\common.hpp -A test_project\src\foo.hpp test_project\include\extra.hpp --json test_project
```

**Same on Unix / Git Bash / WSL** (use forward slashes in `-A`):

```bash
# Only common allowed → extra forbidden
./CppDepScan -I test_project/include -e test_project/build -A test_project/src/foo.hpp test_project/include/common.hpp --json test_project

# Both allowed
./CppDepScan -I test_project/include -e test_project/build -A test_project/src/foo.hpp test_project/include/common.hpp -A test_project/src/foo.hpp test_project/include/extra.hpp --json test_project
```

## Building the test project

Requires a C++17 compiler. From repo root:

**Windows:**

```powershell
g++ -std=c++17 -I test_project\include -o test_project\build\demo.exe test_project\src\main.cpp test_project\src\foo.cpp
.\test_project\build\demo.exe
```

**Unix / Git Bash:**

```bash
g++ -std=c++17 -I test_project/include -o test_project/build/demo test_project/src/main.cpp test_project/src/foo.cpp
./test_project/build/demo
```

Expected output: `version 1`
