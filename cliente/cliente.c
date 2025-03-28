#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>

#define PUERTO 8080
#define TAM_BUFFER 4096

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Uso: %s <IP_SERVIDOR> <ARCHIVO_A_SOLICITAR>\n", argv[0]);
        exit(1);
    }

    // Crear socket
    int cliente_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (cliente_socket < 0) {
        perror("[ERROR] No se pudo crear el socket");
        exit(1);
    }

    // Configurar dirección del servidor
    struct sockaddr_in direccion_servidor;
    direccion_servidor.sin_family = AF_INET;
    direccion_servidor.sin_port = htons(PUERTO);
    inet_pton(AF_INET, argv[1], &direccion_servidor.sin_addr);

    // Conectar al servidor
    if (connect(cliente_socket, (struct sockaddr*)&direccion_servidor, sizeof(direccion_servidor)) < 0) {
        perror("[ERROR] Fallo al conectar");
        exit(1);
    }

    // Enviar solicitud GET (ej: GET /archivo.txt HTTP/1.1)
    char solicitud[TAM_BUFFER];
    snprintf(solicitud, sizeof(solicitud), "GET /%s HTTP/1.1\r\n\r\n", argv[2]);
    send(cliente_socket, solicitud, strlen(solicitud), 0);

    // Crear carpeta 'descargas' si no existe
    mkdir("descargas", 0700);  // Permisos: rwx para el dueño

    // Guardar el archivo recibido
    char ruta_local[TAM_BUFFER];
    snprintf(ruta_local, sizeof(ruta_local), "descargas/%s", argv[2]);
    
    FILE *archivo = fopen(ruta_local, "wb");
    if (!archivo) {
        perror("[ERROR] No se pudo crear el archivo local");
        exit(1);
    }

    // Leer respuesta del servidor y guardar
    char buffer[TAM_BUFFER];
    int bytes_leidos;
    while ((bytes_leidos = recv(cliente_socket, buffer, TAM_BUFFER, 0)) > 0) {
        fwrite(buffer, 1, bytes_leidos, archivo);
    }

    fclose(archivo);
    close(cliente_socket);

    printf("[ÉXITO] Archivo guardado en: %s\n", ruta_local);
    return 0;
}