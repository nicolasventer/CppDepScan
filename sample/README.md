# CppDepScan sample project

This folder contains a small C/C++ tree used to demonstrate every CppDepScan option.

**Running from sample:** Run commands from **inside this directory** (`cd sample` from the repo root), so that the scan path is `.` and paths like `src`, `include`, `build` work as in the examples below.

**Running from repo root:** You can also run from the repo root using paths like `sample`, `sample/src`, `sample/include`. The script `exec_sample.bat` (in the repo root) runs all demo variants and writes D2 files to `result/`; you can use it as a reference for path and option usage.

## Layout

| Path                | Purpose                                                                                                         |
| ------------------- | --------------------------------------------------------------------------------------------------------------- |
| `src/main.cpp`      | Includes allowed (app, extra), forbidden (lib), unresolved (`nonexistent.h`), and `<iostream>`                  |
| `src/app/`          | App code; used in **-A** (allowed) rules                                                                        |
| `src/lib/`          | Library; includes here can be **forbidden** when using **-A**                                                   |
| `src/legacy.c`      | C source (shows `.c` scanning)                                                                                  |
| `include/extra.hpp` | Header found via **-I include**                                                                                 |
| `build/junk.cpp`    | Excluded by **-e build**                                                                                        |
| `external/skip.cpp` | Excluded by **-e external**                                                                                     |
| `external/keep/`    | Force-included with **-e !external/keep** despite **-e external** (add `-e !external/keep` to scan these files) |

---

## Option examples

### Scan paths and exclude

**Basic scan (D2 to stdout):**

```bash
./CppDepScan -I src -I include -e build -e external .
```

**Exclude `build` and `external`; force-include `external/keep`:**

```bash
./CppDepScan -I src -I include -e build -e external -e !external/keep .
```

Without `-e !external/keep`, `external/keep/*.cpp` is not scanned.

**Multiple scan paths:**

```bash
./CppDepScan -I src -I include -e build src include
```

---

### Resolution: include paths and allowed rules

**Include paths (`-I`)** — resolve `#include "..."` and `#include <...>`:

```bash
./CppDepScan -I src -I include -e build -e external .
```

`main.cpp` includes `"extra.hpp"`; it resolves to `include/extra.hpp` via `-I include`.

**Allowed rules (`-A`, `--allowed`)** — declare that `<from>` may include `<to>`. Any include that resolves outside the allowed `<to>` list becomes **forbidden**.

Example: allow only `src/app` and `include` for files under `src/main` (e.g. `main.cpp`). Then includes of `src/lib/*` are forbidden:

```bash
./CppDepScan -I src -I include -e build -e external -A src/main src/app -A src/main include .
```

Output will show **allowed** (e.g. `app/app.hpp`, `extra.hpp`) and **forbidden** (e.g. `lib/public.hpp`, `lib/private.hpp`).

**Multiple allowed rules (e.g. app may include app and include):**

```bash
./CppDepScan -I src -I include -e build -e external -A src/main src/app -A src/main include -A src/app src/app -A src/app include .
```

---

### Output: files, format, grouping, standard library

**JSON to file:**

```bash
./CppDepScan -I src -I include -e build -e external -o out.json .
```

**D2 to file (extension other than `.json`):**

```bash
./CppDepScan -I src -I include -e build -e external -o graph.d2 .
```

**Both JSON and D2:**

```bash
./CppDepScan -I src -I include -e build -e external -o out.json -o graph.d2 .
```

**JSON to stdout (default stdout is D2):**

```bash
./CppDepScan -I src -I include -e build -e external --json .
```

**Include standard library headers in output (`--std`):**

```bash
./CppDepScan -I src -I include -e build -e external --std -o with_std.json .
```

Without `--std`, `<iostream>` (and other std headers) are resolved but omitted from the report.

**Grouping by path (`-g`, `--group`):**

```bash
./CppDepScan -I src -I include -e build -e external -g src -g include -o grouped.d2 .
```

Files are grouped by the given paths in the output (e.g. D2 nodes by group). Groups are not applied to forbidden or unresolved includes (they stay as separate edges/nodes).

---

### Special cases

- **Exclude unresolved path:** Using `-e path/to/nonexistent.h` excludes a path that is never resolved (e.g. a missing header). Possible but not recommended; prefer resolving with `-I` and excluding the resolved path.
- **Exclude resolved path:** Use `-I include -e include/extra.hpp` so `extra.hpp` is resolved first, then excluded from the scan. Cleaner than excluding the unresolved form.
- **Group ignored:** When using `-A` (allowed rules), forbidden or unresolved includes are not folded into the requested groups in the D2 output.

---

### Full example (all options)

```bash
./CppDepScan \
  -I src -I include \
  -e build -e external -e !external/keep \
  -A src/main src/app -A src/main include \
  -A src/app src/app -A src/app include \
  -g src -g include \
  --std \
  -o full.json -o full.d2 \
  .
```

- **Scan:** `.`, excluding `build` and `external`, force-including `external/keep`.
- **Resolve:** `-I src`, `-I include`; **allowed:** `src/main` → `src/app` + `include`, `src/app` → same.
- **Output:** grouped by `src` and `include`, std headers kept, JSON and D2 written to `full.json` and `full.d2`.

---

### Help

```bash
./CppDepScan -h
# or
./CppDepScan --help
```

### Generating diagrams from D2

D2 files (e.g. `graph.d2`) can be rendered to PNG with the [D2](https://d2lang.com/) tool:

```bash
d2 graph.d2 graph.png
```

On Windows, run from `sample` and use `CppDepScan.exe` instead of `./CppDepScan` (e.g. `CppDepScan.exe -I src -I include -e build -e external .`). From the repo root, `exec_sample.bat` runs the sample scans and then renders all `result/*.d2` to `result/*.png`.
