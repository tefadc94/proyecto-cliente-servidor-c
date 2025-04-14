#include "common.h"
#include <time.h> // Para medir el tiempo de respuesta
#include <sys/resource.h> // Para medir el uso de memoria

// Variables globales para métricas
static int conexiones_exitosas = 0;
static int conexiones_fallidas = 0;
static int total_bytes_enviados = 0;

void handle_client(int client_fd, struct sockaddr_in* client_addr) {
    struct timespec start, end; // Para medir el tiempo de respuesta
    clock_gettime(CLOCK_MONOTONIC, &start);

    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(client_addr->sin_addr), client_ip, INET_ADDRSTRLEN);
    
    log_server("Nueva conexión FIFO");
    log_server_detail("Cliente: %s:%d", client_ip, ntohs(client_addr->sin_port));

    char buffer[BUFFER_SIZE];
    ssize_t bytes = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
    if (bytes <= 0) {
        log_server_detail("Error recibiendo solicitud: %s", strerror(errno));
        close(client_fd);
        conexiones_fallidas++;
        return;
    }
    buffer[bytes] = '\0';

    log_server("Solicitud recibida");
    log_server_detail("Contenido inicial: %.*s", 200, buffer);

    char method[16], path[256], protocol[16];
    if (sscanf(buffer, "%15s /%255s %15s", method, path, protocol) != 3) {
        log_server("Solicitud mal formada");
        enviar_error(client_fd, 400, "Solicitud inválida");
        log_request(client_ip, "BAD REQUEST", 400);
        close(client_fd);
        conexiones_fallidas++;
        return;
    }

    log_server_detail("Método: %s", method);
    log_server_detail("Archivo solicitado: %s", path);
    log_server_detail("Protocolo: %s", protocol);

    if (strcasecmp(method, "GET") != 0) {
        log_server("Método no permitido");
        enviar_error(client_fd, 405, "Método no soportado");
        log_request(client_ip, method, 405);
        close(client_fd);
        conexiones_fallidas++;
        return;
    }

    if (!validar_nombre_archivo(path)) {
        log_server("Nombre de archivo inválido");
        enviar_error(client_fd, 403, "Acceso denegado");
        log_request(client_ip, path, 403);
        close(client_fd);
        conexiones_fallidas++;
        return;
    }

    char full_path[MAX_PATH_LEN];
    snprintf(full_path, sizeof(full_path), "%s/%s", FILES_DIR, path);
    log_server("Procesando archivo");
    log_server_detail("Ruta completa: %s", full_path);

    struct stat file_stat;
    if (stat(full_path, &file_stat) != 0) {
        log_server("Archivo no encontrado");
        enviar_error(client_fd, 404, "Archivo no existe");
        log_request(client_ip, path, 404);
        conexiones_fallidas++;
    } else {
        enviar_archivo(client_fd, full_path);
        log_request(client_ip, path, 200);
        log_server("Transferencia exitosa");
        conexiones_exitosas++;
    }

    close(client_fd);
    log_server("Conexión cerrada");

    // Medir el tiempo de respuesta
    clock_gettime(CLOCK_MONOTONIC, &end);
    double tiempo_respuesta = (end.tv_sec - start.tv_sec) * 1000.0 +
    (end.tv_nsec - start.tv_nsec) / 1000000.0;
    log_server_detail("Tiempo de respuesta: %.2f ms", tiempo_respuesta);
}

int main() {
    log_server("Iniciando servidor FIFO");
    log_server_detail("Configuración:");
    log_server_detail("  Puerto: %d", PORT);
    log_server_detail("  Directorio: %s", FILES_DIR);
    log_server_detail("  Timeout: %d segundos", SEND_TIMEOUT);

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        log_server_detail("Error creando socket: %s", strerror(errno));
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
        log_server_detail("Error vinculando socket: %s", strerror(errno));
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, MAX_CONNECTIONS) < 0) {
        log_server_detail("Error escuchando conexiones: %s", strerror(errno));
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    log_server("Servidor listo");
    log_server_detail("Escuchando en 0.0.0.0:%d", PORT);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        log_server("Esperando conexión...");
        int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
        if (client_fd < 0) {
            log_server_detail("Error aceptando conexión: %s", strerror(errno));
            continue;
        }

        handle_client(client_fd, &client_addr);

        // Medir memoria máxim utilizada
        struct rusage usage;
        getrusage(RUSAGE_SELF, &usage);
        log_server_detail("Memoria máxima utilizada: %ld KB", usage.ru_maxrss);
        
        // Mostrar métricas acumuladas
        log_server_detail("Conexiones exitosas: %d", conexiones_exitosas);
        log_server_detail("Conexiones fallidas: %d", conexiones_fallidas);
        log_server_detail("Total de datos enviados: %.2f MB", total_bytes_enviados / (1024.0 * 1024.0));
    }

    close(server_fd);
    return 0;
}