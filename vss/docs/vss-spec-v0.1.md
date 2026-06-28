# VSS Language Specification v0.1

## 1. Purpose

VSS means **Very Simple Syntax**. It is a new programming language designed to be easy to read, easy to learn, and practical for real software.

VSS is built on these principles:

- simplicity over complexity
- readability over symbols
- fewer keywords
- less typing
- beginner friendliness
- safe defaults
- fast execution
- easy debugging
- original syntax

VSS is **not** a clone of Python, C, Java, JavaScript, Go, Rust, Ruby, Lua, or any other existing language. Its syntax is intentionally designed to be line-based, word-guided, and block-structured with explicit endings.

This document defines **VSS v0.1**, the first formal specification draft.

---

## 2. Design Goals

VSS must:

- let beginners read code like instructions
- support scripts and large applications
- avoid mandatory braces and semicolons
- avoid class-heavy object design
- support automatic memory management
- provide strong standard libraries
- support future bytecode and native compilation

VSS should eventually support:

- AI applications
- machine learning
- deep learning
- web apps
- desktop apps
- mobile apps
- games
- APIs
- automation
- robotics
- IoT
- data science

These capabilities belong mostly to official libraries, not to core syntax.

---

## 3. Source File Rules

- Default file extension: `.vss`
- A source file is plain UTF-8 text.
- One statement normally appears on one line.
- New lines separate statements.
- Indentation improves readability but does not define scope.
- Blocks are ended explicitly with the keyword `finish`.

Example:

```vss
make age becomes 20
when age above 18
    say "Adult"
finish
```

---

## 4. Comments

### 4.1 Single-line comments

Use `note` to begin a comment.

```vss
note this is a comment
make age becomes 20
```

Anything after `note` on the same line is ignored.

### 4.2 Multi-line comments

V0.1 does not require multi-line comments. They may be added later.

---

## 5. Keywords

Reserved words in VSS v0.1:

- `say`
- `make`
- `keep`
- `becomes`
- `when`
- `orwhen`
- `otherwise`
- `finish`
- `repeat`
- `times`
- `through`
- `to`
- `each`
- `in`
- `during`
- `leave`
- `skip`
- `task`
- `needs`
- `send`
- `with`
- `yes`
- `no`
- `empty`
- `and`
- `or`
- `not`
- `above`
- `below`
- `at_least`
- `at_most`
- `same_as`
- `not_same_as`
- `item`
- `field`
- `put`
- `into`
- `map`
- `set`
- `grab`
- `attempt`
- `rescue`
- `problem`
- `read`
- `write`
- `add`
- `erase`
- `exists`
- `size`
- `of`
- `choose`
- `case`

Future versions may add more words, but V0.1 should keep the list small.

---

## 6. Identifiers

Identifiers are names for variables, tasks, and modules.

Rules:

- must begin with a letter or underscore
- may contain letters, digits, and underscores
- are case-sensitive in V0.1
- cannot match reserved keywords

Examples:

Valid:
- `age`
- `user_name`
- `_temp`
- `result2`

Invalid:
- `2age`
- `make`
- `user-name`

---

## 7. Literals

### 7.1 Number literals

V0.1 supports:

- integers: `0`, `7`, `42`, `-5`
- decimal numbers: `3.14`, `-0.5`

Scientific notation may be added later.

### 7.2 String literals

Strings use double quotes.

```vss
"hello"
"VSS"
"line one"
```

Escape sequences supported in V0.1:

- `\"` for quote
- `\\` for backslash
- `\n` for newline
- `\t` for tab

### 7.3 Boolean literals

- `yes`
- `no`

### 7.4 Empty literal

- `empty`

This represents the absence of a value.

---

## 8. Core Types

VSS v0.1 has these runtime value categories:

- number
- text
- truth value
- empty value
- list
- map
- task value
- built-in value

Future versions may add sets, tuples, queues, stacks, shapes, and async jobs.

---

## 9. Variables and Constants

### 9.1 Mutable variables

Use `make`.

```vss
make age becomes 20
make name becomes "Asha"
```

### 9.2 Constants

Use `keep`.

```vss
keep pi becomes 3.14159
```

A constant may not be reassigned after definition.

### 9.3 Reassignment

Use `becomes`.

```vss
make count becomes 1
count becomes 2
```

If the name does not already exist, reassignment is an error.

---

## 10. Output

Use `say`.

```vss
say "Hello"
say age
say 10 + 20
```

`say` prints a readable text form of the value followed by a newline.

---

## 11. Expressions

Expressions produce values.

### 11.1 Arithmetic operators

Supported in V0.1:

- `+`
- `-`
- `*`
- `/`
- `%`

Examples:

```vss
make total becomes 10 + 5
make left becomes 8 - 3
make area becomes 4 * 6
make half becomes 20 / 2
make rest becomes 10 % 3
```

### 11.2 String joining

`+` may join text values.

```vss
make full becomes "Hello " + "World"
```

### 11.3 Comparison operators as words

Supported comparisons:

- `above`
- `below`
- `at_least`
- `at_most`
- `same_as`
- `not_same_as`

Examples:

```vss
when age above 18
when score at_least 90
when name same_as "VSS"
```

### 11.4 Logical operators

- `and`
- `or`
- `not`

Examples:

```vss
when age above 18 and age below 60
when not ready
```

### 11.5 Truth rules

In V0.1:

- `no` is false
- `empty` is false
- number `0` is false
- empty text `""` is false
- empty list is false
- all other values are true

This rule may be revised later, but it keeps beginner behavior simple.

---

## 12. Operator Precedence

From highest to lowest:

1. unary `not`, unary `-`
2. `*`, `/`, `%`
3. `+`, `-`
4. `above`, `below`, `at_least`, `at_most`, `same_as`, `not_same_as`
5. `and`
6. `or`

Parentheses are intentionally avoided in surface syntax for V0.1. If grouping becomes necessary, users should split expressions into smaller variables. Later versions may add optional grouping syntax.

---

## 13. Statements

VSS v0.1 supports these statement categories:

- variable definition
- constant definition
- reassignment
- output
- conditional block
- repeat loop
- during loop
- each loop
- task definition
- value return
- import
- collection update
- error handling block
- choose block
- expression statement for task calls

---

## 14. Conditional Execution

### 14.1 `when`

```vss
when age above 18
    say "Adult"
finish
```

### 14.2 `orwhen`

```vss
when score at_least 90
    say "A"
orwhen score at_least 80
    say "B"
otherwise
    say "C"
finish
```

### 14.3 `otherwise`

Optional final fallback branch.

Blocks must end with `finish`.

---

## 15. Loops

### 15.1 Repeat fixed count

```vss
repeat 5 times
    say "Hi"
finish
```

The body runs exactly five times.

### 15.2 Range loop

```vss
repeat i through 1 to 5
    say i
finish
```

For V0.1, the range is inclusive.

### 15.3 Each loop

```vss
repeat each color in colors
    say color
finish
```

This iterates through a list from first element to last.

### 15.4 Conditional loop

```vss
during count below 10
    say count
    count becomes count + 1
finish
```

### 15.5 Loop control

`leave` exits the nearest loop.

`skip` moves to the next loop iteration.

Example:

```vss
repeat i through 1 to 5
    when i same_as 3
        skip
    finish
    say i
finish
```

---

## 16. Choose Blocks

A switch-like statement uses `choose`.

```vss
choose color
    case "red"
        say "stop"
    case "green"
        say "go"
    otherwise
        say "unknown"
finish
```

Rules:

- the target expression is evaluated once
- each `case` compares using `same_as`
- the first matching case runs
- no fallthrough in V0.1
- `otherwise` is optional

---

## 17. Tasks

Tasks are VSS functions.

### 17.1 Define a task

```vss
task add needs first second
    send first + second
finish
```

### 17.2 Call a task

```vss
make result becomes add with 4 5
```

### 17.3 No-argument task

```vss
task hello
    say "Hi"
finish

hello with
```

V0.1 permits `with` even with no arguments for grammar consistency.

### 17.4 Return a value

Use `send`.

```vss
task square needs n
    send n * n
finish
```

If a task reaches `finish` without `send`, it returns `empty`.

### 17.5 Recursion

Tasks may call themselves if the runtime supports it.

---

## 18. Scope Rules

V0.1 uses lexical scope.

Scopes are created by:

- task bodies
- loop bodies
- conditional branches
- rescue blocks

Variable lookup searches:

1. local scope
2. enclosing scopes
3. global scope
4. built-ins

A new `make` inside an inner scope creates a new local variable, even if an outer one exists.

---

## 19. Collections

### 19.1 Lists

List literal syntax:

```vss
make items becomes ["pen", "book", "cup"]
```

Access element:

```vss
say items item 0
```

Append element:

```vss
put "lamp" into items
```

List length:

```vss
say size of items
```

Indexes are zero-based.

Out-of-range access is an error.

### 19.2 Maps

Map literal syntax:

```vss
make user becomes map [
    "name": "Asha"
    "age": 22
]
```

Access field by key:

```vss
say user field "name"
```

Set field by key:

```vss
set user field "city" becomes "Delhi"
```

In V0.1, keys should be text values.

### 19.3 Future collections

These are planned but not required in V0.1:

- sets
- tuples
- queues
- stacks

---

## 20. Imports and Modules

Use `grab`.

```vss
grab math
grab web
grab text_tools
```

V0.1 module rules:

- a module name maps to a `.vss` file or built-in module
- imported names become available in the current file
- exact export rules may be simple in V0.1: all top-level task names and constants are public

Alias support is postponed unless needed.

---

## 21. Error Handling

Use `attempt` and `rescue`.

```vss
attempt
    make text becomes read "data.txt"
rescue problem
    say "Could not read file"
    say problem
finish
```

Rules:

- code in `attempt` runs normally
- if an error occurs, control jumps to `rescue`
- `problem` is the caught error value
- if no `rescue` exists, the error stops the program

V0.1 does not include `always`. That may be added later.

---

## 22. Built-in File Features

Read file:

```vss
make text becomes read "notes.txt"
```

Write file:

```vss
write "hello" into "log.txt"
```

Append file:

```vss
add "more" into "log.txt"
```

Delete file:

```vss
erase "old.txt"
```

Check file existence:

```vss
when exists "data.txt"
    say "found"
finish
```

These may be implemented as built-ins instead of syntax nodes in the first compiler.

---

## 23. Expression-Based Built-ins

Some forms look like language syntax but may compile into built-in calls.

Examples:

- `size of items`
- `exists "data.txt"`
- `read "file.txt"`

This gives VSS a simple surface while keeping the runtime implementation manageable.

---

## 24. Errors

VSS errors should be readable and educational.

Error messages should include:

- file name
- line number
- column number if available
- what went wrong
- what was expected
- a possible fix suggestion

Example:

```text
line 4: expected 'finish' after 'when' block
help: add 'finish' to close the block
```

Examples of compile-time errors:

- unknown keyword use
- invalid token
- assigning to constant
- missing `finish`
- malformed list or map literal
- invalid task parameter list

Examples of runtime errors:

- divide by zero
- missing file
- out-of-range list access
- missing map key where required
- calling a non-task value

---

## 25. Type Behavior

V0.1 is dynamically typed.

This means:

- variables do not require declared types
- values carry runtime types
- operations check compatibility at runtime

Examples:

- number + number => number
- text + text => text
- number + text => runtime error in V0.1 unless explicit conversion is added later

Future versions may add optional type hints.

---

## 26. Memory Model

V0.1 uses automatic memory management.

The programmer does not manually allocate or free memory.

The C implementation may start with simple reference counting or mark-and-sweep garbage collection. The choice belongs to the runtime design, not to VSS source syntax.

---

## 27. Standard Library Direction

V0.1 core standard library areas:

- text
- lists
- maps
- files
- math
- time

Future official libraries:

- web
- http
- websocket
- database
- json
- async
- ai / mind
- vision
- data

These should feel native to VSS without hardcoding them into the language grammar.

---

## 28. AI Library Direction

AI is not part of the core parser.

Instead, VSS should include official libraries that reduce code for:

- model loading
- training
- prediction
- embeddings
- vector databases
- image tasks
- speech tasks
- text generation
- GPU execution

Example future style:

```vss
grab mind
make model becomes model open "image.vmodel"
make result becomes model predict "cat.jpg"
say result
```

This is a design target, not a V0.1 parser requirement.

---

## 29. Tooling Vision

Official tools should include:

- formatter
- linter
- package manager
- documentation generator
- testing framework
- debugger
- REPL
- build command

Suggested command layout:

- `vss run file.vss`
- `vss build file.vss`
- `vss room`
- `grab add web`
- `polish file.vss`
- `check file.vss`
- `prove`
- `guide make`
- `trace file.vss`

These names are part of the product vision, not the grammar.

---

## 30. Implementation Notes for v0.1

The first implementation in C should include:

- lexer
- parser
- AST
- semantic checks
- tree-walk interpreter
- basic runtime values
- environments
- built-ins
- REPL

Bytecode and VM should come next.

---

## 31. Formal Grammar Sketch

This is a readable grammar sketch, not a full parser generator grammar.

```text
program        -> statement* EOF

statement      -> make_stmt
               | keep_stmt
               | assign_stmt
               | say_stmt
               | when_stmt
               | repeat_stmt
               | during_stmt
               | task_stmt
               | send_stmt
               | grab_stmt
               | attempt_stmt
               | choose_stmt
               | expression_stmt

make_stmt      -> "make" IDENT "becomes" expression
keep_stmt      -> "keep" IDENT "becomes" expression
assign_stmt    -> IDENT "becomes" expression
say_stmt       -> "say" expression

grab_stmt      -> "grab" IDENT

when_stmt      -> "when" expression block
                  ("orwhen" expression block)*
                  ("otherwise" block)?
                  "finish"

repeat_stmt    -> "repeat" NUMBER "times" block "finish"
               |  "repeat" IDENT "through" expression "to" expression block "finish"
               |  "repeat" "each" IDENT "in" expression block "finish"

during_stmt    -> "during" expression block "finish"

task_stmt      -> "task" IDENT ("needs" IDENT*)? block "finish"
send_stmt      -> "send" expression

attempt_stmt   -> "attempt" block "rescue" IDENT block "finish"

choose_stmt    -> "choose" expression
                  ("case" expression block)+
                  ("otherwise" block)?
                  "finish"

block          -> NEWLINE statement*

expression     -> logic_or
logic_or       -> logic_and ("or" logic_and)*
logic_and      -> equality ("and" equality)*
equality       -> comparison (("same_as" | "not_same_as") comparison)*
comparison     -> term (("above" | "below" | "at_least" | "at_most") term)*
term           -> factor (("+" | "-") factor)*
factor         -> unary (("*" | "/" | "%") unary)*
unary          -> ("not" | "-") unary | postfix
postfix        -> primary (postfix_op)*
postfix_op     -> "item" expression
               |  "field" expression
               |  "with" argument_list

primary        -> NUMBER
               | STRING
               | "yes"
               | "no"
               | "empty"
               | IDENT
               | list_literal
               | map_literal

list_literal   -> "[" (expression ("," expression)*)? "]"
map_literal    -> "map" "[" map_entry* "]"
map_entry      -> STRING ":" expression
argument_list  -> expression*
```

The exact grammar can be tightened during parser implementation.

---

## 32. Example Programs

### 32.1 Hello world

```vss
say "Hello, world"
```

### 32.2 Variables and condition

```vss
make age becomes 20
when age at_least 18
    say "Adult"
otherwise
    say "Minor"
finish
```

### 32.3 Task and call

```vss
task add needs first second
    send first + second
finish

make answer becomes add with 4 5
say answer
```

### 32.4 List loop

```vss
make colors becomes ["red", "green", "blue"]
repeat each color in colors
    say color
finish
```

### 32.5 Error handling

```vss
attempt
    make text becomes read "missing.txt"
    say text
rescue problem
    say "Error happened"
    say problem
finish
```

---

## 33. Non-Goals for V0.1

The following are intentionally postponed:

- classes and inheritance
- advanced object system
- generics
- optional static typing
- macros
- annotations
- thread APIs
- native compiler backend
- LLVM backend
- sophisticated package resolution
- GPU runtime integration

This keeps V0.1 realistic.

---

## 34. Summary

VSS v0.1 is a line-based, beginner-friendly, original programming language design with explicit block endings, readable condition words, simple task syntax, dynamic typing, built-in collection support, and a clean path to implementation in C.

It is intentionally small in V0.1 so the implementation can be completed correctly and explained clearly.
