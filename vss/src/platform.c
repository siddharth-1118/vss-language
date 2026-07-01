#include "platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *platform_strdup(const char *s) {
    if (!s) return NULL;
    char *dup = malloc(strlen(s) + 1);
    if (dup) {
        strcpy(dup, s);
    }
    return dup;
}

#ifdef _WIN32
#include <direct.h>
#include <io.h>
#include <windows.h>
#include <shellapi.h>

bool vss_network_init(void) {
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2,2), &wsa) != 0) {
        return false;
    }
    return true;
}

void vss_network_cleanup(void) {
    WSACleanup();
}

VSS_Socket vss_socket_create(void) {
    VSS_Socket sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) return VSS_INVALID_SOCKET;
    
    // Set reuseaddr
    int opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt));
    return sock;
}

bool vss_socket_bind(VSS_Socket sock, int port) {
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);
    if (bind(sock, (struct sockaddr *)&address, sizeof(address)) == SOCKET_ERROR) {
        return false;
    }
    return true;
}

bool vss_socket_listen(VSS_Socket sock, int backlog) {
    if (listen(sock, backlog) == SOCKET_ERROR) {
        return false;
    }
    return true;
}

VSS_Socket vss_socket_accept(VSS_Socket sock) {
    VSS_Socket client = accept(sock, NULL, NULL);
    if (client == INVALID_SOCKET) return VSS_INVALID_SOCKET;
    return client;
}

int vss_socket_send(VSS_Socket sock, const char *buf, int len) {
    return send(sock, buf, len, 0);
}

int vss_socket_recv(VSS_Socket sock, char *buf, int len) {
    return recv(sock, buf, len, 0);
}

void vss_socket_close(VSS_Socket sock) {
    if (sock != INVALID_SOCKET) {
        closesocket(sock);
    }
}

bool vss_file_exists(const char *path) {
    DWORD dwAttrib = GetFileAttributesA(path);
    return (dwAttrib != INVALID_FILE_ATTRIBUTES && !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

bool vss_dir_exists(const char *path) {
    DWORD dwAttrib = GetFileAttributesA(path);
    return (dwAttrib != INVALID_FILE_ATTRIBUTES && (dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

bool vss_make_dir(const char *path) {
    return CreateDirectoryA(path, NULL) != 0 || GetLastError() == ERROR_ALREADY_EXISTS;
}

bool vss_list_dir_clean_vssc(const char *path) {
    char search_path[MAX_PATH];
    snprintf(search_path, sizeof(search_path), "%s\\*.vssc", path);
    
    WIN32_FIND_DATAA fd;
    HANDLE hFind = FindFirstFileA(search_path, &fd);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            char filepath[MAX_PATH];
            snprintf(filepath, sizeof(filepath), "%s\\%s", path, fd.cFileName);
            DeleteFileA(filepath);
            printf("  Removed: %s\n", fd.cFileName);
        } while (FindNextFileA(hFind, &fd));
        FindClose(hFind);
    }
    return true;
}

int vss_scan_htmvss(const char *path, char ***filenames) {
    char search_path[MAX_PATH];
    snprintf(search_path, sizeof(search_path), "%s\\*.htmvss", path);
    
    WIN32_FIND_DATAA fd;
    HANDLE hFind = FindFirstFileA(search_path, &fd);
    if (hFind == INVALID_HANDLE_VALUE) {
        *filenames = NULL;
        return 0;
    }
    
    int count = 0;
    char **list = NULL;
    do {
        list = realloc(list, sizeof(char*) * (count + 1));
        list[count] = platform_strdup(fd.cFileName);
        count++;
    } while (FindNextFileA(hFind, &fd));
    
    FindClose(hFind);
    *filenames = list;
    return count;
}

void vss_launch_browser(int port, const char *filename) {
    char url[512];
    snprintf(url, sizeof(url), "http://localhost:%d/%s", port, filename);
    ShellExecuteA(NULL, "open", url, NULL, NULL, SW_SHOWNORMAL);
}

int vss_execute_cmd(const char *cmd) {
    return system(cmd);
}

char *vss_get_home_dir(void) {
    const char *home = getenv("USERPROFILE");
    if (!home) {
        const char *drive = getenv("HOMEDRIVE");
        const char *path = getenv("HOMEPATH");
        if (drive && path) {
            char *buf = malloc(strlen(drive) + strlen(path) + 1);
            sprintf(buf, "%s%s", drive, path);
            return buf;
        }
    }
    return home ? platform_strdup(home) : NULL;
}

#else
// POSIX Implementation (Linux/macOS)
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>

bool vss_network_init(void) {
    return true;
}

void vss_network_cleanup(void) {
    // No-op
}

VSS_Socket vss_socket_create(void) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return VSS_INVALID_SOCKET;
    
    int opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    return sock;
}

bool vss_socket_bind(VSS_Socket sock, int port) {
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);
    if (bind(sock, (struct sockaddr *)&address, sizeof(address)) < 0) {
        return false;
    }
    return true;
}

bool vss_socket_listen(VSS_Socket sock, int backlog) {
    if (listen(sock, backlog) < 0) {
        return false;
    }
    return true;
}

VSS_Socket vss_socket_accept(VSS_Socket sock) {
    int client = accept(sock, NULL, NULL);
    if (client < 0) return VSS_INVALID_SOCKET;
    return client;
}

int vss_socket_send(VSS_Socket sock, const char *buf, int len) {
    return send(sock, buf, len, 0);
}

int vss_socket_recv(VSS_Socket sock, char *buf, int len) {
    return recv(sock, buf, len, 0);
}

void vss_socket_close(VSS_Socket sock) {
    if (sock >= 0) {
        close(sock);
    }
}

bool vss_file_exists(const char *path) {
    return access(path, F_OK) == 0;
}

bool vss_dir_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

bool vss_make_dir(const char *path) {
    return mkdir(path, 0777) == 0;
}

bool vss_list_dir_clean_vssc(const char *path) {
    DIR *d = opendir(path);
    if (d) {
        struct dirent *dir;
        while ((dir = readdir(d)) != NULL) {
            char *ext = strrchr(dir->d_name, '.');
            if (ext && strcmp(ext, ".vssc") == 0) {
                char filepath[1024];
                snprintf(filepath, sizeof(filepath), "%s/%s", path, dir->d_name);
                remove(filepath);
                printf("  Removed: %s\n", dir->d_name);
            }
        }
        closedir(d);
    }
    return true;
}

int vss_scan_htmvss(const char *path, char ***filenames) {
    DIR *d = opendir(path);
    if (!d) {
        *filenames = NULL;
        return 0;
    }
    
    int count = 0;
    char **list = NULL;
    struct dirent *dir;
    while ((dir = readdir(d)) != NULL) {
        char *ext = strrchr(dir->d_name, '.');
        if (ext && strcmp(ext, ".htmvss") == 0) {
            list = realloc(list, sizeof(char*) * (count + 1));
            list[count] = platform_strdup(dir->d_name);
            count++;
        }
    }
    closedir(d);
    *filenames = list;
    return count;
}

void vss_launch_browser(int port, const char *filename) {
    char launch_cmd[512];
#ifdef __APPLE__
    snprintf(launch_cmd, sizeof(launch_cmd), "open http://localhost:%d/%s 2>/dev/null &", port, filename);
#else
    snprintf(launch_cmd, sizeof(launch_cmd), "xdg-open http://localhost:%d/%s 2>/dev/null &", port, filename);
#endif
    system(launch_cmd);
}

int vss_execute_cmd(const char *cmd) {
    return system(cmd);
}

char *vss_get_home_dir(void) {
    const char *home = getenv("HOME");
    return home ? platform_strdup(home) : NULL;
}

#endif
