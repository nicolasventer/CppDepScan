## Planned Features

### Allowed rules (`-A`, `--allowed`)

- Change option usage to: `-A<from>=<to_1>,<to_2>,...`
- Add support of `$folder` in `-A` / `--allowed` that corresponds to the folder of the source (useful when the source is defined with a glob)

### Output formats

- Add option `-stdout=<format_1>,<format_2>,...` (supported formats: `d2`, `json`; default: `d2`)
- Add option `-stderr` (same values as `-stdout`)
- Maybe add output format `csv`

### Config file support

- Add support for config files: `.config.json` and `.config.d2` (default config path is read automatically, but can be specified with `-c`, `--config`)
- Note: command line arguments and config files are additive
- Also support dumping the effective command line with `-o myCmd.config.json` (extension can be `.config.json` or `.config.d2`)

### Forbidden behavior

- Maybe add option `forceForbidden`, i.e. -A<from>=!<to>

### Command override

- Add option `--override`
- Example: `--override: <exe> arg_of_type_1 arg_of_type_2 --override new_arg_of_type_2`
- Behavior: execute the command with `arg_of_type_1` and the overridden `new_arg_of_type_2`
