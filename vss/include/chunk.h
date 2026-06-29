#ifndef VSS_CHUNK_H
#define VSS_CHUNK_H

#include "common.h"
#include "value.h"

typedef enum {
    OP_CONSTANT,       // Push constant (1-byte index operand)
    OP_CONSTANT_LONG,  // Push constant (3-byte index operand)
    OP_EMPTY,          // Push empty
    OP_TRUE,           // Push yes (true)
    OP_FALSE,          // Push no (false)
    OP_POP,            // Pop top of stack
    
    OP_GET_LOCAL,      // Read local variable (1-byte stack slot index)
    OP_SET_LOCAL,      // Write local variable (1-byte stack slot index)
    
    OP_GET_GLOBAL,     // Read global variable (operand: constant pool string index)
    OP_DEFINE_GLOBAL,  // Define global variable/constant (operand: constant pool string index, +1 byte is_const flag)
    OP_SET_GLOBAL,     // Assign to global variable (operand: constant pool string index)
    
    OP_GET_UPVALUE,    // Read captured closure variable (operand: upvalue index)
    OP_SET_UPVALUE,    // Write captured closure variable (operand: upvalue index)
    
    OP_ADD,            // Add numbers or join strings
    OP_SUB,            // Subtract numbers
    OP_MUL,            // Multiply numbers
    OP_DIV,            // Divide numbers
    OP_MOD,            // Modulo of numbers
    
    OP_NOT,            // Logical NOT
    OP_NEGATE,         // Numeric negation
    
    OP_ABOVE,          // Compare above (>)
    OP_BELOW,          // Compare below (<)
    OP_AT_LEAST,       // Compare at least (>=)
    OP_AT_MOST,        // Compare at most (<=)
    OP_SAME_AS,        // Compare equality (same_as)
    OP_NOT_SAME_AS,    // Compare inequality (not_same_as)
    
    OP_SAY,            // Print top of stack followed by newline
    
    OP_JUMP,           // Jump forward/backward unconditionally (2-byte offset operand)
    OP_JUMP_IF_FALSE,  // Jump if stack top is falsy (2-byte offset operand)
    OP_LOOP,           // Loop jump backward (2-byte offset operand)
    
    OP_CALL,           // Call a closure or native function (1-byte operand: argument count)
    OP_CLOSURE,        // Instantiate closure (operand: function index in constants, followed by upvalue layout)
    OP_RETURN,         // Return from current task
    
    OP_BUILD_LIST,     // Create list from stack elements (1-byte operand: count)
    OP_BUILD_MAP,      // Create map from stack elements (1-byte operand: key-value pair count)
    OP_GET_ITEM,       // Access list item: stack has [list, index] -> pushes item
    OP_PUT_ITEM,       // Put item in list: stack has [val, list] -> appends val
    OP_GET_FIELD,      // Access map field: stack has [map, field_key] -> pushes field value
    OP_SET_FIELD,      // Set map field: stack has [map, field_key, val] -> sets field
    
    OP_SIZE_OF,        // Get size of list/map/string
    OP_EXISTS,         // File exists check
    OP_READ_FILE,      // Read file
    OP_WRITE_FILE,     // Write content into file
    OP_ADD_FILE,       // Append content to file
    OP_ERASE_FILE,     // Delete file
    
    OP_ATTEMPT,        // Enter attempt block (2-byte jump offset to rescue block)
    OP_END_ATTEMPT,    // Exit attempt block (pop exception handler)
    OP_GRAB,           // Import/grab module (1-byte operand: constant pool string index)
    
    OP_HI_HTMVSS,      // Emit HTML document prefix
    OP_BYE_HTMVSS      // Emit HTML document suffix
} OpCode;

typedef struct {
    uint8_t *code;
    int *lines;
    int count;
    int capacity;
    Value *constants;
    int const_count;
    int const_capacity;
} Chunk;

void chunk_init(Chunk *chunk);
void chunk_free(Chunk *chunk);
void chunk_write(Chunk *chunk, uint8_t byte, int line);
int chunk_add_constant(Chunk *chunk, Value value);

// Disassembly tools for debugging
void disassemble_chunk(Chunk *chunk, const char *name);
int disassemble_instruction(Chunk *chunk, int offset);

#endif
