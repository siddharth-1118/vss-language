# VSS Compiler and Runtime Architecture in C

## 1. Goal

This document defines the architecture for the first VSS implementation in C.

The implementation strategy is staged:

1. lexer
2. parser
3. AST builder
4. semantic analyzer
5. tree-walk interpreter
6. bytecode compiler
7. virtual machine
8. native backend later

The first milestone should produce a working interpreter for VSS v0.1.

---

## 2. Why implement in stages

Building a full VM first is possible, but not ideal for a language that is still evolving.

A tree-walk interpreter gives these benefits:

- faster language iteration
- easier debugging
- clearer semantics
- simpler teaching path
- easier parser validation

Once syntax and semantics are stable, a bytecode compiler can be added without redesigning the whole front end.

---

## 3. High-Level Architecture

```text
source file
   -> lexer
   -> token stream
   -> parser
   -> AST
   -> semantic analyzer
   -> interpreter
   -> runtime result
```

Later:

```text
source file
   -> lexer
   -> parser
   -> AST
   -> semantic analyzer
   -> bytecode compiler
   -> bytecode
   -> VM
   -> runtime result
```

---

## 4. Project Layout

```text
vss/
├── Makefile
├── README.md
├── docs/
│   ├── vss-spec-v0.1.md
│   └── compiler-architecture.md
├── examples/
│   ├── hello.vss
│   ├── conditions.vss
│   └── tasks.vss
├── include/
│   ├── common.h
│   ├── token.h
│   ├── lexer.h
│   ├── ast.h
│   ├── parser.h
│   ├── value.h
│   ├── env.h
│   ├── interpreter.h
│   ├── error.h
│   └── memory.h
├── src/
│   ├── main.c
│   ├── token.c
│   ├── lexer.c
│   ├── parser.c
│   ├── ast.c
│   ├── value.c
│   ├── env.c
│   ├── interpreter.c
│   ├── error.c
│   └── memory.c
├── tests/
│   ├── lexer/
│   ├── parser/
│   └── runtime/
└── tools/
    └── repl_plan.md
```

This layout keeps headers separated from implementation files and leaves space for future VM work.

---

## 5. Main Subsystems

## 5.1 Lexer

### Purpose
The lexer converts raw source text into tokens.

### Input
- UTF-8 source text

### Output
- array of tokens

### Responsibilities
- skip whitespace
- track line and column
- recognize identifiers and keywords
- recognize numbers
- recognize strings
- recognize operators
- recognize delimiters like `[` `]` `,` `:`
- emit end-of-line tokens if needed
- emit EOF token
- report invalid characters

### Important note
Because VSS is line-oriented, the lexer should emit `TOKEN_NEWLINE` so the parser can separate statements easily.

---

## 5.2 Parser

### Purpose
The parser turns tokens into an AST.

### Strategy
Use a **recursive descent parser**.

Why:
- easy to write in C
- easy to explain
- ideal for small to medium language grammars
- supports custom statement forms well

### Responsibilities
- parse top-level statements
- parse block structures ending with `finish`
- parse expressions with precedence
- build AST nodes
- recover from syntax errors where possible

---

## 5.3 AST

### Purpose
The AST captures the logical program structure independent of source formatting.

### Why AST first
- clearer separation of syntax and execution
- easier semantic checks
- easier pretty printing and debugging
- reusable for both interpreter and bytecode compiler

### Node categories
- expressions
- statements
- program root

---

## 5.4 Semantic Analyzer

### Purpose
Perform checks after parsing but before execution.

### Checks for V0.1
- duplicate constant definitions in the same scope
- assignment to constant
- `send` outside task
- `leave` outside loop
- `skip` outside loop
- invalid import names if statically detectable

Some checks can also happen at runtime in V0.1. The semantic analyzer can begin small.

---

## 5.5 Interpreter

### Purpose
Execute the AST directly.

### Design
Use a tree-walk interpreter with environments.

### Responsibilities
- evaluate expressions
- execute statements
- manage scopes
- call built-ins and user tasks
- handle runtime errors
- support truth evaluation

---

## 5.6 Runtime Value System

### Purpose
Represent VSS values in C.

### Value kinds for V0.1
- number
- string
- boolean
- empty
- list
- map
- task
- native function

The runtime should use a tagged union.

---

## 5.7 Environment System

### Purpose
Store variables and constants in nested scopes.

### Requirements
- define mutable and constant bindings
- assign existing mutable bindings
- lookup by name
- parent links for lexical scope

---

## 5.8 Error System

### Purpose
Provide readable compile-time and runtime errors.

### Requirements
- source location
- category: lexer/parser/semantic/runtime
- human-readable message
- optional fix hint

---

## 5.9 Memory Management

### Initial recommendation
Start with a simple ownership model and heap allocation helpers.

Then move to either:
- reference counting for early versions, or
- mark-and-sweep GC once closures and cycles appear

For V0.1, reference counting plus clear container ownership can work, but VM stage will likely benefit from tracing GC.

---

## 6. Front-End Design

## 6.1 Token Model

Suggested token structure:

```c
typedef struct {
    TokenType type;
    const char *start;
    size_t length;
    int line;
    int column;
} Token;
```

Why store slices instead of copies:
- fewer allocations
- faster lexing
- easier diagnostics

Later, parser or AST creation can duplicate text when needed.

---

## 6.2 Token Types

Categories:

### Special
- EOF
- NEWLINE
- ERROR

### Literals
- IDENTIFIER
- NUMBER
- STRING

### Keywords
- SAY
- MAKE
- KEEP
- BECOMES
- WHEN
- ORWHEN
- OTHERWISE
- FINISH
- REPEAT
- TIMES
- THROUGH
- TO
- EACH
- IN
- DURING
- LEAVE
- SKIP
- TASK
- NEEDS
- SEND
- WITH
- YES
- NO
- EMPTY
- AND
- OR
- NOT
- ABOVE
- BELOW
- AT_LEAST
- AT_MOST
- SAME_AS
- NOT_SAME_AS
- ITEM
- FIELD
- PUT
- INTO
- MAP
- SET
- GRAB
- ATTEMPT
- RESCUE
- READ
- WRITE
- ADD
- ERASE
- EXISTS
- SIZE
- OF
- CHOOSE
- CASE
- NOTE

### Operators and punctuation
- PLUS
- MINUS
- STAR
- SLASH
- PERCENT
- LEFT_BRACKET
- RIGHT_BRACKET
- COMMA
- COLON

Even though VSS minimizes punctuation, these tokens are still needed internally.

---

## 6.3 Parsing Strategy

### Statement parsing
The parser should first inspect the current token and dispatch:

- `make` -> parse variable declaration
- `keep` -> parse constant declaration
- `say` -> parse print statement
- `when` -> parse conditional
- `repeat` -> parse loop
- `during` -> parse while-like loop
- `task` -> parse function definition
- `attempt` -> parse try-like block
- `choose` -> parse switch-like block
- `grab` -> parse import
- identifier followed by `becomes` -> parse assignment
- otherwise parse expression statement

### Expression parsing
Use precedence-based recursive descent functions:

- parse_or
- parse_and
- parse_equality
- parse_comparison
- parse_term
- parse_factor
- parse_unary
- parse_postfix
- parse_primary

This is simple and maintainable.

---

## 7. AST Design

## 7.1 Expression Kinds

Suggested expression enum:

```c
typedef enum {
    EXPR_NUMBER,
    EXPR_STRING,
    EXPR_BOOL,
    EXPR_EMPTY,
    EXPR_NAME,
    EXPR_BINARY,
    EXPR_UNARY,
    EXPR_LIST,
    EXPR_MAP,
    EXPR_ITEM_ACCESS,
    EXPR_FIELD_ACCESS,
    EXPR_CALL
} ExprKind;
```

## 7.2 Statement Kinds

```c
typedef enum {
    STMT_MAKE,
    STMT_KEEP,
    STMT_ASSIGN,
    STMT_SAY,
    STMT_WHEN,
    STMT_REPEAT_COUNT,
    STMT_REPEAT_RANGE,
    STMT_REPEAT_EACH,
    STMT_DURING,
    STMT_LEAVE,
    STMT_SKIP,
    STMT_TASK,
    STMT_SEND,
    STMT_GRAB,
    STMT_ATTEMPT,
    STMT_CHOOSE,
    STMT_PUT,
    STMT_SET_FIELD,
    STMT_EXPR
} StmtKind;
```

## 7.3 Program Root

```c
typedef struct {
    Stmt **statements;
    size_t count;
    size_t capacity;
} Program;
```

Dynamic arrays are fine for the first version.

---

## 8. Runtime Value Design

Suggested tagged union:

```c
typedef enum {
    VAL_NUMBER,
    VAL_STRING,
    VAL_BOOL,
    VAL_EMPTY,
    VAL_LIST,
    VAL_MAP,
    VAL_TASK,
    VAL_NATIVE
} ValueType;

typedef struct Value Value;

struct Value {
    ValueType type;
    union {
        double number;
        int boolean;
        char *string;
        struct List *list;
        struct Map *map;
        struct Task *task;
        struct NativeFn *native;
    } as;
};
```

### Why this works
- compact
- standard in C interpreters
- simple dispatch in evaluation code

---

## 9. Environment Design

Suggested binding model:

```c
typedef struct {
    char *name;
    Value value;
    int is_constant;
} Binding;

typedef struct Env {
    Binding *items;
    size_t count;
    size_t capacity;
    struct Env *parent;
} Env;
```

### Operations needed
- `env_define`
- `env_define_const`
- `env_assign`
- `env_get`
- `env_exists_local`

A hash table would be faster, but a dynamic array is simpler for the earliest version. Once the language grows, switch to hashing.

---

## 10. Control Flow Handling in Interpreter

Some statements affect control flow non-locally:

- `send`
- `leave`
- `skip`
- runtime error

In C, a common strategy is to return an execution result struct.

Example idea:

```c
typedef enum {
    FLOW_NORMAL,
    FLOW_SEND,
    FLOW_LEAVE,
    FLOW_SKIP,
    FLOW_ERROR
} FlowType;

typedef struct {
    FlowType type;
    Value value;
    RuntimeError error;
} FlowResult;
```

This avoids using `setjmp` in the first implementation.

---

## 11. Built-in Functions and Surface Forms

VSS has beginner-friendly phrases like:

- `read "file.txt"`
- `size of items`
- `exists "a.txt"`

Implementation options:

### Option A: special AST nodes
Pros:
- custom diagnostics
- exact syntax support

Cons:
- more parser complexity

### Option B: desugar into built-in calls
Example:
- `read "a.txt"` becomes `__read("a.txt")`
- `size of items` becomes `__size(items)`

Pros:
- simpler interpreter
- more reusable built-in mechanism

Recommended for V0.1:
**parse these as distinct expression forms or special prefix expressions, then lower them to built-in calls internally.**

---

## 12. REPL Design

The REPL should:

- read one or more lines
- detect incomplete blocks when `finish` is missing
- preserve global environment between commands
- print expression values optionally
- show friendly errors

This can come soon after the interpreter works for files.

---

## 13. Bytecode VM Plan

After the interpreter works, add a second execution engine.

## 13.1 Bytecode Goals

- faster execution
- lower overhead than AST walk
- easier future optimization
- stable runtime model for package ecosystem

## 13.2 VM Components

- instruction enum
- constant pool
- bytecode chunk
- operand stack
- call frames
- globals table
- heap manager

## 13.3 Possible instruction examples

- LOAD_CONST
- LOAD_NAME
- STORE_NAME
- ADD
- SUB
- MUL
- DIV
- MOD
- COMPARE_ABOVE
- COMPARE_SAME
- JUMP
- JUMP_IF_FALSE
- CALL
- RETURN
- PRINT
- BUILD_LIST
- BUILD_MAP
- GET_ITEM
- GET_FIELD
- POP

The AST compiler should emit these.

---

## 14. Native Compiler Future

A future native backend can target:

- C as an intermediate stage, or
- LLVM IR

LLVM is better long term, but not needed early.

Recommended order:
1. interpreter
2. bytecode VM
3. optimization passes
4. optional LLVM backend

---

## 15. Error Recovery Strategy

Parser recovery should avoid stopping at the first error whenever possible.

Suggested sync points:
- NEWLINE
- `finish`
- top-level keywords like `make`, `when`, `task`, `repeat`

This lets one file report several syntax problems in one run.

---

## 16. Testing Strategy

### Lexer tests
- keyword recognition
- string handling
- line tracking
- error token emission

### Parser tests
- expressions
- nested when blocks
- tasks
- loops
- malformed input recovery

### Runtime tests
- arithmetic
- truth logic
- scopes
- constants
- list/map operations
- error handling

### Golden tests
Store source file + expected output or expected error.

---

## 17. Build Strategy

Start with a simple `Makefile`.

Targets:
- `make`
- `make run`
- `make test`
- `make clean`

Compiler flags:
- `-std=c11`
- `-Wall`
- `-Wextra`
- `-Werror` optional later
- debug builds with `-g`

---

## 18. Recommended Implementation Order

### Phase 1: infrastructure
- common headers
- token definitions
- error struct
- dynamic array helpers

### Phase 2: lexer
- tokenization
- line/column tracking
- test driver

### Phase 3: parser
- expression parser
- statement parser
- AST printer for debugging

### Phase 4: runtime core
- value type
- environment
- built-ins

### Phase 5: interpreter
- statement execution
- function calls
- control flow

### Phase 6: file runner and REPL

### Phase 7: semantic checks

### Phase 8: bytecode engine

---

## 19. Design Decisions Summary

### Why recursive descent?
Because VSS syntax is keyword-rich and custom-shaped. Recursive descent is easier than parser generators for this style.

### Why explicit `finish`?
It makes parsing blocks simple and beginner-readable while avoiding braces.

### Why dynamic typing first?
It minimizes syntax complexity and speeds early implementation.

### Why AST before bytecode?
It stabilizes semantics and supports multiple backends.

### Why C?
Because it gives full control over memory, runtime structures, portability, and future performance.

---

## 20. First Milestone Definition

The first usable VSS implementation should support:

- `say`
- `make`
- `keep`
- reassignment
- numbers, strings, yes/no/empty
- arithmetic
- comparisons
- `when/orwhen/otherwise/finish`
- `repeat`, `during`
- `leave`, `skip`
- `task`, `needs`, `send`, `with`
- list literals
- map literals
- `grab` stub support
- interpreter from source file

If that works reliably, VSS will already be a real small language.
