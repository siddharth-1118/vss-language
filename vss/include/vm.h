#ifndef VSS_VM_H
#define VSS_VM_H

#include <setjmp.h>
#include "common.h"
#include "value.h"
#include "object.h"
#include "env.h"

#define _x009b 256
#define _x0050 64
#define _x011e 16
typedef struct { _x0091 *_x01cd; uint8_t *_x02d8; _x0138 *_x0440; } _x0034; typedef struct { int _x0210; uint8_t *_x02ac; } _x0121; typedef struct _x0132 { _x0034 _x028a[_x0050]; int _x0289; _x0138 _x044e[_x009b]; _x0138 *_x044f; _x0121 _x048f[_x011e]; int _x048e; _x0122 *_x037b; _x0048 *_x02a0; jmp_buf _x02fa; bool _x053c; struct _x0132 *_x03cc; } _x0132; void _x0531(_x0132 *_x04ac, _x0048 *_x029e); void _x0530(_x0132 *_x04ac); bool _x0532(_x0094 *_x028d, _x0048 *_x029e);
#endif
