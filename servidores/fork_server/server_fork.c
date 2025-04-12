#include "common.h"
#include <signal.h>
#include <sys/wait.h>

void handle_client(int client_fd, struct sockaddr_in* client_addr) {
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(client_addr->sin_addr), client_ip, INET_ADDRSTRLEN);
    
    pid_t pid = getpid();
    log_process(pid, "Proceso hijo iniciado");
    log_process_detail(pid, "Atendiendo conexión desde %s:%d", 
                      client_ip, ntohs(client_addr->sin_port));

    char buffer[BUFFER_SIZE];
    int bytes = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
    if (bytes <= 0) {
        log_process_detail(pid, "Error al recibir datos: %s", 
                          bytes == 0 ? "Conexión cerrada" : strerror(errno));
        close(client_fd);
        exit(EXIT_FAILURE);
    }
    buffer[bytes] = '\0';

    log_process_detail(pid, "Solicitud recibida (%d bytes)", bytes);
    log_process_detail(pid, "Contenido: %.*s", 
                      bytes > 200 ? 200 : bytes, buffer);

    char method[16], path[256], protocol[16];
    if (sscanf(buffer, "%15s /%255s %15s", method, path, protocol) != 3) {
        log_process_detail(pid, "Solicitud mal formada");
        enviar_error(client_fd, 400, "Solicitud mal formada");
        log_request(client_ip, "BAD REQUEST", 400);
        close(client_fd);
        exit(EXIT_FAILURE);
    }

    log_process_detail(pid, "Solicitud parseada: %s %s %s", 
                      method, path, protocol);

    if (strcmp(method, "GET") != 0) {
        log_process_detail(pid, "Método no permitido: %s", method);
        enviar_error(client_fd, 405, "Método no permitido");
        log_request(client_ip, method, 405);
        close(client_fd);
        exit(EXIT_FAILURE);
    }

    if (!validar_nombre_archivo(path)) {
        log_process_detail(pid, "Nombre de archivo inválido: %s", path);
        enviar_error(client_fd, 403, "Nombre de archivo inválido");
        log_request(client_ip, path, 403);
        close(client_fd);
        exit(EXIT_FAILURE);
    }

    char full_path[MAX_PATH_LEN];
    snprintf(full_path, sizeof(full_path), "%s/%s", FILES_DIR, path);
    log_process_detail(pid, "Buscando archivo: %s", full_path);

    if (access(full_path, F_OK) == -1) {
        log_process_detail(pid, "Archivo no encontrado");
        enviar_error(client_fd, 404, "Archivo no encontrado");
        log_request(client_ip, path, 404);
    } else {
        FileInfo info = get_file_info(full_path);
        log_process_detail(pid, "Archivo encontrado - Tamaño: %zu bytes, Tipo: %s", 
                          info.size, info.content_type);
        
        enviar_archivo(client_fd, full_path);
        log_request(client_ip, path, 200);
        log_process_detail(pid, "Archivo enviado con éxito");
    }

    close(client_fd);
    log_process_detail(pid, "Proceso hijo finalizado");
    exit(EXIT_SUCCESS);
}

int main() {
    log_server("Iniciando servidor FORK");
    log_server_detail("Configuración: Puerto=%d, Directorio=%s", PORT, FILES_DIR);

    signal(SIGCHLD, SIG_IGN);
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

        pid_t pid = fork();
        if (pid == 0) {
            close(server_fd);
            handle_client(client_fd, &client_addr);
        } else if (pid > 0) {
            log_process_detail(pid, "Proceso hijo creado para conexión");
            close(client_fd);
        } else {
            log_server_detail("Error al crear proceso hijo: %s", strerror(errno));
            close(client_fd);
        }
    }

    close(server_fd);
    return 0;
}