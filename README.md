# CppDepScan

## Description

C++ include detection: scan paths, resolve `#include` directives, and output the include graph as JSON and/or [D2](https://d2lang.com/).

## Features

- Scan C/C++ sources (`.c`, `.cc`, `.cpp`, `.cxx`, `.h`, `.hpp`, `.hxx`, `.hh`, `.h++`)
- Resolve `#include "..."` and `#include <...>` with configurable include paths
- Exclude paths from scanning; force-include with `-e !<path>`
- Declare allowed includes (`-A`) for dependency rules
- Output as **JSON** (include maps with allowed/forbidden/unresolved sets) or **D2** (include list with edges)
- Optional grouping by path (`-g`) and optional standard library headers in output (`--std`)

## Examples

```bash
# D2 to stdout (default)
./CppDepScan -I src -I src/include -e build -e external src
```

```bash
# JSON to file
./CppDepScan -I src -e build --json -o out.json src
```

```bash
# D2 to file (extension other than .json)
./CppDepScan -I src -o graph.d2 src
```

```bash
# Both JSON and D2 via multiple -o (format by extension)
./CppDepScan -I src -e build -o out.json -o graph.d2 src
```

```bash
# D2 with grouping (-g)
./CppDepScan -I src -e build -g src -o graph.d2 src
```

On Windows with `CppDepScan.exe`, use the same options (e.g. `CppDepScan.exe -I src -e build -o out.json src`).

## Usage

```
CppDepScan [options] <scan_path> [scan_path ...]
```

**Scan paths**

| Option        | Description                                                                 |
| ------------- | --------------------------------------------------------------------------- |
| `-e <path>`   | Exclude path for scanning (folder or file); may be repeated.                |
| `-e !<path>`  | Keep path for scanning even if it lies under an exclude.                    |

- **Scan paths** (positional): files or directories to scan for C/C++ sources (`.c`, `.cc`, `.cpp`, `.cxx`, `.h`, `.hpp`, `.hxx`, `.hh`, `.h++`).
- **Exclude** (`-e`): paths under an excluded folder/file are not scanned. Use `-e !<path>` to force-include a path that would otherwise be excluded.

**Resolution**

| Option                        | Description                                                                 |
| ----------------------------- | --------------------------------------------------------------------------- |
| `-I <path>`                   | Add include path for resolution (folder or file); may be repeated.          |
| `-A`, `--allowed <from> <to>` | \<from\> may include \<to\>; may be repeated.                               |

- **Include paths** (`-I`): used to resolve `#include "..."` and `#include <...>`. The current file's directory is always tried first for `"..."`.
- **Allowed** (`-A`): declare that \<from\> may include \<to\>; used for dependency rules and reflected in D2/JSON output.

**Output**

| Option                 | Description                                                                 |
| ---------------------- | --------------------------------------------------------------------------- |
| `-o <file>`            | Write output to file; may be repeated. `.json` → JSON, otherwise D2.        |
| `--json`               | Use JSON for stdout (default when no `-o`: D2).                             |
| `--std`                | Include standard library headers in output (default: off).                  |
| `-g`, `--group <path>` | Gather files by group; path may be repeated.                                |

- **JSON**: `specifiedIncludeMap` — object mapping each source file (dotted path) to an object with `allowedSet`, `forbiddenSet`, `unresolvedSet`; `unspecifiedIncludeMap` — same shape (e.g. headers only).
- **D2**: **# specified include list** / **# unspecified include list** — for each file, **# allowed:** edges `from -> to`, **# forbidden:** edges, **# unresolved:** commented-out edges for unresolved includes.

**Other**

| Option         | Description  |
| -------------- | ------------ |
| `-h`, `--help` | Print help.  |

## Build

### Requirement

- C++17 compiler (e.g. GCC/Clang with `-std:c++17`, or MSVC with `/std:c++17`)

**Windows (cmd):**

```batch
build.bat
```

**Windows (manual), Git Bash, or WSL:**

```bash
g++ -std=c++17 -O2 -o CppDepScan.exe CppDepScan.cpp
```

**Linux / macOS:**

```bash
./build.sh
```

This produces `CppDepScan` (or `CppDepScan.exe` on Windows).

## License

MIT License. See [LICENSE](LICENSE).
