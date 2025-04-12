#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/wait.h>
#include <signal.h>

#define PORT 8080
#define BUFFER_SIZE 4096
#define FILES_DIR "./archivos/"

void handle_client(int client_socket);

int main() {
    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);

    signal(SIGCHLD, SIG_IGN); // evita procesos zombie

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    listen(server_fd, 10);
    printf("Servidor FORK escuchando en puerto %d...\n", PORT);

    while (1) {
        client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &addr_len);
        if (client_fd < 0) {
            perror("accept failed");
            continue;
        }

        pid_t pid = fork();
        if (pid == 0) { // proceso hijo
            close(server_fd);
            handle_client(client_fd);
            close(client_fd);
            exit(0);
        } else {
            close(client_fd); // el padre no lo necesita
        }
    }

    return 0;
}

void handle_client(int client_socket) {
    char buffer[BUFFER_SIZE], filepath[1024];
    int bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
    if (bytes_received <= 0) return;

    buffer[bytes_received] = '\0';
    snprintf(filepath, sizeof(filepath), "%s%s", FILES_DIR, buffer);

    FILE* file = fopen(filepath, "rb");
    if (!file) {
        char* error_msg = "HTTP/1.1 404 Not Found\r\n\r\nArchivo no encontrado.\n";
        send(client_socket, error_msg, strlen(error_msg), 0);
        return;
    }

    char* ok_header = "HTTP/1.1 200 OK\r\n\r\n";
    send(client_socket, ok_header, strlen(ok_header), 0);

    size_t n;
    while ((n = fread(buffer, 1, BUFFER_SIZE, file)) > 0) {
        send(client_socket, buffer, n, 0);
    }

   fclose(file);
}
