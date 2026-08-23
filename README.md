<div align="center">

<img src="https://img.shields.io/badge/VSS-Programming%20Language-blue?style=for-the-badge&logo=code&logoColor=white" />
<img src="https://img.shields.io/github/v/release/siddharth-1118/vss-language?style=for-the-badge&color=green" />
<img src="https://img.shields.io/github/stars/siddharth-1118/vss-language?style=for-the-badge&color=yellow" />
<img src="https://img.shields.io/github/license/siddharth-1118/vss-language?style=for-the-badge" />

# 🚀 VSS Programming Language

**A modern, simple, and powerful programming language with native web server support**

[📦 Install](#-installation) · [📖 Docs](#-language-guide) · [🌐 Web Server](#-web-server) · [💬 Discussions](https://github.com/siddharth-1118/vss-language/discussions)

</div>

---

## ✨ What is VSS?

VSS is a clean, readable programming language designed to be **simple yet powerful**. It features:

- 🌐 **Native HTTP web server** — build websites with just a few lines
- 🗄️ **Built-in SQLite database** — no external setup needed
- 📁 **File system operations** — read, write, list files easily
- 🔐 **Crypto functions** — MD5, SHA-256 hashing built-in
- 🌍 **HTTP client** — call any REST API
- 📦 **JSON, XML, CSV** support out of the box
- 🧮 **Math library** — sin, cos, sqrt, pow and more

---

## 📦 Installation

### Windows
Download `VSS-3.0.0-Setup.exe` from [Releases](https://github.com/siddharth-1118/vss-language/releases/latest)

Or install via **winget**:
```bash
winget install VSS.VSS
```

### Linux
```bash
wget https://github.com/siddharth-1118/vss-language/releases/latest/download/vss-linux-x64.tar.gz
tar -xzf vss-linux-x64.tar.gz
sudo mv vss /usr/local/bin/
```

### macOS
```bash
curl -L https://github.com/siddharth-1118/vss-language/releases/latest/download/vss-macos-arm64.tar.gz | tar xz
sudo mv vss /usr/local/bin/
```

---

## 🌐 Web Server

Build a full website in VSS — no frameworks needed!

```vss
grab web

web.route("/", { req ->
    send "<!DOCTYPE html><html><body><h1>Hello from VSS!</h1></body></html>"
})

web.route("/about", { req ->
    send "<!DOCTYPE html><html><body><h1>About VSS</h1></body></html>"
})

say "Server running at http://localhost:8080"
web.serve(8080)
```

---

## 📖 Language Guide

### Variables & Output
```vss
let name = "VSS"
let version = 3.0
say "Welcome to " + name + " v" + version
```

### Functions (Tasks)
```vss
task greet needs name
    say "Hello, " + name + "!"
finish

greet("World")
```

### Conditions
```vss
let score = 95

check score >= 90 then
    say "A grade!"
else check score >= 80 then
    say "B grade!"
else
    say "Keep studying!"
finish
```

### Loops
```vss
repeat 5 times
    say "VSS is awesome!"
finish

let i = 0
loop while i < 10
    say i
    let i = i + 1
finish
```

### Closures
```vss
let double = { x -> x * 2 }
say double(21)   note → 42
```

### Namespaces
```vss
namespace utils
    task add needs a, b
        send a + b
    finish
finish

say utils_add(3, 4)   note → 7
```

---

## 📚 Standard Library

| Module | Functions |
|--------|-----------|
| `web` | `route(path, handler)`, `serve(port)` |
| `math` | `sin`, `cos`, `sqrt`, `pow`, `log`, `ceil`, `floor` |
| `string` | `upper`, `lower`, `length`, `trim`, `split`, `join`, `replace` |
| `filesystem` | `read`, `write`, `exists`, `list` |
| `json` | `parse`, `stringify`, `read`, `write` |
| `database` | `open`, `execute`, `query` |
| `http` | `request(method, url)` |
| `crypto` | `md5`, `sha256` |
| `testing` | `assert` |

---

## 🗄️ Database Example

```vss
grab database

let db = database.open("myapp.db")
database.execute(db, "CREATE TABLE IF NOT EXISTS todos (id INTEGER, task TEXT)")
database.execute(db, "INSERT INTO todos VALUES (1, 'Learn VSS')")

let todos = database.query(db, "SELECT * FROM todos")
say todos
```

---

## 🧪 Testing

```vss
grab testing

task add needs a, b
    send a + b
finish

testing.assert(add(2, 3) == 5, "addition works")
testing.assert(add(0, 0) == 0, "zero works")
say "All tests passed!"
```

---

## 🔐 Crypto

```vss
grab crypto

let hash = crypto.sha256("hello world")
say hash
```

---

## 📁 File System

```vss
grab filesystem

filesystem.write("notes.txt", "Hello VSS!")
let content = filesystem.read("notes.txt")
say content

let files = filesystem.list(".")
say files
```

---

## 💬 Community

- 💬 [GitHub Discussions](https://github.com/siddharth-1118/vss-language/discussions) — Ask questions, share ideas
- 🐛 [Issues](https://github.com/siddharth-1118/vss-language/issues) — Report bugs
- 🔀 [Pull Requests](https://github.com/siddharth-1118/vss-language/pulls) — Contribute

---

## 📄 License

MIT License — free to use, modify, and distribute.

---

<div align="center">

**If VSS helped you, please ⭐ star this repo!**

Made with ❤️ by [siddharth-1118](https://github.com/siddharth-1118)

</div>
