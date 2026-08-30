#ifndef vss_platform_h
#define vss_platform_h

#include <stdbool.h>
#include <stddef.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
typedef _x001f _x00b9; #define _x0053 _x0011
#define VSS_PATH_SEP '\\'
#define _x0097 "\\"
#else
typedef int _x00b9; #define _x0053 (-1)
#define VSS_PATH_SEP '/'
#define _x0097 "/"
#endif
bool _x04e8(void); void _x04e7(void); _x00b9 _x04f6(void); bool _x04f4(_x00b9 _x0441, int _x03c6); bool _x04f7(_x00b9 _x0441, int _x015a); _x00b9 _x04f3(_x00b9 _x0441); int _x04f9(_x00b9 _x0441, const char *_x016e, int _x0311); int _x04f8(_x00b9 _x0441, char *_x016e, int _x0311); void _x04f5(_x00b9 _x0441); bool _x04d9(const char *_x03b7); bool _x04bb(const char *_x03b7); bool _x04e6(const char *_x03b7); bool _x04e5(const char *_x03b7); int _x04ee(const char *_x03b7, char ***_x026a); int _x04e4(const char *_x03b7, char ***_x026a); void _x04e1(int _x03c6, const char *_x0269); int _x04c6(const char *_x01d1); char *_x04dd(void);
#endif
