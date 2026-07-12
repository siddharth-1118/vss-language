# Changelog

All notable changes to the VSS Programming Language are documented here.

---

## [2.1.0] - 2026-07-12

### Added
- **Standard Library v2.1**: 11 built-in modules — `math`, `string`, `file`, `json`, `http`, `time`, `random`, `system`, `database`, `crypto`, `network`
- **Native C backends** for all standard library modules (`builtins.c`, `json.c`)
- **JSON parser/serializer** — native bidirectional conversion between JSON text and VSS values
- **File I/O module** — `file.read`, `file.write`, `file.exists`, `file.append`, `file.delete`, `file.list`
- **Cryptography module** — MD5, SHA1, SHA256 hash functions via `crypto.md5`, `crypto.sha1`, `crypto.sha256`
- **HTTP client module** — `http.get`, `http.post`, `http.put`, `http.delete` using system `curl`
- **Database module** — SQLite3 dynamic-load backend with JSON-file mock fallback
- **Network module** — `network.ping`, `network.resolve`, `network.open_connection`
- **Directory listing** — `file.list(path)` for cross-platform directory enumeration
- **Math module** — `sin`, `cos`, `tan`, `sqrt`, `log`, `pow`, `ceil`, `floor`
- **String module** — `lower`, `upper`, `trim`, `split`, `join`, `replace`, `contains`, `starts_with`, `ends_with`, `length`
- **Time module** — `time.now`, `time.format`
- **Random module** — `random.number`, `random.int`, `random.choice`
- **System module** — `system.platform`, `system.env`, `system.args`, `system.exit`, `system.run`

### Fixed
- **VM re-entrancy crash**: Nested `grab` sub-VM no longer clears global module cache, resolving exit-code-1 crashes on any `grab` statement
- **String interpolation JSON conflict**: Strings containing `{` (JSON payloads) no longer get mis-parsed as interpolated expression segments
- **Keyword-as-identifier in member access**: Reserved words like `read`, `write`, `add`, `erase`, `size` are now permitted as field names after `.`
- **Parser error recovery**: Added synchronization to continue parsing after syntax errors
- **Colored diagnostics**: ANSI color output and caret-pointing source snippets in compile errors

### Changed
- Module namespace export: imported modules expose both a namespace map (e.g. `file`) and flat individual bindings
- `grab` now supports modules from the `packages/` directory alongside `stdlib/`

---

## [1.0.0] - 2024

### Added
- Initial release of VSS (Very Simple Syntax) Programming Language
- Bytecode VM engine with automatic reference counting
- Full CLI: `run`, `build`, `new`, `init`, `test`, `format`, `lint`, `docs`, `clean`, `doctor`, `version`, `help`
- Package manager: `vss package install`, `remove`, `update`, `publish`
- Cross-platform platform abstraction layer (Windows, Linux, macOS)
- Project-wide `VSS_` namespace to prevent SDK conflicts
- Standard library: `stdlib/math.vss`
- Web server mode with `.htmvss` template support
- `grab` module system for importing packages
- GitHub Actions CI/CD release pipeline
- Packages hosted at `github.com/siddharth-1118/vss-language/packages/`
- Installers: `install.sh` (Linux/macOS), `install.ps1` (Windows)

### Language Features
- Variables: `make`, `keep` (constant), `becomes`
- Control flow: `when`/`orwhen`/`otherwise`/`finish`, `choose`/`case`
- Loops: `repeat N times`, `repeat i through 1 to N`, `repeat each x in list`, `during`
- Functions: `task`/`needs`/`send`/`finish`
- Error handling: `attempt`/`rescue`/`finish`
- Collections: lists `[...]`, maps `map [key: value]`
- File I/O: `read`, `write`, `add`, `erase`, `exists`
- Module system: `grab <module>`
