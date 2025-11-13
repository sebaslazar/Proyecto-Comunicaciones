#include <winsock2.h>
#include <windows.h>
#include <stdio.h>
#include <locale.h> // Proporciona funciones para localización
#include <time.h>   // Para el timestamp
#include <string.h> // Para el símbolo personalizado en el prompt

#define SERVER_IP "10.253.56.15"        // Dirección IP del servidor
#define SERVER_PORT 8080                // Puerto del servidor
#define HISTORY_FILE "chat_history.txt" // Nombre del archivo exportable
#define MAX_HISTORY 1000                // Número máximo de registros en el historial

// Estructura para el historial de mensajes
typedef struct
{
    char timestamp[20];
    char sender[32];
    char message[1024];
} chat_message_t;

chat_message_t chat_history[MAX_HISTORY];
int history_count = 0;
volatile int prompt_ready = 0; // Controla si ya se puede mostrar el prompt
CRITICAL_SECTION history_cs;
CRITICAL_SECTION console_cs;

// Función para obtener timestamp actual
void obtener_timestamp(char *buffer, int buffer_size)
{
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(buffer, buffer_size, "%Y-%m-%d %H:%M:%S", tm_info);
}

// Función para guardar mensaje en el historial
void agregar_a_historial(const char *sender, const char *message)
{
    EnterCriticalSection(&history_cs);

    if (history_count < MAX_HISTORY)
    {
        obtener_timestamp(chat_history[history_count].timestamp,
                          sizeof(chat_history[history_count].timestamp));

        strncpy(chat_history[history_count].sender, sender,
                sizeof(chat_history[history_count].sender) - 1);
        chat_history[history_count].sender[sizeof(chat_history[history_count].sender) - 1] = '\0';

        strncpy(chat_history[history_count].message, message,
                sizeof(chat_history[history_count].message) - 1);
        chat_history[history_count].message[sizeof(chat_history[history_count].message) - 1] = '\0';

        history_count++;
    }

    LeaveCriticalSection(&history_cs);
}

// Función para exportar historial a archivo
void exportar_historial_hacia_archivo()
{
    EnterCriticalSection(&history_cs);

    FILE *file = fopen(HISTORY_FILE, "wb");
    if (file == NULL)
    {
        printf("ERROR: No se pudo crear el archivo de historial.\n");
        LeaveCriticalSection(&history_cs);
        return;
    }

    // Escribe en BOM UTF-8 para que los editores de texto detecten UTF-8 correctamente
    unsigned char bom[] = {0xEF, 0xBB, 0xBF};
    fwrite(bom, sizeof(bom), 1, file);

    // Escribe en el archivo
    fprintf(file, "========== HISTORIAL DE CHAT ==========\n");
    fprintf(file, "Exportado: ");

    char timestamp[20];
    obtener_timestamp(timestamp, sizeof(timestamp));
    fprintf(file, "%s\n\n", timestamp);

    for (int i = 0; i < history_count; i++)
    {
        fprintf(file, "[%s] %s: %s\n",
                chat_history[i].timestamp,
                chat_history[i].sender,
                chat_history[i].message);
    }

    fprintf(file, "\n========== FIN DEL HISTORIAL ==========\n");
    fprintf(file, "Total de mensajes: %d\n", history_count);

    fclose(file);
    printf("Historial exportado a '%s' (%d mensajes)\n", HISTORY_FILE, history_count);

    LeaveCriticalSection(&history_cs);
}

// Función para mostrar el historial en pantalla
void mostrar_historial()
{
    EnterCriticalSection(&history_cs);

    if (history_count == 0)
    {
        printf("El historial está vacío.\n");
        LeaveCriticalSection(&history_cs);
        return;
    }

    printf("\n========== HISTORIAL RECIENTE (%d mensajes) ==========\n", history_count);
    int start = (history_count > 10) ? history_count - 10 : 0;

    for (int i = start; i < history_count; i++)
    {
        printf("[%s] %s: %s",
               chat_history[i].timestamp,
               chat_history[i].sender,
               chat_history[i].message);
    }
    printf("========== FIN DEL HISTORIAL ==========\n\n");

    LeaveCriticalSection(&history_cs);
}

// Función para limpiar el historial
void limpiar_historial()
{
    EnterCriticalSection(&history_cs);
    history_count = 0;
    printf("Historial eliminado.\n");
    LeaveCriticalSection(&history_cs);
}

// Función para recibir mensajes
DWORD WINAPI recibirMensajes(LPVOID lpParam)
{
    SOCKET sock = *((SOCKET *)lpParam);
    char buffer[1024];
    int len;

    while ((len = recv(sock, buffer, sizeof(buffer) - 1, 0)) > 0)
    {
        buffer[len] = '\0';

        // Determina la fuente del mensaje
        if (strstr(buffer, "Sistema:") != NULL ||
            strstr(buffer, "Bienvenido") != NULL ||
            strstr(buffer, "Usuarios conectados") != NULL ||
            strstr(buffer, "Salas Disponibles") != NULL ||
            strstr(buffer, "=== ") != NULL)
        {
            // Guarda en el historial antes de mostrar
            agregar_a_historial("Sistema", buffer);
        }
        else
        {
            // En este caso es un mesaje de chat, así que extraer el remitente
            char sender[32] = "Usuario";
            char *colon = strchr(buffer, ':');
            if (colon != NULL)
            {
                int sender_len = colon - buffer;
                if (sender_len < sizeof(sender) - 1)
                {
                    strncpy(sender, buffer, sender_len);
                    sender[sender_len] = '\0';
                }
            }
            agregar_a_historial(sender, buffer);
        }

        // Gestiona el uso de '> ' en el prompt
        EnterCriticalSection(&console_cs);
        printf("\r%s", buffer); // Limpia posible prompt anterior
        fflush(stdout);

        // Activa el prompt una vez que recibe el primer mensaje del servidor
        prompt_ready = 1;

        // Muestra el prompt sólo si el mensaje termina con \n (ya se completó una línea)
        size_t L = strlen(buffer);
        if (L > 0 && buffer[L - 1] == '\n')
        {
            printf("> ");
            fflush(stdout);
        }

        LeaveCriticalSection(&console_cs);
    }
    return 0;
}

// Función para mostrar comandos de ayuda
void mostrar_menu_ayuda()
{
    printf("\n========== COMANDOS DISPONIBLES ==========\n");
    printf("CHAT PRIVADO:\n");
    printf("  <nombre>     - Iniciar chat con usuario\n");
    printf("  /exit        - Salir del chat actual\n");
    printf("\nSALAS DE CHAT:\n");
    printf("  /join #sala  - Unirse o crear sala\n");
    printf("  /leave       - Salir de la sala actual\n");
    printf("  /rooms       - Listar salas disponibles\n");
    printf("  /roomusers   - Ver usuarios en tu sala\n");
    printf("\nGENERALES:\n");
    printf("  /list        - Mostrar usuarios conectados\n");
    printf("  /history     - Mostrar historial reciente\n");
    printf("  /export      - Exportar historial completo\n");
    printf("  /clear       - Limpiar historial local\n");
    printf("  /help        - Mostrar esta ayuda\n");
    printf("\nEJEMPLOS DE USO:\n");
    printf("  /join #general    - Unirse a sala general\n");
    printf("  /join #equipo1    - Crear la sala equipo1\n");
    printf("  Mario             - Chatear con Mario\n");
    printf("==========================================\n\n");
}

// Función para mostrar banner inicial
void mostrar_banner()
{
    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║                                                              ║\n");
    printf("║               ██████╗██╗  ██╗ █████╗ ████████╗               ║\n");
    printf("║              ██╔════╝██║  ██║██╔══██╗╚══██╔══╝               ║\n");
    printf("║              ██║     ███████║███████║   ██║                  ║\n");
    printf("║              ██║     ██╔══██║██╔══██║   ██║                  ║\n");
    printf("║              ╚██████╗██║  ██║██║  ██║   ██║                  ║\n");
    printf("║               ╚═════╝╚═╝  ╚═╝╚═╝  ╚═╝   ╚═╝                  ║\n");
    printf("║                                                              ║\n");
    printf("║             Bienvenido al Chat Local con Sockets             ║\n");
    printf("║                                                              ║\n");
    printf("║                Proyecto de Comunicaciones II                 ║\n");
    printf("║                    Facultad de Ingeniería                    ║\n");
    printf("║              Universidad Tecnológica de Pereira              ║\n");
    printf("║                                                              ║\n");
    printf("║            Integrantes: Sebastian Cacante Salazar            ║\n");
    printf("║                         Santiago Gónzalez Hincapié           ║\n");
    printf("║                         Nicolás Orozco Flórez                ║\n");
    printf("║                                                              ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n");
}

// Función que muestra una cuenta regresiva para la finalización del programa
void cuenta_regresiva_para_cierre(int segundos)
{
    printf("\nCerrando en %d segundos...", segundos);
    fflush(stdout);

    for (int i = segundos - 1; i >= 0; i--)
    {
        Sleep(1000); // Esperar 1 segundo
        printf("\rCerrando en %d segundos... ", i);
        fflush(stdout);
    }

    printf("\n");
}

int main()
{
    // Configuración para carácteres Unicode
    SetConsoleOutputCP(CP_UTF8);
    setlocale(LC_ALL, "es_ES.UTF-8");

    // Inicio de programa principal
    WSADATA wsaData;
    SOCKET sock;
    struct sockaddr_in server;
    char mensaje[1024];
    DWORD threadId;

    // Inicializa secciones críticas
    InitializeCriticalSection(&history_cs);
    InitializeCriticalSection(&console_cs);

    WSAStartup(MAKEWORD(2, 2), &wsaData);
    sock = socket(AF_INET, SOCK_STREAM, 0);

    // Válida que el socket se ha creado correctamente
    if (sock == INVALID_SOCKET)
    {
        printf("ERROR: No se pudo crear el socket...\n");
        cuenta_regresiva_para_cierre(5);
        return 1;
    }

    server.sin_family = AF_INET;
    server.sin_port = htons(SERVER_PORT);
    server.sin_addr.s_addr = inet_addr(SERVER_IP);

    mostrar_banner();

    if (connect(sock, (struct sockaddr *)&server, sizeof(server)) < 0)
    {
        printf("ERROR: No se pudo conectar al servidor %s:%d\n", SERVER_IP, SERVER_PORT);
        printf("Asegúrese de que el servidor se esté ejecutando.\n");
        cuenta_regresiva_para_cierre(5);
        return 1;
    }

    printf("Conectado al servidor.\n\n");

    // Muestra mensajes de ayuda
    mostrar_menu_ayuda();

    // Crea hilo para recibir mensajes
    CreateThread(NULL, 0, recibirMensajes, &sock, 0, &threadId);

    while (1)
    {
        // Imprime el prompt protegido
        EnterCriticalSection(&console_cs);
        if (prompt_ready)
        { // Sólo muestra el prompt si ya es adecuado
            printf("> ");
            fflush(stdout);
        }
        LeaveCriticalSection(&console_cs);

        if (fgets(mensaje, sizeof(mensaje), stdin) == NULL)
        {
            break;
        }

        // Elimina salto de línea
        mensaje[strcspn(mensaje, "\n")] = 0;

        // Verifica que se haya escrito algo
        if (strlen(mensaje) == 0)
            continue;

        // Procesamiento de comandos locales
        if (strcmp(mensaje, "/exit") == 0)
        {
            // Guarda mensaje de salida en historial
            agregar_a_historial("Tú", "Saliste del chat");

            // Envia comando de salida al servidor
            strcat(mensaje, "\n");
            send(sock, mensaje, strlen(mensaje), 0);
            break;
        }
        else if (strcmp(mensaje, "/history") == 0)
        {
            mostrar_historial();
            continue;
        }
        else if (strcmp(mensaje, "/export") == 0)
        {
            exportar_historial_hacia_archivo();
            continue;
        }
        else if (strcmp(mensaje, "/clear") == 0)
        {
            limpiar_historial();
            continue;
        }
        else if (strcmp(mensaje, "/help") == 0)
        {
            mostrar_menu_ayuda();
            continue;
        }
        else if (strcmp(mensaje, "/cls") == 0)
        {
            system("cls");
            mostrar_menu_ayuda();
            continue;
        }

        // Guarda el mensaje en historial antes de enviar, pero sólo si es no es un comando del sistema
        if (strncmp(mensaje, "/", 1) != 0)
        {
            agregar_a_historial("Tú", mensaje);
        }

        // Envia mensaje al servidor
        char mensaje_con_salto[1024];
        strcpy(mensaje_con_salto, mensaje);
        strcat(mensaje_con_salto, "\n");

        // Verifica que el mensaje se haya se haya envíado correctamente
        if (send(sock, mensaje_con_salto, strlen(mensaje_con_salto), 0) < 0)
        {
            printf("ERROR: No se pudo enviar el mensaje. Conexión perdida.\n");
            break;
        }

        // Pequeña pausa para evitar saturación
        Sleep(10);
    }

    // Exporta automáticamente el historial al salir
    printf("Exportando historial automaticamente...\n");
    exportar_historial_hacia_archivo();

    // Limpiar recursos
    closesocket(sock);
    WSACleanup();
    DeleteCriticalSection(&history_cs);
    DeleteCriticalSection(&console_cs);

    printf("Conexion cerrada. Presiona Enter para salir...\n");
    getchar();

    return 0;
}