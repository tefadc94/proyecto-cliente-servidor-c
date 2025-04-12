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
    
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        log_thread_detail(task->tid, "Error creando socket: %s", strerror(errno));
        free(task);
        return NULL;
    }

    struct timeval tv = {TIMEOUT_SEC, 0};
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    struct sockaddr_in server_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(task->port)
    };
    
    if (inet_pton(AF_INET, task->server_ip, &server_addr.sin_addr) <= 0) {
        log_thread_detail(task->tid, "IP inválida: %s", task->server_ip);
        close(sockfd);
        free(task);
        return NULL;
    }

    if (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        log_thread_detail(task->tid, "Error de conexión: %s", strerror(errno));
        close(sockfd);
        free(task);
        return NULL;
    }

    char request[1024];
    snprintf(request, sizeof(request),
        "GET /%s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "Connection: close\r\n\r\n",
        task->filename, task->server_ip);

    if (send(sockfd, request, strlen(request), 0) < 0) {
        log_thread_detail(task->tid, "Error enviando solicitud: %s", strerror(errno));
        close(sockfd);
        free(task);
        return NULL;
    }

    // Procesar respuesta
    char response[BUFFER_SIZE];
    size_t total_received = 0;
    size_t content_length = 0;
    int header_processed = 0;
    FILE* output_file = NULL;
    
    mkdir("descargas", 0755);
    char output_path[MAX_PATH_LEN];
    snprintf(output_path, sizeof(output_path), "descargas/%s", basename(task->filename));

    while (1) {
        ssize_t bytes = recv(sockfd, response, sizeof(response), 0);
        if (bytes <= 0) break;

        if (!header_processed) {
            char *header_end = strstr(response, "\r\n\r\n");
            if (header_end) {
                size_t header_size = header_end - response + 4;
                size_t body_size = bytes - header_size;
                if (body_size > 0) {
                    pthread_mutex_lock(&file_mutex);
                    fwrite(header_end + 4, 1, body_size, output_file);
                    pthread_mutex_unlock(&file_mutex);
                }
                header_processed = 1;
            }
        } else {
            pthread_mutex_lock(&file_mutex);
            if (output_file) {
                fwrite(response, 1, bytes, output_file);
                total_received += bytes;
            }
            pthread_mutex_unlock(&file_mutex);
        }
        
        printf("\r  Progreso: %zu bytes", total_received);
        fflush(stdout);
        
        if (content_length > 0 && total_received >= content_length) break;
    }

    pthread_mutex_lock(&file_mutex);
    if (output_file) fclose(output_file);
    pthread_mutex_unlock(&file_mutex);

    close(sockfd);
    
    log_thread_detail(task->tid, "Descarga finalizada");
    log_thread_detail(task->tid, "Archivo: %s", output_path);
    log_thread_detail(task->tid, "Bytes recibidos: %zu", total_received);
    
    free(task);
    return NULL;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printf("Uso: %s <archivo1> [archivo2 ...]\n", argv[0]);
        return 1;
    }

    log_server("Iniciando cliente HTTP");
    log_server_detail("Configuración:");
    log_server_detail("  Servidor: 127.0.0.1:%d", PORT);
    log_server_detail("  Máximo de hilos: %d", MAX_THREADS);
    log_server_detail("  Archivos a descargar: %d", argc - 1);

    pthread_t threads[MAX_THREADS];
    int thread_count = 0;

    for (int i = 1; i < argc && thread_count < MAX_THREADS; i++) {
        DownloadTask* task = malloc(sizeof(DownloadTask));
        snprintf(task->server_ip, sizeof(task->server_ip), "127.0.0.1");
        task->port = PORT;
        snprintf(task->filename, sizeof(task->filename), "%s", argv[i]);

        if (pthread_create(&threads[thread_count], NULL, download_thread, task)) {
            log_server_detail("Error al crear hilo: %s", strerror(errno));
            free(task);
        } else {
            task->tid = threads[thread_count];
            thread_count++;
            log_thread_detail(task->tid, "Hilo creado para descarga");
            log_thread_detail(task->tid, "Archivo a descargar: %s", task->filename);
        }
    }

    for (int i = 0; i < thread_count; i++) {
        pthread_join(threads[i], NULL);
        log_thread_detail(threads[i], "Hilo finalizado");
    }

    log_server("Cliente terminado");
    log_server_detail("Total de archivos descargados: %d", thread_count);
    return 0;
}