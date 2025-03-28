// ----------------------------
// common.h - Funciones compartidas
// ----------------------------

#ifndef COMMON_H
#define COMMON_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>  // Para sockets y constantes
#include <netinet/in.h>   // Para struct sockaddr_in
#include <arpa/inet.h>    // Para funciones de red
#include <sys/stat.h>

// Constantes globales
#define PORT 8080          // Puerto del servidor
#define BUFFER_SIZE 4096   // Tamaño para leer/escribir datos
#define FILES_DIR "./archivos"  // Carpeta con archivos a servir

// Funciones útiles para todos los componentes
char* leer_archivo(const char* ruta, long* tamano);  // Lee un archivo en bytes
void enviar_error_404(int socket_cliente);           // Manda error 404 al cliente
void enviar_archivo(int socket_cliente, const char* ruta);  // Envía archivo + cabeceras HTTP

#endif