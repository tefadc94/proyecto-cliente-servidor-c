CC = gcc
CFLAGS = -Wall -Wextra -O2 -I.
LDFLAGS = -lpthread

# Objetivos principales
all: cliente_http server_fifo server_fork server_thread

# Cliente
cliente_http: cliente/cliente.c common.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Servidores
server_fifo: servidores/fifo_server/server_fifo.c common.c
	$(CC) $(CFLAGS) -o $@ $^

server_fork: servidores/fork_server/server_fork.c common.c
	$(CC) $(CFLAGS) -o $@ $^

server_thread: servidores/thread_server/server_thread.c common.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Limpieza
clean:
	rm -f cliente_http server_fifo server_fork server_thread

.PHONY: all clean