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

| Option         | Description                                                                |
| -------------- | -------------------------------------------------------------------------- |
| `-I <path>`    | Add include path for resolution (folder or file). Like compiler `-I`.      |
| `-e <path>`    | Exclude path (folder or file); paths under this are skipped.               |
| `--stdlib`     | Consider standard library when resolving `<...>` includes.                 |
| `--json`       | Output JSON.                                                               |
| `--d2`         | Output D2 lang (default if no format given).                               |
| `-o <file>`    | Write output to file. With both `--json` and `--d2`, `-o` applies to JSON. |
| `-h`, `--help` | Print help.                                                                |

- **Scan paths**: list of files or directories to scan for C/C++ sources (`.c`, `.cpp`, `.cc`, `.cxx`, `.h`, `.hpp`, `.hxx`, `.hh`, `.h++`).
- **Include paths**: used to resolve `#include "..."` and `#include <...>` (current file’s directory is always tried first for `"..."`).
- **Exclude paths**: any file or directory under these is not scanned.

## Output

### JSON

- `includeList`: array of `{ "from", "to" }` (from/to as dotted paths; `to` is an array of resolved includes).
- `unresolvedIncludeList`: array of `{ "from", "to" }` for includes that could not be resolved.

### D2

- **# include list**: edges `from -> to` for resolved includes (dotted paths).
- **# unresolved include list**: edges for includes that could not be resolved.

## Example

<!-- TODO: test readme example command -->

```bash
./CppDepScan -Isrc -Isrc/include -e build -e external --json --d2 -o out.json src
# D2 goes to stdout; JSON to out.json
```

```bash
./CppDepScan -Isrc --d2 -o graph.d2 src
# Only D2, written to graph.d2
```
