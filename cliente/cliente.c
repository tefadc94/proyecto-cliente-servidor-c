#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <sys/types.h>

#define BUFFER_SIZE 65536 // 64 KB

void descargar_archivo(const char *ip, int puerto, const char *archivo) {
    int sock;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];

    // Crear socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Error al crear el socket");
        return;
    }

    // Configurar tiempo de espera para el socket
    struct timeval timeout;
    timeout.tv_sec = 10; // 10 segundos
    timeout.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(puerto);

    // Convertir dirección IP
    if (inet_pton(AF_INET, ip, &server_addr.sin_addr) <= 0) {
        perror("Dirección IP no válida");
        close(sock);
        return;
    }

    // Conectar al servidor
    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Error al conectar al servidor");
        close(sock);
        return;
    }

    // Enviar solicitud HTTP
    snprintf(buffer, sizeof(buffer), "GET /%s HTTP/1.0\r\n\r\n", archivo);
    send(sock, buffer, strlen(buffer), 0);

    // Crear la carpeta "descargas" si no existe
    struct stat st = {0};
    if (stat("../descargas", &st) == -1) {
        if (mkdir("../descargas", 0700) != 0) {
            perror("Error al crear el directorio descargas");
            close(sock);
            return;
        }
    }

    // Construir la ruta completa para guardar el archivo
    char ruta_completa[BUFFER_SIZE];
    snprintf(ruta_completa, sizeof(ruta_completa), "../descargas/%s", archivo);

    // Abrir archivo para escritura en modo binario
    FILE *fp = fopen(ruta_completa, "wb");
    if (!fp) {
        perror("Error al abrir el archivo para escritura");
        close(sock);
        return;
    }

    // Leer respuesta del servidor
    int bytes_received = recv(sock, buffer, sizeof(buffer) - 1, 0);
    if (bytes_received <= 0) {
        perror("Error al recibir datos del servidor");
        fclose(fp);
        close(sock);
        return;
    }

    // Procesar encabezados HTTP
    char *body = strstr(buffer, "\r\n\r\n");
    if (body) {
        body += 4; // Salta los caracteres "\r\n\r\n"
        fwrite(body, 1, bytes_received - (body - buffer), fp);
    } else {
        perror("No se encontraron los encabezados HTTP");
        fclose(fp);
        close(sock);
        return;
    }

    // Recibir y escribir el resto del archivo
    size_t total_bytes_received = bytes_received - (body - buffer);
    printf("\rBytes descargados: %zu", total_bytes_received);
    fflush(stdout);

    while ((bytes_received = recv(sock, buffer, sizeof(buffer), 0)) > 0) {
        if (fwrite(buffer, 1, bytes_received, fp) != bytes_received) {
            perror("Error al escribir en el archivo");
            break;
        }
        total_bytes_received += bytes_received;
        printf("\rBytes descargados: %zu", total_bytes_received);
        fflush(stdout);
    }

    if (bytes_received < 0) {
        perror("Error al recibir datos del servidor");
    }

    printf("\n");

    fclose(fp);
    close(sock);
    printf("Archivo %s descargado correctamente en descargas.\n", archivo);
}

int main() {
    const char *ip = "127.0.0.1";
    int puerto = 8080;

    // Solicitar al usuario los nombres de los archivos
    char archivos[BUFFER_SIZE];
    printf("Introduce los nombres de los archivos (separados por comas): ");
    fgets(archivos, sizeof(archivos), stdin);

    // Eliminar el salto de línea al final de la entrada
    archivos[strcspn(archivos, "\n")] = '\0';

    // Dividir los nombres de los archivos por comas
    char *archivo = strtok(archivos, ",");
    while (archivo != NULL) {
        // Eliminar espacios al inicio y al final del nombre del archivo
        while (*archivo == ' ') archivo++;
        char *end = archivo + strlen(archivo) - 1;
        while (end > archivo && *end == ' ') *end-- = '\0';

        // Descargar el archivo
        descargar_archivo(ip, puerto, archivo);

        // Siguiente archivo
        archivo = strtok(NULL, ",");
    }

    return 0;
}