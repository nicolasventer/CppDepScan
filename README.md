# CppDepScan

C++ include detection: scan paths, resolve `#include` directives, output include graph as JSON and/or D2.

## Build

```bash
./build.sh
```

Produces the `CppDepScan` executable. Requires a C++17 compiler (e.g. g++ with `-std=c++17`). On Windows you can use Git Bash, WSL, or:

```bash
g++ -std=c++17 -O2 -o CppDepScan.exe CppDepScan.cpp
```

## Usage

```
CppDepScan [options] <scan_path> [scan_path ...]
```

| Option         | Description                                                                                                                         |
| -------------- | ----------------------------------------------------------------------------------------------------------------------------------- |
| `-I <path>`    | Add include path for resolution (folder or file). Like compiler `-I`.                                                               |
| `-e <path>`    | Exclude path (folder or file); paths under this are not scanned. Use `-e !<path>` to force-include (scan even if under an exclude). |
| `--stdlib`     | Include standard library headers in the output (default: false).                                                                    |
| `--json`       | Output JSON. Default format when neither `--json` nor `-o` implies format is D2.                                                    |
| `-o <file>`    | Write output to file (default: stdout). May be repeated. Format: `.json` → JSON, otherwise D2.                                      |
| `-h`, `--help` | Print help.                                                                                                                         |

- **Scan paths**: files or directories to scan for C/C++ sources (`.c`, `.cc`, `.cpp`, `.cxx`, `.h`, `.hpp`, `.hxx`, `.hh`, `.h++`).
- **Include paths**: used to resolve `#include "..."` and `#include <...>` (current file’s directory is always tried first for `"..."`).
- **Exclude paths**: any file or directory under these is not scanned. Prefix with `!` to force-include that path even if it lies under an exclude.

## Output

### JSON

- `specifiedIncludeList`: array of objects with `"from"` (dotted path), `"allowedToList"`, `"forbiddenToList"`, `"unresolvedToList"` (arrays of dotted paths or raw include names).
- `unspecifiedIncludeList`: same shape (currently unused).

### D2

- **# specified include list** / **# unspecified include list**: for each file, **# allowed:** edges `from -> to`, **# forbidden:** edges, **# unresolved:** commented-out edges for unresolved includes.

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
