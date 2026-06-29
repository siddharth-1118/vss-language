# Changelog

All notable changes to the VSS Programming Language are documented here.

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
