## Planned Features

### Allowed rules (`-A`, `--allowed`)

- Add support of `$folder` in `-A` / `--allowed` that corresponds to the folder of the source (useful when the source is defined with a glob)

### Forbidden behavior

- Add option `forceForbidden`, i.e. -A <from> !<to>

### Text format error

- Add option `-stderr`

example:

```
3 errors:
- unspecified file: 'sample/src/legacy.c'
- unresolved include: 'sample/src/main.cpp' failed to include 'nonexistent.h'
- forbidden includes: 'sample/src/main.cpp' cannot include 'sample/include/extra.hpp', it can only include 'sample/src'
```

### Config file support

- Add support for config files: `.config.json` and `.config.d2` (default config path is read automatically, but can be specified with `-c`, `--config`)
- Note: command line arguments and config files are additive
- Also support dumping the effective command line with `-o myCmd.config.json` (extension can be `.config.json` or `.config.d2`)

### Command override

- Add option `--override`
- Example: `--override: <exe> arg_of_type_1 arg_of_type_2 --override new_arg_of_type_2`
- Behavior: execute the command with `arg_of_type_1` and the overridden `new_arg_of_type_2`
