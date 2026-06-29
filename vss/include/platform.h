#ifndef vss_platform_h
#define vss_platform_h

#include <stdbool.h>
#include <stddef.h>

#ifdef _WIN32
  #include <winsock2.h>
  #include <ws2tcpip.h>
  typedef SOCKET VssSocket;
  #define VSS_INVALID_SOCKET INVALID_SOCKET
  #define VSS_PATH_SEP '\\'
  #define VSS_PATH_SEP_STR "\\"
#else
  typedef int VssSocket;
  #define VSS_INVALID_SOCKET (-1)
  #define VSS_PATH_SEP '/'
  #define VSS_PATH_SEP_STR "/"
#endif

// Network Sockets API
bool vss_network_init(void);
void vss_network_cleanup(void);

VssSocket vss_socket_create(void);
bool vss_socket_bind(VssSocket sock, int port);
bool vss_socket_listen(VssSocket sock, int backlog);
VssSocket vss_socket_accept(VssSocket sock);
int vss_socket_send(VssSocket sock, const char *buf, int len);
int vss_socket_recv(VssSocket sock, char *buf, int len);
void vss_socket_close(VssSocket sock);

// Filesystem API
bool vss_file_exists(const char *path);
bool vss_dir_exists(const char *path);
bool vss_make_dir(const char *path);
bool vss_list_dir_clean_vssc(const char *path);

// Process and Environment API
void vss_launch_browser(int port);
int vss_execute_cmd(const char *cmd);
char *vss_get_home_dir(void);

#endif
