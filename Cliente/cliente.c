#include <winsock2.h>
#include <windows.h>
#include <stdio.h>
#include <locale.h> // Proporciona funciones para localización

#define SERVER_IP "10.253.56.15"
#define SERVER_PORT 8080

DWORD WINAPI recibirMensajes(LPVOID lpParam)
{
    SOCKET sock = *((SOCKET *)lpParam);
    char buffer[1024];
    int len;
    while ((len = recv(sock, buffer, sizeof(buffer) - 1, 0)) > 0)
    {
        buffer[len] = '\0'; // Se cambió el '�' por '\0
        printf("%s", buffer);
    }
    return 0;
}

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

    WSAStartup(MAKEWORD(2, 2), &wsaData);
    sock = socket(AF_INET, SOCK_STREAM, 0);

    server.sin_family = AF_INET;
    server.sin_port = htons(SERVER_PORT);
    server.sin_addr.s_addr = inet_addr(SERVER_IP);

    mostrar_banner();

    if (connect(sock, (struct sockaddr *)&server, sizeof(server)) < 0)
    {
        printf("No se pudo conectar con el servidor...\n\n");
        printf("Cerrando en 5 segundos...\n");
        Sleep(5000);  // Espera 5 segundos (5000 milisegundos)
        return 1;
    }

    printf("Conectado al servidor.\n\n");
    printf("Comandos disponibles:\n");
    printf("    /list - Mostrar usuarios conectados\n");
    printf("    /exit - Salir del chat\n\n");
    CreateThread(NULL, 0, recibirMensajes, &sock, 0, &threadId);

    while (1)
    {
        fgets(mensaje, sizeof(mensaje), stdin);
        if (strncmp(mensaje, "/exit", 5) == 0)
            break;
        send(sock, mensaje, strlen(mensaje), 0);
    }

    closesocket(sock);
    WSACleanup();
    return 0;
}