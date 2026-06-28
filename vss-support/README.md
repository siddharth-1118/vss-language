# VSS (Very Simple Syntax) Language

VSS is a highly readable, human-centric programming language designed to eliminate syntax noise (no curly braces, semicolons, or parentheses) and deliver absolute simplicity for both system automation and web development.

This repository contains the complete VSS v1.0.0 implementation, including a compiler frontend, dynamic runtime with reference counting, a tree-walk interpreter, a live development web server, and official IDE support extensions.

---

## Key Features

- **No Syntax Noise:** No `{}` or `;`. Indentation and simple keywords (`when`, `repeat`, `finish`) manage code blocks.
- **Dynamic Type System:** Supports numbers, booleans, strings, lists, maps, and task closures.
- **Memory Safety (ARC):** Automatic Reference Counting (ARC) handles garbage collection in C with zero memory leaks.
- **Error Handling (`attempt/rescue`):** Native try-catch mechanism with named error variables.
- **Embedded Webpage Delimiters:** Create Tailwind-powered responsive webpages natively using `hi htmvss` and `bye htmvss`.
- **Built-in Dev Server:** Run `vss --serve` to compile and launch `.htmvss` pages with dynamic hot-reloading.

---

## Getting Started

### 1. Requirements
- Linux or WSL (Ubuntu/Debian)
- GCC compiler and Make (`build-essential`)

### 2. Compilation
To build the VSS compiler executable:
```bash
cd vss
make
```

### 3. Running Examples
Run the built-in VSS programs:
```bash
./vss examples/hello.vss
./vss examples/conditions.vss
./vss examples/tasks.vss
```

---

## Web Development in VSS

VSS has native support for compiling and serving web templates. Create a file named `page.htmvss`:

```vss
hi htmvss
    say "    <div class='bg-slate-800 p-8 rounded-2xl shadow-2xl text-center'>"
    say "        <h1 class='text-4xl font-extrabold text-indigo-400'>Hello from VSS!</h1>"
    say "        <p class='text-slate-300 mt-2'>Rendered dynamically by the VSS Runtime.</p>"
    say "    </div>"
bye htmvss
```

Start the interactive dev server to automatically compile and launch this in your browser:
```bash
./vss --serve
```

---

## Editor Support (VS Code & Antigravity IDE)

The official extension provides syntax highlighting (including HTML inside VSS strings) and real-time error checking.

To install manually:
Copy the `vss-support` directory to your extensions folder:
- **VS Code:** `~/.vscode/extensions/`
- **Antigravity IDE:** `~/.antigravity-ide/extensions/`
