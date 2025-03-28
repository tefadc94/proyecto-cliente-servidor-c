// ----------------------------
// server.c - Servidor FIFO (atiende una solicitud a la vez)
// ----------------------------

#include "../../common.h"
#include <stdlib.h>     // exit

int main() {
    int servidor_fd, cliente_socket;
    struct sockaddr_in direccion;
    socklen_t largo_direccion = sizeof(direccion);  // Tipo correcto

    // Crear socket
    if ((servidor_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("[ERROR] Socket");
        exit(1);
    }

    // Configurar dirección
    direccion.sin_family = AF_INET;          // Familia IPv4
    direccion.sin_addr.s_addr = INADDR_ANY;  // Escuchar en todas las interfaces
    direccion.sin_port = htons(PORT);  // Puerto en formato red

    // Vincular socket al puerto
    if (bind(servidor_fd, (struct sockaddr*)&direccion, sizeof(direccion)) < 0) {
        perror("[ERROR] Fallo al vincular socket");
        exit(1);
    }

    // Escuchar conexiones entrantes (máximo 10 en cola)
    if (listen(servidor_fd, 10) < 0) {
        perror("[ERROR] Fallo al escuchar");
        exit(1);
    }

    printf("[FIFO] Servidor listo en el puerto %d...\n", PORT);

    // Bucle principal: Atiende una solicitud a la vez
    while (1) {
        // Aceptar nueva conexión
        if ((cliente_socket = accept(servidor_fd, (struct sockaddr*)&direccion, (socklen_t*)&largo_direccion)) < 0) {
            perror("[ERROR] Fallo al aceptar conexión");
            continue;  // Si falla, sigue intentando
        }

        // Leer solicitud del cliente (ej: GET /archivo.txt HTTP/1.1)
        char buffer[BUFFER_SIZE] = {0};
        read(cliente_socket, buffer, BUFFER_SIZE);

        // Extraer nombre del archivo de la solicitud
        char* token = strtok(buffer, " ");
        if (token == NULL || strcmp(token, "GET") != 0) {
            enviar_error_404(cliente_socket);
            close(cliente_socket);
            continue;
        }

        token = strtok(NULL, " ");  // Obtener la ruta del archivo
        char ruta_completa[256];
        snprintf(ruta_completa, sizeof(ruta_completa), "%s%s", FILES_DIR, token);

        // Enviar archivo o error
        enviar_archivo(cliente_socket, ruta_completa);
        close(cliente_socket);  // Cierra la conexión después de responder
    }

    return 0;
}