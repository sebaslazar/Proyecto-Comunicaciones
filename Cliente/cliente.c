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
    strftime(buffer, buffer_size, "%d-%m-%Y- %H:%M:%S", tm_info);
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
        printf("Error: No se pudo crear el archivo de historial.\n");
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

        agregar_a_historial("Sistema", buffer);

        // Gestiona el uso de '> ' en el prompt
        EnterCriticalSection(&console_cs);
        printf("\r%s", buffer);   // Limpia posible prompt anterior
        fflush(stdout);

        // Activa el prompt una vez que recibe el primer mensaje del servidor
        prompt_ready = 1;

        // Muestra el prompt sólo si el mensaje termina con \n (ya se completó una línea)
        size_t L = strlen(buffer);
        if (L > 0 && buffer[L - 1] == '\n') {
            printf("> ");
            fflush(stdout);
        }

        LeaveCriticalSection(&console_cs);
    }
    return 0;
}

// Función para mostrar ayuda de comandos
void mostrar_menu_ayuda()
{
    printf("\n========== COMANDOS DISPONIBLES ==========\n");
    printf("/list     - Mostrar usuarios conectados\n");
    printf("/history  - Mostrar historial reciente\n");
    printf("/export   - Exportar historial completo a archivo\n");
    printf("/clear    - Limpiar historial local\n");
    printf("/help     - Mostrar esta ayuda\n");
    printf("/exit     - Salir del chat\n\n");
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

    server.sin_family = AF_INET;
    server.sin_port = htons(SERVER_PORT);
    server.sin_addr.s_addr = inet_addr(SERVER_IP);

    mostrar_banner();

    if (connect(sock, (struct sockaddr *)&server, sizeof(server)) < 0)
    {
        printf("No se pudo conectar con el servidor...\n\n");

        // Hace cuenta regresiva para el cierre de la ventana
        int segundos = 5;
        printf("Cerrando en %d segundos...", segundos);
        fflush(stdout);

        for (int i = segundos - 1; i >= 0; i--)
        {
            Sleep(1000); // Esperar 1 segundo
            printf("\rCerrando en %d segundos... ", i);
            fflush(stdout);
        }

        printf("\n");
        return 1;
    }

    // Muestra mensajes de ayuda
    printf("Conectado al servidor.\n\n");
    mostrar_menu_ayuda();
    CreateThread(NULL, 0, recibirMensajes, &sock, 0, &threadId);

    while (1)
    {
        // Imprime el prompt protegido
        EnterCriticalSection(&console_cs);
        if (prompt_ready) { // Sólo muestra el prompt si ya es adecuado
            printf("> ");
            fflush(stdout);
        }
        LeaveCriticalSection(&console_cs);

        if (fgets(mensaje, sizeof(mensaje), stdin) == NULL) {
            break;
        }

        // Elimina salto de línea
        mensaje[strcspn(mensaje, "\n")] = 0;

        // Verifia que se haya escrito algo
        if (strlen(mensaje) == 0)
            continue;

        // Procesamiento de comandos
        if (strcmp(mensaje, "/exit") == 0)
        {
            agregar_a_historial("Tú", "Saliste del chat");
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

        // Guarda el mensaje propio en historial antes de enviar
        agregar_a_historial("Tú", mensaje);

        // Envia mensaje al servidor
        strcat(mensaje, "\n");
        send(sock, mensaje, strlen(mensaje), 0);
    }

    // Exporta el automáticamente el historial al salir
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