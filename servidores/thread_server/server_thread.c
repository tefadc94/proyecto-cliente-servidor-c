#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <fcntl.h>

#define PORT 8080
#define BUFFER_SIZE 1024

void *handle_client(void *arg);

int main() {
    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);
    pthread_t tid;

    // Crear socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("Error al crear el socket");
        exit(1);
    }

    // Configurar dirección del servidor
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    // Asociar socket con la dirección
    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        perror("Error en bind");
        close(server_fd);
        exit(1);
    }

    // Escuchar conexiones
    if (listen(server_fd, 10) == -1) {
        perror("Error en listen");
        close(server_fd);
        exit(1);
    }

    printf("[SERVIDOR] Escuchando en el puerto %d...\n", PORT);

    while (1) {
        client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &addr_len);
        if (client_fd == -1) {
            perror("Error en accept");
            continue;
        }

        int *pclient = malloc(sizeof(int));
        *pclient = client_fd;

        if (pthread_create(&tid, NULL, handle_client, pclient) != 0) {
            perror("Error al crear hilo");
            close(client_fd);
            free(pclient);
        }

        pthread_detach(tid); // Liberar recursos del hilo automáticamente
    }

    close(server_fd);
    return 0;
}

void *handle_client(void *arg) {
    int client_fd = *((int*)arg);
    free(arg);

    char buffer[BUFFER_SIZE];
    int bytes_received = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
    if (bytes_received <= 0) {
        close(client_fd);
        return NULL;
    }

    buffer[bytes_received] = '\0';

    // Buscar línea con GET
    char archivo[256];
    if (sscanf(buffer, "GET /%255s", archivo) != 1) {
        char *msg = "HTTP/1.0 400 Bad Request\r\n\r\nSolicitud malformada";
        send(client_fd, msg, strlen(msg), 0);
        close(client_fd);
        return NULL;
    }

    // Eliminar espacios o parámetros
    for (int i = 0; archivo[i]; ++i) {
        if (archivo[i] == ' ' || archivo[i] == '?') {
            archivo[i] = '\0';
            break;
        }
    }

    printf("[SOLICITUD] Cliente pidió: %s\n", archivo);

    char filepath[512];
    snprintf(filepath, sizeof(filepath), "archivos/%s", archivo);

    FILE *fp = fopen(filepath, "rb");
    if (!fp) {
        char *msg = "HTTP/1.0 404 Not Found\r\n\r\nArchivo no encontrado";
        send(client_fd, msg, strlen(msg), 0);
        close(client_fd);
        return NULL;
    }

    char *ok = "HTTP/1.0 200 OK\r\n\r\n";
    send(client_fd, ok, strlen(ok), 0);

    int n;
    while ((n = fread(buffer, 1, BUFFER_SIZE, fp)) > 0) {
        send(client_fd, buffer, n, 0);
    }

    fclose(fp);
    close(client_fd);
    printf("[FINALIZADO] Archivo enviado correctamente.\n");
    return NULL;
}
