#ifndef VSS_CHUNK_H
#define VSS_CHUNK_H

#include "common.h"
#include "value.h"
typedef enum { _x0065, _x0066, _x0069, _x008d, _x006e, _x0080, _x0072, _x0088, _x0070, _x0067, _x0087, _x0075, _x008a, _x0057, _x008c, _x007c, _x0068, _x007b, _x007e, _x007d, _x0056, _x005d, _x005b, _x005c, _x0084, _x007f, _x0085, _x0078, _x0079, _x007a, _x0062, _x0064, _x0083, _x008f, _x005e, _x0060, _x005f, _x0071, _x0081, _x006f, _x0086, _x008b, _x006d, _x0082, _x008e, _x0058, _x006c, _x005a, _x006a, _x0076, _x0077, _x0061, _x0063, _x006b, _x0073, _x0089, _x0074, _x0059 } _x0096; typedef struct { uint8_t *_x01d2; int *_x031c; int _x01f4; int _x01b4; _x0138 *_x01ee; int _x01eb; int _x01ea; } _x0036; void _x04b2(_x0036 *_x01c3); void _x04b1(_x0036 *_x01c3); void _x04b3(_x0036 *_x01c3, uint8_t _x01a3, int _x0317); int _x04b0(_x0036 *_x01c3, _x0138 _x04a7); void _x04bc(_x0036 *_x01c3, const char *_x0362); int _x04bd(_x0036 *_x01c3, int _x0378);
#endif
