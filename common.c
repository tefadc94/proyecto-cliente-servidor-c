// ----------------------------
// common.c - Funciones útiles para servidores y cliente
// ----------------------------

#include "common.h"

// Lee un archivo y devuelve su contenido en memoria (o NULL si falla)
char* leer_archivo(const char* ruta, long* tamano) {
    FILE* archivo = fopen(ruta, "rb");
    if (!archivo) return NULL;

    // Calcula tamaño del archivo
    fseek(archivo, 0, SEEK_END);
    *tamano = ftell(archivo);
    fseek(archivo, 0, SEEK_SET);

    // Reserva memoria y lee
    char* buffer = malloc(*tamano);
    if (!buffer) {
        fclose(archivo);
        return NULL;
    }
    fread(buffer, 1, *tamano, archivo);
    fclose(archivo);
    
    return buffer;
}

// Envía una respuesta HTTP 404 (archivo no encontrado)
void enviar_error_404(int socket_cliente) {
    char respuesta[] = 
        "HTTP/1.1 404 Not Found\r\n"
        "Content-Type: text/html\r\n\r\n"
        "<html><body><h1>404 - Archivo no encontrado</h1></body></html>";
    
    send(socket_cliente, respuesta, strlen(respuesta), 0);
}

// Envía un archivo con cabeceras HTTP correctas
void enviar_archivo(int socket_cliente, const char* ruta) {
    long tamano;
    char* datos = leer_archivo(ruta, &tamano);
    if (!datos) {
        enviar_error_404(socket_cliente);
        return;
    }

    // Cabeceras HTTP
    char cabecera[BUFFER_SIZE];
    snprintf(cabecera, sizeof(cabecera), 
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: application/octet-stream\r\n"
        "Content-Length: %ld\r\n\r\n",  // Usamos octet-stream para cualquier tipo de archivo
        tamano);
    
    send(socket_cliente, cabecera, strlen(cabecera), 0);  // Envía cabeceras
    send(socket_cliente, datos, tamano, 0);               // Envía contenido
    free(datos);
}