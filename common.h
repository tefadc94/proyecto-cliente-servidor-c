// ----------------------------
// common.h - Funciones compartidas
// ----------------------------

#ifndef COMMON_H
#define COMMON_H

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/time.h>
#include <pthread.h>
#include <semaphore.h>
#include <dirent.h>
#include <libgen.h>
#include <sys/sendfile.h>

#define PORT 8080
#define BUFFER_SIZE 65536
#define MAX_PATH_LEN 1024
#define FILES_DIR "./archivos"
#define MAX_THREADS 100
#define TIMEOUT_SEC 10
#define MAX_CONNECTIONS 100
#define SEND_TIMEOUT 30

typedef struct {
    char filename[256];
    size_t size;
    char last_modified[64];
    char content_type[64];
} FileInfo;

// Funciones
int validar_nombre_archivo(const char* filename);
FileInfo get_file_info(const char* path);
void enviar_error(int socket_cliente, int code, const char* message);
void enviar_archivo(int socket_cliente, const char* ruta);
void log_request(const char* ip, const char* request, int response_code);
const char* get_content_type(const char* filename);

// Sistema de logging
void log_server(const char* message);
void log_server_detail(const char* format, ...);
void log_thread(pthread_t tid, const char* message);
void log_thread_detail(pthread_t tid, const char* format, ...);
void log_process(pid_t pid, const char* message);
void log_process_detail(pid_t pid, const char* format, ...);

// Gestión de hilos
void init_thread_limit();
void wait_thread_slot();
void release_thread_slot();

#endif