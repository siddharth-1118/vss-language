#ifndef vss_platform_h
#define vss_platform_h

#include <stdbool.h>
#include <stddef.h>

#ifdef _WIN32
  #include <winsock2.h>
  #include <ws2tcpip.h>
  typedef SOCKET VSS_Socket;
  #define VSS_INVALID_SOCKET INVALID_SOCKET
  #define VSS_PATH_SEP '\\'
  #define VSS_PATH_SEP_STR "\\"
#else
  typedef int VSS_Socket;
  #define VSS_INVALID_SOCKET (-1)
  #define VSS_PATH_SEP '/'
  #define VSS_PATH_SEP_STR "/"
#endif

// Network Sockets API
bool vss_network_init(void);
void vss_network_cleanup(void);

VSS_Socket vss_socket_create(void);
bool vss_socket_bind(VSS_Socket sock, int port);
bool vss_socket_listen(VSS_Socket sock, int backlog);
VSS_Socket vss_socket_accept(VSS_Socket sock);
int vss_socket_send(VSS_Socket sock, const char *buf, int len);
int vss_socket_recv(VSS_Socket sock, char *buf, int len);
void vss_socket_close(VSS_Socket sock);

// Filesystem API
bool vss_file_exists(const char *path);
bool vss_dir_exists(const char *path);
bool vss_make_dir(const char *path);
bool vss_list_dir_clean_vssc(const char *path);
int vss_scan_htmvss(const char *path, char ***filenames);

// Process and Environment API
void vss_launch_browser(int port, const char *filename);
int vss_execute_cmd(const char *cmd);
char *vss_get_home_dir(void);

#endif
