#include <stdio.h>
#include <stdlib.h>
#include "chunk.h"
#include "object.h"

void chunk_init(Chunk *chunk) {
    chunk->code = NULL;
    chunk->lines = NULL;
    chunk->count = 0;
    chunk->capacity = 0;
    chunk->constants = NULL;
    chunk->const_count = 0;
    chunk->const_capacity = 0;
}

void chunk_free(Chunk *chunk) {
    free(chunk->code);
    free(chunk->lines);
    for (int i = 0; i < chunk->const_count; i++) {
        value_release(chunk->constants[i]);
    }
    free(chunk->constants);
    chunk_init(chunk);
}

void chunk_write(Chunk *chunk, uint8_t byte, int line) {
    if (chunk->count >= chunk->capacity) {
        chunk->capacity = chunk->capacity < 8 ? 8 : chunk->capacity * 2;
        chunk->code = realloc(chunk->code, chunk->capacity);
        chunk->lines = realloc(chunk->lines, chunk->capacity * sizeof(int));
    }
    chunk->code[chunk->count] = byte;
    chunk->lines[chunk->count] = line;
    chunk->count++;
}

int chunk_add_constant(Chunk *chunk, Value value) {
    // Retain the constant value
    value_retain(value);
    
    if (chunk->const_count >= chunk->const_capacity) {
        chunk->const_capacity = chunk->const_capacity < 8 ? 8 : chunk->const_capacity * 2;
        chunk->constants = realloc(chunk->constants, chunk->const_capacity * sizeof(Value));
    }
    chunk->constants[chunk->const_count] = value;
    return chunk->const_count++;
}

void disassemble_chunk(Chunk *chunk, const char *name) {
    printf("== %s ==\n", name);
    for (int offset = 0; offset < chunk->count;) {
        offset = disassemble_instruction(chunk, offset);
    }
}

static int simple_instruction(const char *name, int offset) {
    printf("%s\n", name);
    return offset + 1;
}

static int constant_instruction(const char *name, Chunk *chunk, int offset) {
    uint8_t constant = chunk->code[offset + 1];
    printf("%-16s %4d '", name, constant);
    value_say(chunk->constants[constant]);
    printf("'\n");
    return offset + 2;
}

static int byte_instruction(const char *name, Chunk *chunk, int offset) {
    uint8_t slot = chunk->code[offset + 1];
    printf("%-16s %4d\n", name, slot);
    return offset + 2;
}

static int jump_instruction(const char *name, int sign, Chunk *chunk, int offset) {
    uint16_t jump = (uint16_t)(chunk->code[offset + 1] << 8);
    jump |= chunk->code[offset + 2];
    printf("%-16s %4d -> %d\n", name, offset, offset + 3 + sign * jump);
    return offset + 3;
}

static int global_define_instruction(const char *name, Chunk *chunk, int offset) {
    uint8_t constant = chunk->code[offset + 1];
    uint8_t is_const = chunk->code[offset + 2];
    printf("%-16s %4d '", name, constant);
    value_say(chunk->constants[constant]);
    printf("' (const: %s)\n", is_const ? "yes" : "no");
    return offset + 3;
}

int disassemble_instruction(Chunk *chunk, int offset) {
    printf("%04d ", offset);
    if (offset > 0 && chunk->lines[offset] == chunk->lines[offset - 1]) {
        printf("   | ");
    } else {
        printf("%4d ", chunk->lines[offset]);
    }

    uint8_t instruction = chunk->code[offset];
    switch (instruction) {
        case OP_CONSTANT:
            return constant_instruction("OP_CONSTANT", chunk, offset);
        case OP_CONSTANT_LONG: {
            uint32_t constant = chunk->code[offset + 1] |
                                (chunk->code[offset + 2] << 8) |
                                (chunk->code[offset + 3] << 16);
            printf("%-16s %4d '", "OP_CONSTANT_LONG", constant);
            value_say(chunk->constants[constant]);
            printf("'\n");
            return offset + 4;
        }
        case OP_EMPTY:
            return simple_instruction("OP_EMPTY", offset);
        case OP_TRUE:
            return simple_instruction("OP_TRUE", offset);
        case OP_FALSE:
            return simple_instruction("OP_FALSE", offset);
        case OP_POP:
            return simple_instruction("OP_POP", offset);
        case OP_GET_LOCAL:
            return byte_instruction("OP_GET_LOCAL", chunk, offset);
        case OP_SET_LOCAL:
            return byte_instruction("OP_SET_LOCAL", chunk, offset);
        case OP_GET_GLOBAL:
            return constant_instruction("OP_GET_GLOBAL", chunk, offset);
        case OP_DEFINE_GLOBAL:
            return global_define_instruction("OP_DEFINE_GLOBAL", chunk, offset);
        case OP_SET_GLOBAL:
            return constant_instruction("OP_SET_GLOBAL", chunk, offset);
        case OP_GET_UPVALUE:
            return byte_instruction("OP_GET_UPVALUE", chunk, offset);
        case OP_SET_UPVALUE:
            return byte_instruction("OP_SET_UPVALUE", chunk, offset);
        case OP_ADD:
            return simple_instruction("OP_ADD", offset);
        case OP_SUB:
            return simple_instruction("OP_SUB", offset);
        case OP_MUL:
            return simple_instruction("OP_MUL", offset);
        case OP_DIV:
            return simple_instruction("OP_DIV", offset);
        case OP_MOD:
            return simple_instruction("OP_MOD", offset);
        case OP_NOT:
            return simple_instruction("OP_NOT", offset);
        case OP_NEGATE:
            return simple_instruction("OP_NEGATE", offset);
        case OP_ABOVE:
            return simple_instruction("OP_ABOVE", offset);
        case OP_BELOW:
            return simple_instruction("OP_BELOW", offset);
        case OP_AT_LEAST:
            return simple_instruction("OP_AT_LEAST", offset);
        case OP_AT_MOST:
            return simple_instruction("OP_AT_MOST", offset);
        case OP_SAME_AS:
            return simple_instruction("OP_SAME_AS", offset);
        case OP_NOT_SAME_AS:
            return simple_instruction("OP_NOT_SAME_AS", offset);
        case OP_SAY:
            return simple_instruction("OP_SAY", offset);
        case OP_JUMP:
            return jump_instruction("OP_JUMP", 1, chunk, offset);
        case OP_JUMP_IF_FALSE:
            return jump_instruction("OP_JUMP_IF_FALSE", 1, chunk, offset);
        case OP_LOOP:
            return jump_instruction("OP_LOOP", -1, chunk, offset);
        case OP_CALL:
            return byte_instruction("OP_CALL", chunk, offset);
        case OP_CLOSURE: {
            offset++;
            uint8_t constant = chunk->code[offset++];
            printf("%-16s %4d ", "OP_CLOSURE", constant);
            value_say(chunk->constants[constant]);
            printf("\n");
            
            ObjFunction *func = chunk->constants[constant].as.function;
            for (int j = 0; j < func->upvalue_count; j++) {
                int is_local = chunk->code[offset++];
                int index = chunk->code[offset++];
                printf("%04d      |                     %s %d\n",
                       offset - 2, is_local ? "local" : "upvalue", index);
            }
            return offset;
        }
        case OP_RETURN:
            return simple_instruction("OP_RETURN", offset);
        case OP_BUILD_LIST:
            return byte_instruction("OP_BUILD_LIST", chunk, offset);
        case OP_BUILD_MAP:
            return byte_instruction("OP_BUILD_MAP", chunk, offset);
        case OP_GET_ITEM:
            return simple_instruction("OP_GET_ITEM", offset);
        case OP_PUT_ITEM:
            return simple_instruction("OP_PUT_ITEM", offset);
        case OP_GET_FIELD:
            return simple_instruction("OP_GET_FIELD", offset);
        case OP_SET_FIELD:
            return simple_instruction("OP_SET_FIELD", offset);
        case OP_SIZE_OF:
            return simple_instruction("OP_SIZE_OF", offset);
        case OP_EXISTS:
            return simple_instruction("OP_EXISTS", offset);
        case OP_READ_FILE:
            return simple_instruction("OP_READ_FILE", offset);
        case OP_WRITE_FILE:
            return simple_instruction("OP_WRITE_FILE", offset);
        case OP_ADD_FILE:
            return simple_instruction("OP_ADD_FILE", offset);
        case OP_ERASE_FILE:
            return simple_instruction("OP_ERASE_FILE", offset);
        case OP_ATTEMPT:
            return jump_instruction("OP_ATTEMPT", 1, chunk, offset);
        case OP_END_ATTEMPT:
            return simple_instruction("OP_END_ATTEMPT", offset);
        case OP_GRAB:
            return constant_instruction("OP_GRAB", chunk, offset);
        case OP_HI_HTMVSS:
            return simple_instruction("OP_HI_HTMVSS", offset);
        case OP_BYE_HTMVSS:
            return simple_instruction("OP_BYE_HTMVSS", offset);
        default:
            printf("Unknown opcode %d\n", instruction);
            return offset + 1;
    }
}
