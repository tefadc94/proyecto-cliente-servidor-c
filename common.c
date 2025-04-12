#include "common.h"

sem_t thread_sem;

pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

// Logging sincronizado
void log_server(const char* message) {
    pthread_mutex_lock(&log_mutex);
    time_t now = time(NULL);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));
    printf("[SERVER] [%s] %s\n", timestamp, message);
    pthread_mutex_unlock(&log_mutex);
}

void log_thread(pthread_t tid, const char* message) {
    pthread_mutex_lock(&log_mutex);
    time_t now = time(NULL);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));
    printf("[THREAD %lu] [%s] %s\n", (unsigned long)tid, timestamp, message);
    pthread_mutex_unlock(&log_mutex);
}

void log_server_detail(const char* format, ...) {
    pthread_mutex_lock(&log_mutex);
    time_t now = time(NULL);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));
    
    va_list args;
    va_start(args, format);
    printf("[SERVER] [%s] ", timestamp);
    vprintf(format, args);
    printf("\n");
    va_end(args);
    pthread_mutex_unlock(&log_mutex);
}

void log_thread_detail(pthread_t tid, const char* format, ...) {
    pthread_mutex_lock(&log_mutex);
    time_t now = time(NULL);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));
    
    va_list args;
    va_start(args, format);
    printf("[THREAD %lu] [%s] ", (unsigned long)tid, timestamp);
    vprintf(format, args);
    printf("\n");
    va_end(args);
    pthread_mutex_unlock(&log_mutex);
}
void log_process(pid_t pid, const char* message) {
    pthread_mutex_lock(&log_mutex);
    time_t now = time(NULL);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));
    printf("[PROCESS %d] [%s] %s\n", pid, timestamp, message);
    pthread_mutex_unlock(&log_mutex);
}

void log_process_detail(pid_t pid, const char* format, ...) {
    pthread_mutex_lock(&log_mutex);
    time_t now = time(NULL);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));
    
    va_list args;
    va_start(args, format);
    printf("[PROCESS %d] [%s] ", pid, timestamp);
    vprintf(format, args);
    printf("\n");
    va_end(args);
    pthread_mutex_unlock(&log_mutex);
}

// Inicializar el límite de hilos
void init_thread_limit() {
    sem_init(&thread_sem, 0, MAX_THREADS);
    log_server("Inicializado semáforo para límite de hilos");
}

// Esperar un slot disponible
void wait_thread_slot() {
    sem_wait(&thread_sem);
}

// Liberar un slot
void release_thread_slot() {
    sem_post(&thread_sem);
}

// Determinar el tipo de contenido basado en la extensión
const char* get_content_type(const char* filename) {
    const char *dot = strrchr(filename, '.');
    if (!dot) return "application/octet-stream";
    
    if (strcmp(dot, ".html") == 0 || strcmp(dot, ".htm") == 0) return "text/html";
    if (strcmp(dot, ".txt") == 0) return "text/plain";
    if (strcmp(dot, ".jpg") == 0 || strcmp(dot, ".jpeg") == 0) return "image/jpeg";
    if (strcmp(dot, ".png") == 0) return "image/png";
    if (strcmp(dot, ".gif") == 0) return "image/gif";
    if (strcmp(dot, ".pdf") == 0) return "application/pdf";
    if (strcmp(dot, ".zip") == 0) return "application/zip";
    if (strcmp(dot, ".json") == 0) return "application/json";
    if (strcmp(dot, ".css") == 0) return "text/css";
    if (strcmp(dot, ".js") == 0) return "application/javascript";
    
    return "application/octet-stream";
}

// Valida que el nombre de archivo no contenga caracteres peligrosos
int validar_nombre_archivo(const char* filename) {
    if (!filename || strlen(filename) == 0) {
        log_server_detail("Validación fallida: nombre de archivo vacío");
        return 0;
    }
    if (strstr(filename, "..") != NULL || strchr(filename, '/') != NULL) {
        log_server_detail("Validación fallida: nombre de archivo inseguro: %s", filename);
        return 0;
    }
    log_server_detail("Validación exitosa para archivo: %s", filename);
    return 1;
}

// Obtiene metadatos de un archivo
FileInfo get_file_info(const char* path) {
    FileInfo info = {0};
    struct stat file_stat;
    
    if (stat(path, &file_stat) == 0) {
        snprintf(info.filename, sizeof(info.filename), "%s", path);
        info.size = file_stat.st_size;
        
        struct tm *tm_info = localtime(&file_stat.st_mtime);
        strftime(info.last_modified, sizeof(info.last_modified),
                 "%a, %d %b %Y %H:%M:%S GMT", tm_info);
        
        const char* content_type = get_content_type(path);
        strncpy(info.content_type, content_type, sizeof(info.content_type)-1);
        info.content_type[sizeof(info.content_type)-1] = '\0';
        
        log_server_detail("Obtenida información para %s: tamaño=%zu, tipo=%s", 
                         path, info.size, info.content_type);
    } else {
        log_server_detail("Error al obtener información del archivo %s", path);
    }
    return info;
}

// Envía una respuesta de error HTTP
void enviar_error(int socket_cliente, int code, const char* message) {
    char response[BUFFER_SIZE];
    const char* status;
    
    switch(code) {
        case 400: status = "400 Bad Request"; break;
        case 403: status = "403 Forbidden"; break;
        case 404: status = "404 Not Found"; break;
        case 405: status = "405 Method Not Allowed"; break;
        default:  status = "500 Internal Server Error"; break;
    }
    
    int len = snprintf(response, sizeof(response),
        "HTTP/1.1 %s\r\n"
        "Content-Type: text/html\r\n"
        "Connection: close\r\n"
        "Content-Length: %zu\r\n\r\n"
        "<html><body><h1>%s</h1><p>%s</p></body></html>",
        status, strlen(message) + 28, status, message);
    
    send(socket_cliente, response, len, 0);
    log_server_detail("Enviado error HTTP %d: %s", code, message);
}


void enviar_archivo(int socket_cliente, const char* ruta) {
    int file_fd = open(ruta, O_RDONLY);
    if (file_fd < 0) {
        log_server_detail("Error abriendo archivo: %s (%s)", ruta, strerror(errno));
        enviar_error(socket_cliente, 404, "Archivo no encontrado");
        return;
    }

    FileInfo info = get_file_info(ruta);
    
    // Configurar timeout de envío
    struct timeval tv = {SEND_TIMEOUT, 0};
    setsockopt(socket_cliente, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    
    char header[BUFFER_SIZE];
    int header_len = snprintf(header, sizeof(header),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %zu\r\n"
        "Last-Modified: %s\r\n"
        "Connection: close\r\n\r\n",
        info.content_type, info.size, info.last_modified);
    
    if (send(socket_cliente, header, header_len, 0) < 0) {
        log_server_detail("Error enviando cabecera: %s", strerror(errno));
        close(file_fd);
        return;
    }

    off_t offset = 0;
    size_t remaining = info.size;
    ssize_t sent;
    
    while (remaining > 0 && (sent = sendfile(socket_cliente, file_fd, &offset, remaining)) > 0) {
        remaining -= sent;
    }

    if (sent < 0) {
        log_server_detail("Error enviando datos: %s", strerror(errno));
    }

    close(file_fd);
    log_server_detail("Transferencia completada: %s (%zu/%zu bytes)", 
                     ruta, info.size - remaining, info.size);
}

// Registra una solicitud en el log
void log_request(const char* ip, const char* request, int response_code) {
    time_t now = time(NULL);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));
    
    printf("[REQUEST] [%s] %s - %s - %d\n", timestamp, ip, request, response_code);
}