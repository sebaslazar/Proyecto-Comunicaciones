#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <poll.h>

// Configuración
#define PORT 8080
#define BUF_SIZE 1024
#define MAX_CLIENTS 100
#define NAME_LEN 32

// Estado del cliente
#define STATE_NAME 0     // Esperando nombre de usuario
#define STATE_CHOOSING 1 // Esperando elegir interlocutor
#define STATE_CHATTING 2 // Chat activo

typedef struct
{
    int fd;              // Descriptor de socket
    char name[NAME_LEN]; // Nombre del usuario
    int state;           // Estado actual
    int peer_idx;        // Índice del cliente emparejado, o -1
} client_t;

// Envia al cliente i la lista de usuarios disponibles y el prompt de elección
void send_user_list(int i, struct pollfd fds[], client_t clients[])
{
    char buf[BUF_SIZE];
    int len = snprintf(buf, sizeof(buf), "Usuarios conectados: \n");

    for (int j = 1; j <= MAX_CLIENTS; j++)
    {
        if (j == i)
            continue;
        if (clients[j].fd >= 0 && clients[j].state >= STATE_CHOOSING)
        {
            len += snprintf(buf + len, sizeof(buf) - len, " - %s\n", clients[j].name);
        }
    }

    len += snprintf(buf + len, sizeof(buf) - len,
                    "Escribe el nombre del usuario con quien chatear: \n");
    send(clients[i].fd, buf, len, 0);
}

// Función para enviar la lista de usuarios conectados (Mejora nivel básico)
void send_user_list_to_client(int client_idx, struct pollfd fds[], client_t clients[])
{
    char buf[BUF_SIZE];
    int len = snprintf(buf, sizeof(buf), "=== Usuarios Conectados ===\n");

    for (int j = 1; j <= MAX_CLIENTS; j++)
    {
        if (j == client_idx)
            continue; // Saltar el propio cliente
        if (clients[j].fd >= 0 && clients[j].state >= STATE_CHOOSING)
        {
            char status[20];
            if (clients[j].state == STATE_CHATTING)
            {
                strcpy(status, "[En chat]");
            }
            else
            {
                strcpy(status, "[Disponible]");
            }
            len += snprintf(buf + len, sizeof(buf) - len,
                            "- %s %s\n", clients[j].name, status);
        }
    }

    len += snprintf(buf + len, sizeof(buf) - len,
                    "=== Total: %d usuarios ===\n", count_connected_users(clients));

    send(clients[client_idx].fd, buf, len, 0);
}

// Función auxiliar para contar usuarios conectados
int count_connected_users(client_t clients[])
{
    int count = 0;
    for (int i = 1; i <= MAX_CLIENTS; i++)
    {
        if (clients[i].fd >= 0 && clients[i].state >= STATE_CHOOSING)
        {
            count++;
        }
    }
    return count;
}

int main()
{
    int listen_fd, nfds;
    struct sockaddr_in addr;
    struct pollfd fds[MAX_CLIENTS + 1];
    client_t clients[MAX_CLIENTS + 1];
    char buffer[BUF_SIZE];

    // 1. Crear y configurar socket de escucha
    listen_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (listen_fd < 0)
    {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);

    if (bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("bind");
        close(listen_fd);
        exit(EXIT_FAILURE);
    }

    if (listen(listen_fd, 5) < 0)
    {
        perror("listen");
        close(listen_fd);
        exit(EXIT_FAILURE);
    }

    printf("Servidor uno-a-uno en poll() escuchando puerto %d\n", PORT);

    // 2. Inicializar fds y clients
    fds[0].fd = listen_fd;
    fds[0].events = POLLIN;

    for (int i = 1; i <= MAX_CLIENTS; i++)
    {
        fds[i].fd = -1;
        clients[i].fd = -1;
        clients[i].state = -1;
        clients[i].peer_idx = -1;
    }

    // 3. Bucle principal
    while (1)
    {
        nfds = poll(fds, MAX_CLIENTS + 1, -1);

        if (nfds < 0)
        {
            perror("poll");
            break;
        }

        // 3a. Nueva conexión entrante
        if (fds[0].revents & POLLIN)
        {
            int conn_fd = accept(listen_fd, NULL, NULL);

            if (conn_fd < 0)
            {
                perror("accept");
            }
            else
            {
                // buscar ranura libre
                int i;

                for (i = 1; i <= MAX_CLIENTS; i++)
                {
                    if (fds[i].fd < 0)
                    {
                        fds[i].fd = conn_fd;
                        fds[i].events = POLLIN;
                        clients[i].fd = conn_fd;
                        clients[i].state = STATE_NAME;
                        clients[i].peer_idx = -1;
                        const char *msg = "Bienvenido. Ingresa tu nombre: \n";
                        send(conn_fd, msg, strlen(msg), 0);
                        printf("Nuevo cliente fd=%d asignado al slot %d\n", conn_fd, i);
                        break;
                    }
                }

                if (i > MAX_CLIENTS)
                {
                    const char *msg = "Servidor lleno. Intenta mas tarde.\n";
                    send(conn_fd, msg, strlen(msg), 0);
                    close(conn_fd);
                }
            }

            if (--nfds == 0)
                continue;
        }

        // 3b. Manejo de actividad en clientes existentes
        for (int i = 1; i <= MAX_CLIENTS; i++)
        {
            if (fds[i].fd < 0)
                continue;
            if (!(fds[i].revents & POLLIN))
                continue;

            int fd = fds[i].fd;
            ssize_t n = recv(fd, buffer, BUF_SIZE - 1, 0);

            if (n <= 0)
            {
                // desconexión o error
                printf("Cliente %s (fd=%d) desconectado\n", clients[i].name, fd);
                int peer = clients[i].peer_idx;

                if (peer > 0 && clients[peer].state == STATE_CHATTING)
                {
                    // notificar al peer
                    const char *msg = "El otro usuario se desconecto.\n";
                    send(clients[peer].fd, msg, strlen(msg), 0);
                    // volver a elegir interlocutor
                    clients[peer].state = STATE_CHOOSING;
                    clients[peer].peer_idx = -1;
                    send_user_list(peer, fds, clients);
                }
                // limpiar slot i
                close(fd);
                fds[i].fd = -1;
                clients[i].fd = -1;
                clients[i].state = -1;
                clients[i].peer_idx = -1;
                if (--nfds == 0)
                    break;

                continue;
            }

            buffer[n] = '\0';
            // eliminar posible '\r' y '\n'
            while (n > 0 && (buffer[n - 1] == '\n' || buffer[n - 1] == '\r'))
            {
                buffer[--n] = '\0';
            }
            switch (clients[i].state)
            {
            case STATE_NAME:
                // guardar nombre y pasar a choosing
                strncpy(clients[i].name, buffer, NAME_LEN - 1);
                clients[i].name[NAME_LEN - 1] = '\0';
                clients[i].state = STATE_CHOOSING;
                printf("Cliente fd=%d se registro como '%s'\n", fd, clients[i].name);
                send_user_list(i, fds, clients);

                // Notificar a otros clientes que están esperando elegir (STATE_CHOOSING)
                for (int k = 1; k <= MAX_CLIENTS; k++)
                {
                    if (k == i)
                        continue;
                    if (clients[k].fd >= 0 && clients[k].state == STATE_CHOOSING)
                    {
                        send_user_list(k, fds, clients);
                    }
                }

                break;

            case STATE_CHOOSING:
            {
                // Verifica si se ingresó el comando /list
                if (strcmp(buffer, "/list") == 0)
                {
                    send_user_list_to_client(i, fds, clients);
                    break;
                }

                // buscar peer por nombre
                int found = 0;

                for (int j = 1; j <= MAX_CLIENTS; j++)
                {
                    if (j == i)
                        continue;
                    if (clients[j].fd >= 0 && clients[j].state == STATE_CHOOSING && strcmp(clients[j].name, buffer) == 0)
                    {
                        // emparejar i y j
                        clients[i].peer_idx = j;
                        clients[j].peer_idx = i;
                        clients[i].state = STATE_CHATTING;
                        clients[j].state = STATE_CHATTING;
                        const char *msg_i = "Chat iniciado. Usa /exit para salir.\n";
                        const char *msg_j = "Chat iniciado. Usa /exit para salir.\n";
                        send(clients[i].fd, msg_i, strlen(msg_i), 0);
                        send(clients[j].fd, msg_j, strlen(msg_j), 0);
                        printf("Emparejados '%s' <--> '%s'\n", clients[i].name, clients[j].name);
                        found = 1;
                        break;
                    }
                }

                if (!found)
                {
                    const char *msg = "Usuario no disponible. Elige otro o usa /list para ver usuarios:\n";
                    send(fd, msg, strlen(msg), 0);
                    // send_user_list(i, fds, clients); (Descomentar para mostrar la lista simplificada)
                }

                break;
            }

            case STATE_CHATTING:
            {
                int peer = clients[i].peer_idx;

                // Verifica si se ingresó el comando /list
                if (strcmp(buffer, "/list") == 0) {
                    const char *msg = "Usa /exit para salir del chat actual y ver la lista.\n";
                    send(fd, msg, strlen(msg), 0);
                    break;
                }

                if (strcmp(buffer, "/exit") == 0)
                {
                    // romper emparejamiento
                    const char *msg_exit = "Has salido del chat.\n";
                    send(fd, msg_exit, strlen(msg_exit), 0);

                    if (peer > 0)
                    {
                        const char *msg_peer = "El otro usuario salio del chat.\n";
                        send(clients[peer].fd, msg_peer, strlen(msg_peer), 0);
                        clients[peer].state = STATE_CHOOSING;
                        clients[peer].peer_idx = -1;
                        send_user_list(peer, fds, clients);
                    }

                    clients[i].state = STATE_CHOOSING;
                    clients[i].peer_idx = -1;
                    send_user_list(i, fds, clients);
                    printf("Chat terminado para '%s'\n", clients[i].name);
                }
                else
                {
                    // reenviar mensaje al peer
                    if (peer > 0 && clients[peer].fd >= 0)
                    {
                        char forward[BUF_SIZE];
                        int L = snprintf(forward, sizeof(forward), "%s: %s\n", clients[i].name, buffer);
                        send(clients[peer].fd, forward, L, 0);
                    }
                }
                break;
            }

            default:
                break;
            }

            if (--nfds == 0)
                break;
        }
    }

    // cerrar socket de escucha
    close(listen_fd);
    return 0;
}