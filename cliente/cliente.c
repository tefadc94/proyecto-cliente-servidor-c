#include "common.h"

typedef struct {
    char server_ip[16];
    int port;
    char filename[256];
    pthread_t tid;
} DownloadTask;

pthread_mutex_t file_mutex = PTHREAD_MUTEX_INITIALIZER;

void* download_thread(void* arg) {
    DownloadTask* task = (DownloadTask*)arg;
    log_thread(task->tid, "Iniciando descarga");
    log_thread_detail(task->tid, "Archivo: %s", task->filename);
    
    // Crear directorio si no existe
    if (mkdir("descargas", 0755) && errno != EEXIST) {
        log_thread_detail(task->tid, "Error creando directorio: %s", strerror(errno));
        free(task);
        return NULL;
    }

    // Preparar archivo de salida
    char output_path[MAX_PATH_LEN];
    snprintf(output_path, sizeof(output_path), "descargas/%s", basename(task->filename));
    
    FILE* output_file = fopen(output_path, "wb");
    if (!output_file) {
        log_thread_detail(task->tid, "Error abriendo archivo: %s", strerror(errno));
        free(task);
        return NULL;
    }

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        log_thread_detail(task->tid, "Error creando socket: %s", strerror(errno));
        fclose(output_file);
        free(task);
        return NULL;
    }

    // Configurar timeout
    struct timeval tv = {TIMEOUT_SEC, 0};
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    struct sockaddr_in server_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(task->port)
    };
    
    if (inet_pton(AF_INET, task->server_ip, &server_addr.sin_addr) <= 0) {
        log_thread_detail(task->tid, "Dirección IP inválida: %s", task->server_ip);
        close(sockfd);
        fclose(output_file);
        free(task);
        return NULL;
    }

    if (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        log_thread_detail(task->tid, "Error de conexión: %s", strerror(errno));
        close(sockfd);
        fclose(output_file);
        free(task);
        return NULL;
    }

    // Construir solicitud HTTP
    char request[1024];
    int req_len = snprintf(request, sizeof(request),
        "GET /%s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "Connection: close\r\n\r\n",
        task->filename, task->server_ip);

    if (send(sockfd, request, req_len, 0) < 0) {
        log_thread_detail(task->tid, "Error enviando solicitud: %s", strerror(errno));
        close(sockfd);
        fclose(output_file);
        free(task);
        return NULL;
    }

    // Procesar respuesta
    char buffer[BUFFER_SIZE];
    size_t total_received = 0;
    size_t content_length = 0;
    int headers_received = 0;
    
    while (1) {
        ssize_t bytes = recv(sockfd, buffer, sizeof(buffer), 0);
        if (bytes <= 0) break;

        if (!headers_received) {
            char* header_end = strstr(buffer, "\r\n\r\n");
            if (header_end) {
                // Parsear Content-Length
                char* cl_header = strstr(buffer, "Content-Length: ");
                if (cl_header) {
                    sscanf(cl_header, "Content-Length: %zu", &content_length);
                }

                // Escribir cuerpo
                size_t header_size = header_end - buffer + 4;
                size_t body_size = bytes - header_size;
                
                if (body_size > 0) {
                    pthread_mutex_lock(&file_mutex);
                    if (fwrite(header_end + 4, 1, body_size, output_file) != body_size) {
                        log_thread_detail(task->tid, "Error escribiendo datos iniciales");
                    }
                    pthread_mutex_unlock(&file_mutex);
                    total_received += body_size;
                }
                headers_received = 1;
            }
        } else {
            pthread_mutex_lock(&file_mutex);
            if (fwrite(buffer, 1, bytes, output_file) != bytes) {
                log_thread_detail(task->tid, "Error escribiendo datos");
            }
            pthread_mutex_unlock(&file_mutex);
            total_received += bytes;
        }
        
        // Mostrar progreso
        printf("\r[%s] %zu/%zu bytes", task->filename, total_received, content_length);
        fflush(stdout);
        
        if (content_length > 0 && total_received >= content_length) break;
    }

    // Limpiar recursos
    pthread_mutex_lock(&file_mutex);
    fclose(output_file);
    pthread_mutex_unlock(&file_mutex);
    close(sockfd);

    // Verificar integridad
    if (content_length > 0 && total_received != content_length) {
        log_thread_detail(task->tid, "ADVERTENCIA: Descarga incompleta (%zu/%zu bytes)", 
                         total_received, content_length);
    } else {
        log_thread_detail(task->tid, "Descarga exitosa: %zu bytes", total_received);
    }

    free(task);
    return NULL;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printf("Uso: %s <archivo1> [archivo2 ...]\n", argv[0]);
        return 1;
    }

    log_server("Iniciando cliente HTTP");
    log_server_detail("Servidor: 127.0.0.1:%d", PORT);
    log_server_detail("Archivos a descargar: %d", argc - 1);

    pthread_t threads[MAX_THREADS];
    int thread_count = 0;

    for (int i = 1; i < argc; i++) {
        if (thread_count >= MAX_THREADS) {
            log_server("Se alcanzó el máximo de hilos (%d)");
            break;
        }

        DownloadTask* task = malloc(sizeof(DownloadTask));
        strncpy(task->server_ip, "127.0.0.1", sizeof(task->server_ip));
        task->port = PORT;
        strncpy(task->filename, argv[i], sizeof(task->filename));

        if (pthread_create(&threads[thread_count], NULL, download_thread, task)) {
            log_server_detail("Error creando hilo: %s", strerror(errno));
            free(task);
        } else {
            thread_count++;
        }
    }

    // Esperar todos los hilos
    for (int i = 0; i < thread_count; i++) {
        pthread_join(threads[i], NULL);
    }

    log_server("Cliente terminado");
    log_server_detail("Archivos descargados: %d/%d", thread_count, argc - 1);
    
    return 0;
}