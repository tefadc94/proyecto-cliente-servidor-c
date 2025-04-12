#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h> // Para manejar hilos
#include <netinet/in.h> // Para estructuras de red como sockaddr_in

#define BUFFER_SIZE 65536 // 64 KB
#define PORT 8080 // Puerto en el que escuchará el servidor

void *handle_client(void *arg) {
    int client_fd = *((int*)arg);
    free(arg);

    char buffer[BUFFER_SIZE];
    char archivo[256];

    // Leer solicitud del cliente
    int bytes_received = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
    if (bytes_received <= 0) {
        perror("[ERROR] Error al recibir datos del cliente");
        close(client_fd);
        return NULL;
    }

    buffer[bytes_received] = '\0';

    // Procesar solicitud HTTP
    if (sscanf(buffer, "GET /%255s", archivo) != 1) {
        const char *msg = "HTTP/1.0 400 Bad Request\r\n\r\nSolicitud malformada";
        send(client_fd, msg, strlen(msg), 0);
        close(client_fd);
        return NULL;
    }

    // Construir la ruta del archivo
    char filepath[512];
    snprintf(filepath, sizeof(filepath), "../../archivos/%s", archivo);

    FILE *fp = fopen(filepath, "rb");
    if (!fp) {
        perror("[ERROR] No se pudo abrir el archivo solicitado");
        const char *msg = "HTTP/1.0 404 Not Found\r\n\r\nArchivo no encontrado";
        send(client_fd, msg, strlen(msg), 0);
        close(client_fd);
        return NULL;
    }

    // Determinar el tipo de contenido
    char content_type[64];
    if (strstr(archivo, ".html")) {
        strcpy(content_type, "text/html");
    } else if (strstr(archivo, ".txt")) {
        strcpy(content_type, "text/plain");
    } else if (strstr(archivo, ".jpg")) {
        strcpy(content_type, "image/jpeg");
    } else if (strstr(archivo, ".png")) {
        strcpy(content_type, "image/png");
    } else {
        strcpy(content_type, "application/octet-stream");
    }

    // Enviar encabezado HTTP
    char header[BUFFER_SIZE];
    snprintf(header, sizeof(header), "HTTP/1.0 200 OK\r\nContent-Type: %s\r\n\r\n", content_type);
    if (send(client_fd, header, strlen(header), 0) == -1) {
        perror("[ERROR] Error al enviar el encabezado HTTP");
        fclose(fp);
        close(client_fd);
        return NULL;
    }

    // Enviar el contenido del archivo
    int n;
    while ((n = fread(buffer, 1, BUFFER_SIZE, fp)) > 0) {
        if (send(client_fd, buffer, n, 0) == -1) {
            perror("[ERROR] Error al enviar el contenido del archivo");
            break;
        }
    }

    fclose(fp);
    close(client_fd);
    printf("[FINALIZADO] Archivo enviado correctamente.\n");
    return NULL;
}

int main() {
    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);

    // Crear socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("[ERROR] No se pudo crear el socket");
        exit(EXIT_FAILURE);
    }

    // Configurar dirección del servidor
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    // Enlazar socket
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("[ERROR] Error al enlazar el socket");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // Escuchar conexiones
    if (listen(server_fd, 10) == -1) {
        perror("[ERROR] Error al escuchar en el socket");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("[INICIO] Servidor escuchando en el puerto %d\n", PORT);

    while (1) {
        // Aceptar conexión de cliente
        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd == -1) {
            perror("[ERROR] Error al aceptar conexión");
            continue;
        }

        printf("[CONEXIÓN] Cliente conectado\n");

        // Crear un hilo para manejar al cliente
        pthread_t thread_id;
        int *client_fd_ptr = malloc(sizeof(int));
        *client_fd_ptr = client_fd;
        if (pthread_create(&thread_id, NULL, handle_client, client_fd_ptr) != 0) {
            perror("[ERROR] No se pudo crear el hilo");
            free(client_fd_ptr); // Libera memoria si no se puede crear el hilo
            close(client_fd);
        }

        // Separar el hilo para que se limpie automáticamente al terminar
        pthread_detach(thread_id);
    }

    close(server_fd);
    return 0;
}