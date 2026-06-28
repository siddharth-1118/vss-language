# VSS Lexer Implementation Walkthrough

This document explains the first lexer implementation for VSS in C.

## 1. What the lexer does

The lexer is the first stage of the compiler.

It reads raw source text like this:

```vss
make age becomes 20
say age
```

and turns it into tokens like this:

- TOKEN_MAKE
- TOKEN_IDENTIFIER(age)
- TOKEN_BECOMES
- TOKEN_NUMBER(20)
- TOKEN_NEWLINE
- TOKEN_SAY
- TOKEN_IDENTIFIER(age)
- TOKEN_NEWLINE
- TOKEN_EOF

The parser will later consume these tokens.

---

## 2. Why write the lexer first

The lexer is the foundation of everything that comes next.

If tokens are wrong, then:
- the parser will fail
- error messages will be confusing
- later stages will be harder to debug

So the lexer should be built and tested before the parser.

---

## 3. Files involved

Current lexer files:

- `include/token.h`
- `include/lexer.h`
- `src/token.c`
- `src/lexer.c`
- `src/main.c`

Each file has a specific role.

### `token.h`
Defines token kinds and the `Token` structure.

### `lexer.h`
Defines the `Lexer` structure and function declarations.

### `token.c`
Maps token enums to readable names for debugging.

### `lexer.c`
Contains the actual tokenization logic.

### `main.c`
Currently works as a lexer demo runner that prints tokens.

---

## 4. Token structure explanation

The token struct is:

```c
typedef struct {
    TokenType type;
    const char *start;
    size_t length;
    int line;
    int column;
} Token;
```

### Why each field exists

#### `type`
This tells us what kind of token it is.

Example:
- TOKEN_MAKE
- TOKEN_NUMBER
- TOKEN_STRING

#### `start`
This points into the original source text at the first character of the token.

Why this is useful:
- avoids copying memory during lexing
- makes token creation faster
- lets debug tools print the original text directly

#### `length`
This tells how many characters the token spans.

Since `start` is only a pointer, we need `length` to know where the token ends.

#### `line`
The source line number where the token begins.

This is necessary for error reporting.

#### `column`
The source column where the token begins.

This helps produce precise diagnostics.

---

## 5. Lexer state explanation

The lexer struct is:

```c
typedef struct {
    const char *source;
    const char *start;
    const char *current;
    int line;
    int column;
    int token_column;
} Lexer;
```

### Meaning of each field

#### `source`
Points to the beginning of the full source text.

#### `start`
Points to the beginning of the current token being scanned.

#### `current`
Points to the current character being examined.

This pointer moves forward as the lexer reads input.

#### `line`
Tracks the current line number.

#### `column`
Tracks the current column number.

#### `token_column`
Stores the column where the current token started.

This matters because `column` changes as characters are consumed. Without `token_column`, the token would only know where scanning ended, not where it began.

---

## 6. Helper functions in `lexer.c`

## `is_at_end`

```c
static bool is_at_end(Lexer *lexer) {
    return *lexer->current == '\0';
}
```

### Purpose
Checks whether we have reached the end of the source string.

### Why it matters
The source code is stored as a null-terminated C string. The zero byte `\0` marks the end.

This function makes end checks readable and reusable.

---

## `advance_char`

```c
static char advance_char(Lexer *lexer) {
    char c = *lexer->current;
    lexer->current++;
    lexer->column++;
    return c;
}
```

### Purpose
Consumes the current character and moves forward by one.

### Step-by-step
1. read the character under `current`
2. move `current` to the next character
3. increase the column count
4. return the consumed character

### Why this design is good
It centralizes pointer movement. That reduces bugs from manually incrementing the pointer in many places.

---

## `peek_char`

```c
static char peek_char(Lexer *lexer) {
    return *lexer->current;
}
```

### Purpose
Looks at the current character without consuming it.

### Why it matters
The lexer often needs to decide what to do before moving forward.

Example:
- if the next character is a digit, continue scanning a number
- if the next character is a quote, finish a string when it closes

---

## `peek_next_char`

```c
static char peek_next_char(Lexer *lexer) {
    if (is_at_end(lexer)) {
        return '\0';
    }
    return lexer->current[1];
}
```

### Purpose
Looks one character ahead without consuming it.

### Why it matters
This is useful for decimal numbers and escape handling.

Example:
- when reading `12.5`, after seeing `.`, we need to know whether a digit follows

---

## `make_token`

```c
static Token make_token(Lexer *lexer, TokenType type) {
    Token token;
    token.type = type;
    token.start = lexer->start;
    token.length = (size_t)(lexer->current - lexer->start);
    token.line = lexer->line;
    token.column = lexer->token_column;
    return token;
}
```

### Purpose
Creates a normal token from the current token span.

### Important detail
`token.length` is calculated using pointer subtraction.

That works because both pointers point into the same source string.

### Why this is efficient
No string copy is needed here.

---

## `error_token`

```c
static Token error_token(Lexer *lexer, const char *message) {
    Token token;
    token.type = TOKEN_ERROR;
    token.start = message;
    token.length = strlen(message);
    token.line = lexer->line;
    token.column = lexer->token_column;
    return token;
}
```

### Purpose
Creates a special error token.

### Why use an error token
Instead of crashing immediately, the lexer can return a token that says what went wrong.

This makes early testing easier.

Later we may switch to a richer diagnostic system.

---

## `skip_spaces`

```c
static void skip_spaces(Lexer *lexer) {
    for (;;) {
        char c = peek_char(lexer);
        if (c == ' ' || c == '\t' || c == '\r') {
            advance_char(lexer);
        } else {
            break;
        }
    }
}
```

### Purpose
Skips spacing characters that do not matter for tokens.

### Why newline is not skipped
Because VSS is line-oriented, newlines are meaningful. They separate statements.

So:
- spaces are ignored
- tabs are ignored
- carriage returns are ignored
- newlines are emitted as tokens

That is a very important design decision.

---

## 7. Identifier handling

## `is_name_start`

```c
static bool is_name_start(char c) {
    return isalpha((unsigned char)c) || c == '_';
}
```

### Purpose
Checks whether a character can begin an identifier.

### Why cast to `unsigned char`
The C character classification functions like `isalpha` are safest when the value is cast to `unsigned char`.

This avoids undefined behavior for negative `char` values on some systems.

---

## `is_name_part`

```c
static bool is_name_part(char c) {
    return isalnum((unsigned char)c) || c == '_';
}
```

### Purpose
Checks whether a character can continue an identifier.

This allows:
- letters
- digits
- underscore

---

## `keyword_type`

This function compares scanned identifier text against the reserved words table.

If the text matches a keyword like `make`, it returns `TOKEN_MAKE`.
Otherwise it returns `TOKEN_IDENTIFIER`.

### Why use a table
Because it keeps the lexer simple and makes keywords easy to add.

A future optimization could replace linear search with a trie or keyword switch logic, but this is perfectly fine for the first version.

---

## `identifier`

```c
static Token identifier(Lexer *lexer) {
    while (is_name_part(peek_char(lexer))) {
        advance_char(lexer);
    }

    TokenType type = keyword_type(lexer->start, (size_t)(lexer->current - lexer->start));
    return make_token(lexer, type);
}
```

### Purpose
Scans either:
- an identifier
- or a keyword

### Algorithm
1. keep consuming valid identifier characters
2. compare the full text against the keyword table
3. return either the keyword token or a generic identifier token

---

## 8. Number handling

## `number`

```c
static Token number(Lexer *lexer) {
    while (isdigit((unsigned char)peek_char(lexer))) {
        advance_char(lexer);
    }

    if (peek_char(lexer) == '.' && isdigit((unsigned char)peek_next_char(lexer))) {
        advance_char(lexer);
        while (isdigit((unsigned char)peek_char(lexer))) {
            advance_char(lexer);
        }
    }

    return make_token(lexer, TOKEN_NUMBER);
}
```

### Purpose
Scans integer and decimal number literals.

### Algorithm
1. consume all leading digits
2. if a dot is followed by a digit, treat it as a decimal point
3. consume the decimal digits
4. return TOKEN_NUMBER

### Why require a digit after the dot
This prevents `10.` from being silently treated as a decimal literal in V0.1.

That makes the rule simpler.

---

## 9. String handling

## `string`

```c
static Token string(Lexer *lexer) {
    while (!is_at_end(lexer) && peek_char(lexer) != '"') {
        if (peek_char(lexer) == '\n') {
            lexer->line++;
            lexer->column = 1;
        }
        if (peek_char(lexer) == '\\' && peek_next_char(lexer) != '\0') {
            advance_char(lexer);
        }
        advance_char(lexer);
    }

    if (is_at_end(lexer)) {
        return error_token(lexer, "Unterminated string.");
    }

    advance_char(lexer);
    return make_token(lexer, TOKEN_STRING);
}
```

### Purpose
Scans a string literal that starts with a double quote.

### Algorithm
1. keep consuming characters until the closing quote or end of file
2. if a newline appears, update line and column tracking
3. if a backslash appears and another character follows, consume the backslash so escaped characters are handled as part of the string text span
4. if no closing quote appears before end of input, return an error token
5. consume the final quote and return TOKEN_STRING

### Important note
This version recognizes strings lexically but does not yet decode escape sequences into final runtime characters.

That decoding will happen later, likely when building AST literal nodes or runtime values.

---

## 10. Lexer initialization

## `lexer_init`

```c
void lexer_init(Lexer *lexer, const char *source) {
    lexer->source = source;
    lexer->start = source;
    lexer->current = source;
    lexer->line = 1;
    lexer->column = 1;
    lexer->token_column = 1;
}
```

### Purpose
Sets the lexer to the start of the source text.

### Why line and column start at 1
Because source locations are normally shown to humans starting at line 1, column 1.

---

## 11. Main tokenization function

## `lexer_next`

This is the heart of the lexer.

### Step 1: skip spaces

```c
skip_spaces(lexer);
lexer->start = lexer->current;
lexer->token_column = lexer->column;
```

We ignore spaces and tabs before each token.
Then we mark the start of the next token.

---

### Step 2: end-of-file check

```c
if (is_at_end(lexer)) {
    return make_token(lexer, TOKEN_EOF);
}
```

If there is no more input, return EOF.

---

### Step 3: consume one character

```c
char c = advance_char(lexer);
```

This gives us the first character of the next token.

---

### Step 4: newline handling

```c
if (c == '\n') {
    Token token = make_token(lexer, TOKEN_NEWLINE);
    lexer->line++;
    lexer->column = 1;
    return token;
}
```

### Why emit NEWLINE
Because VSS separates statements by line.

### Why line increases after token creation
The newline token belongs to the line where it was found. After that, scanning moves to the next line.

---

### Step 5: identifier/keyword handling

```c
if (is_name_start(c)) {
    return identifier(lexer);
}
```

If the token begins with a letter or underscore, scan a word token.

---

### Step 6: number handling

```c
if (isdigit((unsigned char)c)) {
    return number(lexer);
}
```

If the token begins with a digit, scan a numeric literal.

---

### Step 7: punctuation and symbols

The switch handles simple one-character tokens.

```c
switch (c) {
    case '"': return string(lexer);
    case '+': return make_token(lexer, TOKEN_PLUS);
    case '-': return make_token(lexer, TOKEN_MINUS);
    case '*': return make_token(lexer, TOKEN_STAR);
    case '/': return make_token(lexer, TOKEN_SLASH);
    case '%': return make_token(lexer, TOKEN_PERCENT);
    case '[': return make_token(lexer, TOKEN_LEFT_BRACKET);
    case ']': return make_token(lexer, TOKEN_RIGHT_BRACKET);
    case ',': return make_token(lexer, TOKEN_COMMA);
    case ':': return make_token(lexer, TOKEN_COLON);
    default: return error_token(lexer, "Unexpected character.");
}
```

### Why this is enough for now
VSS v0.1 deliberately uses little punctuation. So the lexer remains small and understandable.

---

## 12. Current limitations

This lexer is a strong first step, but it is still incomplete.

### Missing features
- comment skipping for `note`
- negative numbers as parser-level unary minus rather than lexical literals
- better string escape validation
- more detailed error messages
- token buffering utilities for parser use
- unit tests

### Important design point
Negative numbers should usually be parsed as:
- `-` token
- `NUMBER` token

instead of a single negative-number token.

That is already compatible with the current lexer design.

---

## 13. Why the lexer design is good for VSS

This implementation fits VSS well because:

- it preserves newlines
- it handles word-based keywords clearly
- it avoids unnecessary allocation during lexing
- it tracks source location for future diagnostics
- it is easy to extend

---

## 14. Next lexer improvements

Recommended next tasks:

1. make `note` comments ignore the rest of the line
2. add a token dump test suite
3. validate escape sequences in strings
4. improve unexpected-character diagnostics
5. add parser support next

---

## 15. Teaching summary

The lexer works by moving through the source one character at a time.

At each step it asks:
- is this space?
- is this newline?
- is this a word?
- is this a number?
- is this a string?
- is this punctuation?
- otherwise, is it invalid?

That is the essence of lexical analysis.

A good lexer is not about cleverness. It is about being predictable, correct, and easy to extend.
