#ifndef VSS_OBJECT_H
#define VSS_OBJECT_H

#include "common.h"
#include "value.h"
#include "chunk.h"
typedef struct _x0094 { int _x03e6; char *_x0362; size_t _x0396; _x0036 _x01c3; int _x049d; } _x0094; typedef struct _x0122 _x0122; struct _x0122 { int _x03e6; _x0138 *_x0326; _x0138 _x01ca; _x0122 *_x036c; }; typedef struct _x0091 { int _x03e6; _x0094 *_x0291; _x0122 **_x049e; int _x049d; _x0138 _x03e5; } _x0091; _x0094 *_x04da(const char *_x0362, size_t _x0396); void _x04dc(_x0094 *_x028d); void _x04db(_x0094 *_x028d); _x0122 *_x0519(_x0138 *_x043f); void _x051b(_x0122 *_x04a0); void _x051a(_x0122 *_x04a0); _x0091 *_x04b4(_x0094 *_x028d); void _x04b6(_x0091 *_x01cd); void _x04b5(_x0091 *_x01cd);
#endif
