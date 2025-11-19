#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <poll.h>
#include <strings.h> // Para usar strcasecmp
#include <ctype.h>   // Para la manipulación de caracteres
#include <locale.h>  // Proporciona funciones para localización

// Estructura del socket

// Configuración
#define PORT 8080        // Número de puerto
#define BUF_SIZE 1024    // Tamaño de buffer
#define MAX_CLIENTS 100  // Máximo número de clientes permitidos
#define NAME_LEN 32      // Longitud máxima de nombre de usuario
#define MAX_ROOMS 10     // Cantidad máxima de salas permitidos
#define ROOM_NAME_LEN 32 // Lóngitud máxima de nombre de sala

// Estado del cliente
#define STATE_NAME 0     // Esperando nombre de usuario
#define STATE_CHOOSING 1 // Esperando elegir interlocutor
#define STATE_CHATTING 2 // Chat activo
#define STATE_IN_ROOM 3  // En sala de chat

typedef struct
{
    int fd;              // Descriptor de socket
    char name[NAME_LEN]; // Nombre del usuario
    int state;           // Estado actual
    int peer_idx;        // Índice del cliente emparejado, o -1
    int room_idx;        // Índice de la sala, o -1 si no está en sala
} client_t;

void broadcast_a_la_sala(int room_idx, int sender_idx,
                         const char *message, int msg_len,
                        client_t clients[]);

// Estructura para salas de chat
typedef struct
{
    char name[ROOM_NAME_LEN];
    int client_indices[MAX_CLIENTS];
    int client_count;
} room_t;



// Variables globales para salas
room_t rooms[MAX_ROOMS];
int room_count = 0;

// Variable para el conteo de usuarios conectados
int contar_usuarios_conectados(client_t clients[]);

// Función para buscar o crear una sala
int buscar_o_crear_sala(const char *room_name)
{

    // Buscar sala existente
    for (int i = 0; i < room_count; i++)
    {
        if (strcasecmp(rooms[i].name, room_name) == 0)
        {
            return i;
        }
    }

    // Crear nueva sala si hay espacio
    if (room_count < MAX_ROOMS)
    {
        strncpy(rooms[room_count].name, room_name, ROOM_NAME_LEN - 1);
        rooms[room_count].name[ROOM_NAME_LEN - 1] = '\0';
        rooms[room_count].client_count = 0;
        for (int i = 0; i < MAX_CLIENTS; i++)
        {
            rooms[room_count].client_indices[i] = -1;
        }
        printf("Nueva sala creada: %s\n", room_name);
        return room_count++;
    }

    return -1; // No hay espacio para más salas
}

// Función para agregar un cliente a una sala
void agregar_cliente_a_la_sala(int client_idx, int room_idx, client_t clients[])
{
    if (room_idx < 0 || room_idx >= room_count)
        return;

    room_t *room = &rooms[room_idx];

    // Verificar si ya está en la sala
    for (int i = 0; i < room->client_count; i++)
    {
        if (room->client_indices[i] == client_idx)
            return;
    }

    // Agrega a la sala
    if (room->client_count < MAX_CLIENTS)
    {
        room->client_indices[room->client_count++] = client_idx;
        clients[client_idx].room_idx = room_idx;
        clients[client_idx].state = STATE_IN_ROOM;
        clients[client_idx].peer_idx = -1;

        // Notifica a todos en la sala
        char msg[BUF_SIZE];
        int len = snprintf(msg, sizeof(msg), "========== %s se ha unido a la sala ==========\n", clients[client_idx].name);
        broadcast_a_la_sala(room_idx, client_idx, msg, len, clients);

        printf("El cliente '%s' se ha unido a sala '%s'\n", clients[client_idx].name, room->name);
    }
}

// Función para remover cliente de una sala
void eliminar_cliente_de_la_sala(int client_idx, client_t clients[])
{
    int room_idx = clients[client_idx].room_idx;
    if (room_idx == -1)
        return;

    room_t *room = &rooms[room_idx];

    // Busca y elimina al cliente
    for (int i = 0; i < room->client_count; i++)
    {
        if (room->client_indices[i] == client_idx)
        {
            // Mueve los últimos elementos hacia atrás
            for (int j = i; j < room->client_count - 1; j++)
            {
                room->client_indices[j] = room->client_indices[j + 1];
            }
            room->client_count--;
            break;
        }
    }

    // Notifica a los demás integrantes de la sala
    if (room->client_count > 0)
    {
        char msg[BUF_SIZE];
        int len = snprintf(msg, sizeof(msg), "========== %s ha dejado la sala ==========\n", clients[client_idx].name);
        broadcast_a_la_sala(room_idx, client_idx, msg, len, clients);
    }

    clients[client_idx].room_idx = -1;
    printf("El cliente '%s' ha salido de sala '%s'\n", clients[client_idx].name, room->name);

    // Elimina sala si ya está vacía
    if (room->client_count == 0)
    {
        printf("La sala '%s' ha sido eliminada (vacía)\n", room->name);
        for (int i = room_idx; i < room_count - 1; i++)
        {
            rooms[i] = rooms[i + 1];
        }
        room_count--;
    }
}

// Función para enviar un mensaje a todos los miembros en una sala
void broadcast_a_la_sala(int room_idx, int sender_idx, const char *message, int msg_len, client_t clients[])
{
    if (room_idx < 0 || room_idx >= room_count)
        return;

    room_t *room = &rooms[room_idx];
    char formatted_msg[BUF_SIZE];

    for (int i = 0; i < room->client_count; i++)
    {
        int client_idx = room->client_indices[i];
        if (client_idx != sender_idx && clients[client_idx].fd >= 0)
        {
            // Formatea el mensaje con el nombre del remitente
            if (sender_idx >= 0)
            {
                int len = snprintf(formatted_msg, sizeof(formatted_msg),
                                    "[%s] %s: %s\n", rooms[room_idx].name,
                                    clients[sender_idx].name, message);
                send(clients[client_idx].fd, formatted_msg, len, 0);
            }
            else
            {
                // En este caso se trata de un mensaje del sistema
                send(clients[client_idx].fd, message, msg_len, 0);
            }
        }
    }
}

// Función para listar las salas disponibles
void enviar_lista_de_salas(int client_idx, client_t clients[])
{
    char buf[BUF_SIZE];
    int len = snprintf(buf, sizeof(buf), "========== Salas Disponibles ==========\n");

    if (room_count == 0)
    {
        len += snprintf(buf + len, sizeof(buf) - len, "No hay salas activas.\n");
        len += snprintf(buf + len, sizeof(buf) - len, "Para crear una sala, usa: /join #nombre-de-sala\n");
    }
    else
    {
        for (int i = 0; i < room_count; i++)
        {
            len += snprintf(buf + len, sizeof(buf) - len,
                            "#%s - %d usuarios\n",
                            rooms[i].name, rooms[i].client_count);
        }
    }

    len += snprintf(buf + len, sizeof(buf) - len,
                    "\n========== Comandos de Salas ==========\n");
    len += snprintf(buf + len, sizeof(buf) - len,
                    "/join #nombre-de-sala - Unirse o crear sala\n");
    len += snprintf(buf + len, sizeof(buf) - len,
                    "/leave - Salir de la sala actual\n");
    len += snprintf(buf + len, sizeof(buf) - len,
                    "/rooms - Listar salas disponibles\n");
    len += snprintf(buf + len, sizeof(buf) - len,
                    "/list - Listar usuarios conectados\n");
    len += snprintf(buf + len, sizeof(buf) - len,
                    "=======================================\n");

    send(clients[client_idx].fd, buf, len, 0);
}

// Función para listar usuarios en la sala actual
void enviar_usuarios_de_la_sala(int client_idx, client_t clients[])
{
    int room_idx = clients[client_idx].room_idx;
    if (room_idx == -1)
    {
        const char *msg = "No estás en una sala. Usa /join #nombre-de-sala para unirte.\n";
        send(clients[client_idx].fd, msg, strlen(msg), 0);
        return;
    }

    room_t *room = &rooms[room_idx];
    char buf[BUF_SIZE];
    int len = snprintf(buf, sizeof(buf), "========== Usuarios en #%s ==========\n", room->name);

    for (int i = 0; i < room->client_count; i++)
    {
        int idx = room->client_indices[i];
        len += snprintf(buf + len, sizeof(buf) - len,
                        "- %s\n", clients[idx].name);
    }

    len += snprintf(buf + len, sizeof(buf) - len,
                    "========== Total: %d usuarios ==========\n", room->client_count);

    send(clients[client_idx].fd, buf, len, 0);
}

// Envia al cliente i la lista de usuarios disponibles y el prompt de elección
// Método de listado básico SIN el estado del usuario
void enviar_lista_de_usuarios(int i, struct pollfd fds[], client_t clients[])
{
    char buf[BUF_SIZE];
    int len = snprintf(buf, sizeof(buf), "========== Usuarios Conectados ==========\n");

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
                    "\n========== Opciones ==========\n");
    len += snprintf(buf + len, sizeof(buf) - len,
                    "- Escribe un nombre para acceder a un chat privado\n");
    len += snprintf(buf + len, sizeof(buf) - len,
                    "- Usa /join #nombre-de-sala para unirte a una sala\n");
    len += snprintf(buf + len, sizeof(buf) - len,
                    "- Usa /rooms para ver las salas disponibles\n");
    len += snprintf(buf + len, sizeof(buf) - len,
                    "- Usa /list para ver la lista detallada de los usuarios conectados\n");

    send(clients[i].fd, buf, len, 0);
}

// Función para enviar la lista de usuarios conectados
void enviar_lista_de_usuarios_al_cliente(int client_idx, struct pollfd fds[], client_t clients[])
{
    char buf[BUF_SIZE];
    int len = snprintf(buf, sizeof(buf), "========== Usuarios Conectados ==========\n");

    for (int j = 1; j <= MAX_CLIENTS; j++)
    {
        if (j == client_idx)
            continue;
        if (clients[j].fd >= 0 && clients[j].state >= STATE_CHOOSING)
        {
            char status[64];
            if (clients[j].state == STATE_CHATTING)
            {
                strcpy(status, "[Chat privado]"); // Si el usuario está chateando con otro directamente
            }
            else if (clients[j].state == STATE_IN_ROOM)
            {
                snprintf(status, sizeof(status), "[En #%s]", rooms[clients[j].room_idx].name); // Si el usuario está en una sala de chat
            }
            else
            {
                strcpy(status, "[Disponible]"); // Si el usuario no está chateando actualmente con nadie
            }
            len += snprintf(buf + len, sizeof(buf) - len,
                            "- %s %s\n", clients[j].name, status);
        }
    }

    len += snprintf(buf + len, sizeof(buf) - len,
                    "========== Total: %d usuarios ==========\n", contar_usuarios_conectados(clients));

    send(clients[client_idx].fd, buf, len, 0);
}

// Función auxiliar para contar usuarios conectados
int contar_usuarios_conectados(client_t clients[])
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

// Quita espacios en blanco al inicio y al final de la cadena
void quitar_espacios_en_blanco(char *s)
{
    if (!s)
        return;

    // Trim al inicio
    char *start = s;
    while (*start && isspace((unsigned char)*start))
        start++;

    // Si todo eran espacios en blanco
    if (*start == 0)
    {
        *s = '\0';
        return;
    }

    // Mueve el contenido hacia el inicio
    if (start != s)
        memmove(s, start, strlen(start) + 1);

    // Trim al final
    char *end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end))
        end--;
    *(end + 1) = '\0';
}

int main()
{
    // Configuración para carácteres Unicode
    
    setlocale(LC_ALL, "es_ES.UTF-8");

    // Inicio del programa principal
    int listen_fd, nfds;
    struct sockaddr_in addr;
    struct pollfd fds[MAX_CLIENTS + 1];
    client_t clients[MAX_CLIENTS + 1];
    char buffer[BUF_SIZE];

    // Inicializa el conteo de salas
    room_count = 0;

    // 1. Crea y configura el socket de escucha
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

    printf("El servidor está escuchando el puerto %d\n", PORT);

    // 2. Inicializa fds y clients
    fds[0].fd = listen_fd;
    fds[0].events = POLLIN;

    for (int i = 1; i <= MAX_CLIENTS; i++)
    {
        fds[i].fd = -1;
        clients[i].fd = -1;
        clients[i].state = -1;
        clients[i].peer_idx = -1;
        clients[i].room_idx = -1;
        clients[i].name[0] = '\0'; // Cadena vacía
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
                // Busca ranura libre
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
                        clients[i].room_idx = -1;
                        clients[i].name[0] = '\0';
                        const char *msg = "Bienvenido. Ingresa tu nombre: \n";
                        send(conn_fd, msg, strlen(msg), 0);
                        printf("Nuevo cliente fd=%d asignado al slot %d\n", conn_fd, i);
                        break;
                    }
                }

                if (i > MAX_CLIENTS)
                {
                    const char *msg = "El servidor está lleno. Vuelve a intenta más tarde.\n";
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
                // Se produce desconexión o error
                printf("El cliente %s (fd=%d) se ha desconectado\n", clients[i].name, fd);

                // Maneja la desconexión según el estado
                if (clients[i].state == STATE_CHATTING)
                {
                    int peer = clients[i].peer_idx;
                    if (peer > 0 && clients[peer].state == STATE_CHATTING)
                    {
                        // Notifica al peer
                        const char *msg = "========== El otro usuario se desconecto ==========\n";
                        send(clients[peer].fd, msg, strlen(msg), 0);

                        // Vuelve a elegir interlocutor
                        clients[peer].state = STATE_CHOOSING;
                        clients[peer].peer_idx = -1;
                        enviar_lista_de_usuarios(peer, fds, clients);
                    }
                }
                else if (clients[i].state == STATE_IN_ROOM)
                {
                    eliminar_cliente_de_la_sala(i, clients);
                }

                // limpiar slot i
                close(fd);
                fds[i].fd = -1;
                clients[i].fd = -1;
                clients[i].state = -1;
                clients[i].peer_idx = -1;
                clients[i].room_idx = -1;
                clients[i].name[0] = '\0';
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

            // Procesa comandos de sala
            if (strncmp(buffer, "/join ", 6) == 0)
            {
                char *room_name = buffer + 6;
                quitar_espacios_en_blanco(room_name);

                if (room_name[0] == '#')
                    room_name++;
                if (strlen(room_name) == 0)
                {
                    const char *msg = "Uso: /join #nombre-de-sala\n";
                    send(fd, msg, strlen(msg), 0);
                }
                else
                {
                    int room_idx = buscar_o_crear_sala(room_name);
                    if (room_idx != -1)
                    {
                        // Saca a usuario de la sala actual si está en una
                        if (clients[i].room_idx != -1)
                        {
                            eliminar_cliente_de_la_sala(i, clients);
                        }
                        agregar_cliente_a_la_sala(i, room_idx, clients);
                    }
                    else
                    {
                        const char *msg = "ERROR: No se pudo crear/ingresar a la sala (límite de clientes alcanzado).\n";
                        send(fd, msg, strlen(msg), 0);
                    }
                }
                continue;
            }
            else if (strcmp(buffer, "/leave") == 0)
            {
                if (clients[i].state == STATE_IN_ROOM)
                {
                    eliminar_cliente_de_la_sala(i, clients);
                    clients[i].state = STATE_CHOOSING;
                    enviar_lista_de_usuarios(i, fds, clients);
                }
                else
                {
                    const char *msg = "No estás en una sala actualmente.\n";
                    send(fd, msg, strlen(msg), 0);
                }
                continue;
            }
            else if (strcmp(buffer, "/rooms") == 0)
            {
                enviar_lista_de_salas(i, clients);
                continue;
            }
            else if (strcmp(buffer, "/roomusers") == 0)
            {
                enviar_usuarios_de_la_sala(i, clients);
                continue;
            }

            // Manejo de estados
            switch (clients[i].state)
            {
            case STATE_NAME:
            {
                // Limpia espacios en blanco
                quitar_espacios_en_blanco(buffer);

                // Validaciones adicionales de nombre de usuario
                int valid = 1;
                const char *err_msg = NULL;

                if (buffer[0] == '\0')
                {
                    valid = 0;
                    err_msg = "NOMBRE NO VÁLIDO: no puede estar vacío. Ingresa tu nombre: \n";
                }
                else if (buffer[0] == '/')
                {
                    valid = 0;
                    err_msg = "NOMBRE NO VÁLIDO: no puede iniciar con '/'. Ingresa tu nombre: \n"; // Para no usar el mismo carácter que identifica a los comandos del cliente
                }
                else
                {
                    // Comprueba duplicados (case-insensitive)
                    for (int j = 1; j <= MAX_CLIENTS; j++)
                    {
                        if (j == i)
                            continue;
                        if (clients[j].fd >= 0 && clients[j].name[0] != '\0')
                        {
                            if (strcasecmp(clients[j].name, buffer) == 0)
                            {
                                valid = 0;
                                err_msg = "NOMBRE NO VÁLIDO: ya existe otro usuario con ese nombre. Ingresa otro nombre: \n";
                                break;
                            }
                        }
                    }
                }

                if (!valid)
                {
                    // Mantiene el STATE_NAME para insistir
                    send(fd, err_msg, strlen(err_msg), 0);
                }
                else
                {
                    // Guarda el nombre y pasa a STATE_CHOOSING
                    strncpy(clients[i].name, buffer, NAME_LEN - 1);
                    clients[i].name[NAME_LEN - 1] = '\0';
                    clients[i].state = STATE_CHOOSING;
                    printf("El cliente fd=%d se ha registrado como '%s'\n", fd, clients[i].name);

                    enviar_lista_de_usuarios(i, fds, clients);
                }
                break;
            }
            case STATE_CHOOSING:
            {
                // Verifica si se ingresó el comando /list
                if (strcmp(buffer, "/list") == 0)
                {
                    enviar_lista_de_usuarios_al_cliente(i, fds, clients);
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
                        const char *msg_i = "Chat privado iniciado. Usa /exit para salir.\n";
                        const char *msg_j = "Chat privado iniciado. Usa /exit para salir.\n";
                        send(clients[i].fd, msg_i, strlen(msg_i), 0);
                        send(clients[j].fd, msg_j, strlen(msg_j), 0);
                        printf("Chat privado: '%s' <--> '%s'\n", clients[i].name, clients[j].name);
                        found = 1;
                        break;
                    }
                }

                if (!found)
                {
                    const char *msg = "========== Usuario no disponible ==========\n"
                                        "Elige otro o usa:\n\n"
                                        "/list - Ver usuarios\n"
                                        "/rooms - Ver salas\n"
                                        "/join #nombre-de-sala - Unirse a sala\n";
                    send(fd, msg, strlen(msg), 0);
                    // enviar_lista_de_usuarios(i, fds, clients); (Descomentar para mostrar la lista simplificada)
                }

                break;
            }

            case STATE_CHATTING:
            {
                int peer = clients[i].peer_idx;

                // Verifica si se ingresó el comando /list
                if (strcmp(buffer, "/list") == 0)
                {
                    const char *msg = "Usa /exit para salir del chat actual y ver la lista.\n";
                    send(fd, msg, strlen(msg), 0);
                    break;
                }

                if (strcmp(buffer, "/exit") == 0)
                {
                    // Rompe emparejamiento
                    const char *msg_exit = "========== Has salido del chat ==========\n";
                    send(fd, msg_exit, strlen(msg_exit), 0);

                    if (peer > 0)
                    {
                        const char *msg_peer = "========== El otro usuario ha salido del chat ==========\n";
                        send(clients[peer].fd, msg_peer, strlen(msg_peer), 0);
                        clients[peer].state = STATE_CHOOSING;
                        clients[peer].peer_idx = -1;
                        enviar_lista_de_usuarios(peer, fds, clients);
                    }

                    clients[i].state = STATE_CHOOSING;
                    clients[i].peer_idx = -1;
                    enviar_lista_de_usuarios(i, fds, clients);
                    printf("El chat finalizado para el cliente '%s'\n", clients[i].name);
                }
                else
                {
                    // Reenvía mensaje al peer
                    if (peer > 0 && clients[peer].fd >= 0)
                    {
                        char forward[BUF_SIZE];
                        int L = snprintf(forward, sizeof(forward), "%s: %s\n", clients[i].name, buffer);
                        send(clients[peer].fd, forward, L, 0);
                    }
                }
                break;
            }

            case STATE_IN_ROOM:
            {
                if (strcmp(buffer, "/exit") == 0 || strcmp(buffer, "/leave") == 0)
                {
                    eliminar_cliente_de_la_sala(i, clients);
                    clients[i].state = STATE_CHOOSING;
                    enviar_lista_de_usuarios(i, fds, clients);
                }
                else
                {
                    // Mensaje broadcast a la sala
                    broadcast_a_la_sala(clients[i].room_idx, i, buffer, strlen(buffer), clients);
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

    // Cierra el socket de escucha
    close(listen_fd);
    return 0;
}
