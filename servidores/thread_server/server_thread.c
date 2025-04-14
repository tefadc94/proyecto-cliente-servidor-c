#include "common.h"
#include <time.h>
#include <sys/resource.h>
#include <pthread.h>

// Variables globales para métricas
static int conexiones_exitosas = 0;
static int conexiones_fallidas = 0;
static size_t total_bytes_enviados = 0;
static pthread_mutex_t metrics_mutex = PTHREAD_MUTEX_INITIALIZER;

typedef struct {
    int client_fd;
    struct sockaddr_in client_addr;
    pthread_t tid;
} ThreadData;

void* client_thread(void* arg) {
    ThreadData* data = (ThreadData*)arg;
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(data->client_addr.sin_addr), client_ip, INET_ADDRSTRLEN);
    
    log_thread(data->tid, "Nueva conexión aceptada");
    log_thread_detail(data->tid, "Cliente: %s:%d", 
                     client_ip, ntohs(data->client_addr.sin_port));

    char buffer[BUFFER_SIZE];
    int bytes = recv(data->client_fd, buffer, sizeof(buffer) - 1, 0);
    if (bytes <= 0) {
        log_thread_detail(data->tid, "Error al recibir datos: %s", 
                         bytes == 0 ? "Conexión cerrada" : strerror(errno));
        close(data->client_fd);
        free(data);

        // Incremenetar conexiones fallidas
        pthread_mutex_lock(&metrics_mutex);
        conexiones_fallidas++;
        pthread_mutex_unlock(&metrics_mutex);

        release_thread_slot();
        return NULL;
    }
    buffer[bytes] = '\0';

    log_thread_detail(data->tid, "Solicitud recibida (%d bytes)", bytes);
    log_thread_detail(data->tid, "Contenido: %.*s", 
                     bytes > 200 ? 200 : bytes, buffer);

    char method[16], path[256], protocol[16];
    if (sscanf(buffer, "%15s /%255s %15s", method, path, protocol) != 3) {
        log_thread_detail(data->tid, "Solicitud mal formada");
        enviar_error(data->client_fd, 400, "Solicitud mal formada");
        log_request(client_ip, "BAD REQUEST", 400);
        close(data->client_fd);
        free(data);

        // Incremenetar conexiones fallidas
        pthread_mutex_lock(&metrics_mutex); 
        conexiones_fallidas++;
        pthread_mutex_unlock(&metrics_mutex);

        release_thread_slot();
        return NULL;
    }

    log_thread_detail(data->tid, "Solicitud parseada: %s %s %s", 
                     method, path, protocol);

    if (strcmp(method, "GET") != 0) {
        log_thread_detail(data->tid, "Método no permitido: %s", method);
        enviar_error(data->client_fd, 405, "Método no permitido");
        log_request(client_ip, method, 405);
        close(data->client_fd);
        free(data);

        // Incremenetar conexiones fallidas
        pthread_mutex_lock(&metrics_mutex);
        conexiones_fallidas++;
        pthread_mutex_unlock(&metrics_mutex);

        release_thread_slot();
        return NULL;
    }

    if (!validar_nombre_archivo(path)) {
        log_thread_detail(data->tid, "Nombre de archivo inválido: %s", path);
        enviar_error(data->client_fd, 403, "Nombre de archivo inválido");
        log_request(client_ip, path, 403);
        close(data->client_fd);
        free(data);
        release_thread_slot();
        return NULL;
    }

    char full_path[MAX_PATH_LEN];
    snprintf(full_path, sizeof(full_path), "%s/%s", FILES_DIR, path);
    log_thread_detail(data->tid, "Buscando archivo: %s", full_path);

    if (access(full_path, F_OK) == -1) {
        log_thread_detail(data->tid, "Archivo no encontrado");
        enviar_error(data->client_fd, 404, "Archivo no encontrado");
        log_request(client_ip, path, 404);

        // Incrementar conexiones fallidas
        pthread_mutex_lock(&metrics_mutex);
        conexiones_fallidas++;
        pthread_mutex_unlock(&metrics_mutex);
    } else {
        FileInfo info = get_file_info(full_path);
        log_thread_detail(data->tid, "Archivo encontrado - Tamaño: %zu bytes, Tipo: %s", 
                         info.size, info.content_type);
        
        enviar_archivo(data->client_fd, full_path);

        // Incrementar bytes enviados y conexiones exitosas
        pthread_mutex_lock(&metrics_mutex);
        conexiones_exitosas++;
        pthread_mutex_unlock(&metrics_mutex);

        log_request(client_ip, path, 200);
        log_thread_detail(data->tid, "Archivo enviado con éxito");
    }

    close(data->client_fd);
    free(data);
    release_thread_slot();

    // Medir tiempo de respuesta
    clock_gettime(CLOCK_MONOTONIC, &end);
    double tiempo_respuesta = (end.tv_sec - start.tv_sec) * 1000.0 + 
                               (end.tv_nsec - start.tv_nsec) / 1000000.0;
    log_thread_detail(data->tid, "Tiempo de respuesta: %.2f ms", tiempo_respuesta);

    log_thread_detail(data->tid, "Conexión cerrada");
    return NULL;
}

int main() {
    init_thread_limit();
    log_server("Iniciando servidor THREAD");
    log_server_detail("Configuración: Puerto=%d, Hilos máx=%d, Directorio=%s", 
                     PORT, MAX_THREADS, FILES_DIR);

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        log_server_detail("Error al crear socket: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in server_addr = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = INADDR_ANY,
        .sin_port = htons(PORT)
    };

    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        log_server_detail("Error al vincular socket: %s", strerror(errno));
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, MAX_CONNECTIONS) < 0) {
        log_server_detail("Error al escuchar conexiones: %s", strerror(errno));
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    log_server("Servidor listo. Esperando conexiones...");
    log_server_detail("Socket: %d, Dirección: 0.0.0.0:%d", server_fd, PORT);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        log_server("Esperando nueva conexión...");
        int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
        if (client_fd < 0) {
            log_server_detail("Error al aceptar conexión: %s", strerror(errno));
            continue;
        }

        wait_thread_slot();
        ThreadData* data = malloc(sizeof(ThreadData));
        data->client_fd = client_fd;
        data->client_addr = client_addr;

        pthread_t tid;
        if (pthread_create(&tid, NULL, client_thread, data) != 0) {
            log_server_detail("Error al crear hilo: %s", strerror(errno));
            free(data);
            close(client_fd);
            release_thread_slot();
            continue;
        }

        data->tid = tid;
        pthread_detach(tid);
        log_thread_detail(tid, "Hilo creado para atender conexión");

        // Memoria máxima utilizada
        struct rusage usage;
        getrusage(RUSAGE_SELF, &usage);
        log_server_detail("Memoria máxima utilizada: %ld KB", usage.ru_maxrss);
    
        // Mostrar métricas acumuladas
        pthread_mutex_lock(&metrics_mutex);
        log_server_detail("Conexiones exitosas: %d", conexiones_exitosas);
        log_server_detail("Conexiones fallidas: %d", conexiones_fallidas);
        pthread_mutex_unlock(&metrics_mutex);
    }

    close(server_fd);
    return 0;
}