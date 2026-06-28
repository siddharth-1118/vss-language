#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/types.h>

#include "lexer.h"
#include "parser.h"
#include "interpreter.h"
#include "env.h"
#include "ast.h"

static char *read_file_text(const char *path) {
    FILE *file = fopen(path, "rb");
    if (!file) {
        fprintf(stderr, "Could not open file: %s\n", path);
        return NULL;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        fprintf(stderr, "Could not seek file: %s\n", path);
        return NULL;
    }

    long size = ftell(file);
    if (size < 0) {
        fclose(file);
        fprintf(stderr, "Could not measure file: %s\n", path);
        return NULL;
    }

    rewind(file);

    char *buffer = (char *)malloc((size_t)size + 1);
    if (!buffer) {
        fclose(file);
        fprintf(stderr, "Out of memory while reading file.\n");
        return NULL;
    }

    size_t read_count = fread(buffer, 1, (size_t)size, file);
    fclose(file);

    if (read_count != (size_t)size) {
        free(buffer);
        fprintf(stderr, "Could not fully read file: %s\n", path);
        return NULL;
    }

    buffer[size] = '\0';
    return buffer;
}

static void start_server(int port) {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket failed");
        return;
    }
    
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);
    
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        close(server_fd);
        return;
    }
    
    if (listen(server_fd, 10) < 0) {
        perror("listen failed");
        close(server_fd);
        return;
    }
    
    fprintf(stderr, "VSS Dev Server started on http://localhost:%d/\n", port);
    fprintf(stderr, "Press Ctrl+C to stop.\n");
    
    // Automatically launch the browser to the default page
    char launch_cmd[256];
    snprintf(launch_cmd, sizeof(launch_cmd), "explorer.exe http://localhost:%d/my-webpage.htmvss 2>/dev/null &", port);
    system(launch_cmd);
    
    while (1) {
        int client_fd = accept(server_fd, NULL, NULL);
        if (client_fd < 0) continue;
        
        char buffer[2048];
        ssize_t read_bytes = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
        if (read_bytes <= 0) {
            close(client_fd);
            continue;
        }
        buffer[read_bytes] = '\0';
        
        char method[16], path[256];
        if (sscanf(buffer, "%15s %255s", method, path) == 2) {
            char *filename = path;
            if (filename[0] == '/') filename++;
            if (strlen(filename) == 0) {
                filename = "my-webpage.htmvss";
            }
            
            size_t fn_len = strlen(filename);
            bool is_htmvss = (fn_len > 7 && strcmp(filename + fn_len - 7, ".htmvss") == 0);
            
            FILE *f = fopen(filename, "r");
            if (!f) {
                const char *res_404 = "HTTP/1.1 404 Not Found\r\nContent-Type: text/plain\r\nConnection: close\r\n\r\nFile Not Found";
                send(client_fd, res_404, strlen(res_404), 0);
            } else {
                fclose(f);
                if (is_htmvss) {
                    int stdout_dup = dup(1);
                    FILE *temp_f = freopen("temp_serve.html", "w", stdout);
                    if (temp_f) {
                        char *source = read_file_text(filename);
                        if (source) {
                            Lexer lexer;
                            lexer_init(&lexer, source);
                            Parser parser;
                            parser_init(&parser, &lexer);
                            Block ast = parse_program(&parser);
                            if (!parser.had_error) {
                                Env *global_env = env_new(NULL);
                                register_builtins(global_env);
                                FlowResult result = interpret(ast, global_env);
                                if (result.type == FLOW_ERROR) {
                                    printf("\n<div style='color:red;border:1px solid red;padding:10px;margin-top:10px;'>Runtime Error: %s</div>", result.error_msg);
                                    free(result.error_msg);
                                } else if (result.type == FLOW_SEND) {
                                    value_release(result.value);
                                }
                                env_release(global_env);
                            } else {
                                printf("\n<div style='color:red;border:1px solid red;padding:10px;margin-top:10px;'>Parser Syntax Error</div>");
                            }
                            block_free(ast);
                            free(source);
                        }
                        fflush(stdout);
                        dup2(stdout_dup, 1);
                        close(stdout_dup);
                        
                        char *html = read_file_text("temp_serve.html");
                        remove("temp_serve.html");
                        if (html) {
                            char headers[512];
                            snprintf(headers, sizeof(headers), "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: %zu\r\nConnection: close\r\n\r\n", strlen(html));
                            send(client_fd, headers, strlen(headers), 0);
                            send(client_fd, html, strlen(html), 0);
                            free(html);
                        } else {
                            const char *res_500 = "HTTP/1.1 500 Internal Server Error\r\nContent-Type: text/plain\r\nConnection: close\r\n\r\nCould not read VSS output";
                            send(client_fd, res_500, strlen(res_500), 0);
                        }
                    } else {
                        dup2(stdout_dup, 1);
                        close(stdout_dup);
                        const char *res_500 = "HTTP/1.1 500 Internal Server Error\r\nContent-Type: text/plain\r\nConnection: close\r\n\r\nCould not redirect stdout";
                        send(client_fd, res_500, strlen(res_500), 0);
                    }
                } else {
                    char *content = read_file_text(filename);
                    if (content) {
                        char headers[512];
                        snprintf(headers, sizeof(headers), "HTTP/1.1 200 OK\r\nContent-Length: %zu\r\nConnection: close\r\n\r\n", strlen(content));
                        send(client_fd, headers, strlen(headers), 0);
                        send(client_fd, content, strlen(content), 0);
                        free(content);
                    }
                }
            }
        }
        close(client_fd);
    }
    close(server_fd);
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <file.vss>, %s --serve, or %s --version\n", argv[0], argv[0], argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "-v") == 0) {
        printf("VSS v1.0.0\n");
        return 0;
    }

    if (strcmp(argv[1], "--serve") == 0 || strcmp(argv[1], "-s") == 0) {
        start_server(8080);
        return 0;
    }

    const char *filename = argv[1];
    size_t fn_len = strlen(filename);
    bool is_htmvss = false;
    char *out_html_path = NULL;

    if (fn_len > 7 && strcmp(filename + fn_len - 7, ".htmvss") == 0) {
        is_htmvss = true;
        out_html_path = malloc(fn_len - 7 + 6); // Replace .htmvss with .html
        memcpy(out_html_path, filename, fn_len - 7);
        strcpy(out_html_path + fn_len - 7, ".html");
    }

    char *source = read_file_text(filename);
    if (!source) {
        if (is_htmvss) free(out_html_path);
        return 1;
    }

    Lexer lexer;
    lexer_init(&lexer, source);

    Parser parser;
    parser_init(&parser, &lexer);

    Block ast = parse_program(&parser);

    if (parser.had_error) {
        if (is_htmvss) free(out_html_path);
        block_free(ast);
        free(source);
        return 1;
    }

    if (is_htmvss) {
        if (!freopen(out_html_path, "w", stdout)) {
            fprintf(stderr, "Could not redirect output to %s\n", out_html_path);
            free(out_html_path);
            block_free(ast);
            free(source);
            return 1;
        }
    }

    Env *global_env = env_new(NULL);
    register_builtins(global_env);

    FlowResult result = interpret(ast, global_env);

    int exit_code = 0;
    if (result.type == FLOW_ERROR) {
        fprintf(stderr, "runtime error line %d, col %d: %s\n", result.line, result.column, result.error_msg);
        free(result.error_msg);
        exit_code = 1;
    } else if (result.type == FLOW_SEND) {
        fprintf(stderr, "runtime error line %d, col %d: 'send' statement outside task.\n", result.line, result.column);
        value_release(result.value);
        exit_code = 1;
    } else if (result.type == FLOW_LEAVE) {
        fprintf(stderr, "runtime error line %d, col %d: 'leave' statement outside loop.\n", result.line, result.column);
        exit_code = 1;
    } else if (result.type == FLOW_SKIP) {
        fprintf(stderr, "runtime error line %d, col %d: 'skip' statement outside loop.\n", result.line, result.column);
        exit_code = 1;
    }

    env_release(global_env);
    block_free(ast);
    free(source);

    if (is_htmvss) {
        fprintf(stderr, "HTML generated successfully: %s\n", out_html_path);
        // Automatically open the HTML page in Windows browser via explorer.exe
        char cmd[512];
        snprintf(cmd, sizeof(cmd), "explorer.exe %s 2>/dev/null", out_html_path);
        system(cmd);
        free(out_html_path);
    }

    return exit_code;
}
