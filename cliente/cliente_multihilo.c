#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 8080
#define BUFFER_SIZE 4096

void* solicitar_archivo(void* arg);

typedef struct {
    char archivo[256];
} ArchivoArgs;

int main() {
    char input[1024];
    printf("Ingrese nombres de archivos separados por coma: ");
    fgets(input, sizeof(input), stdin);

    char* token = strtok(input, ",\n");
    pthread_t hilos[100];
    int count = 0;

    while (token && count < 100) {
        ArchivoArgs* args = malloc(sizeof(ArchivoArgs));
        strcpy(args->archivo, token);
        pthread_create(&hilos[count++], NULL, solicitar_archivo, args);
        token = strtok(NULL, ",\n");
    }

    for (int i = 0; i < count; i++) {
        pthread_join(hilos[i], NULL);
    }

    return 0;
}

void* solicitar_archivo(void* arg) {
    ArchivoArgs* data = (ArchivoArgs*)arg;
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        free(data);
        return NULL;
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr);

    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect");
        free(data);
        close(sock);
        return NULL;
    }

    send(sock, data->archivo, strlen(data->archivo), 0);

    char buffer[BUFFER_SIZE];
    snprintf(buffer, sizeof(buffer), "descargas/%s", data->archivo);
    FILE* f = fopen(buffer, "wb");

    int bytes;
    while ((bytes = recv(sock, buffer, BUFFER_SIZE, 0)) > 0) {
        fwrite(buffer, 1, bytes, f);
    }

    printf("Archivo \"%s\" recibido.\n", data->archivo);
    fclose(f);
    close(sock);
    free(data);
    return NULL;
}
