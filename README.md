# CHAT UNO-A-UNO ENTRE WINDOWS Y LINUX

Este proyecto permite a los estudiantes crear un sistema de chat entre dos o más
computadoras conectadas por una red local. El servidor se ejecuta en Linux y los clientes en Windows, lo cual promueve el desarrollo de aplicaciones multiplataforma usando sockets.

## Objetivos del Proyecto

- Aplicar sockets TCP para establecer comunicaciones confiables entre cliente y
servidor, aprovechando su naturaleza orientada a conexión para asegurar el envío
y recepción de mensajes sin pérdidas.

- Manejar múltiples clientes concurrentemente utilizando estructuras como poll() y mecanismos de estado interno, permitiendo conversaciones independientes entre pares de usuarios conectados a través del servidor.

- Intercambiar mensajes de texto entre usuarios de forma interactiva, incluyendo el desarrollo de comandos personalizados como /exit, así como futuras extensiones como /rename o /list que permitan enriquecer la experiencia de chat.

- Introducir a los estudiantes en el diseño de protocolos simples sobre TCP,
reforzando su comprensión de los flujos de datos, estructura de mensajes, y eventos propios de una conversación bidireccional en tiempo real.

- Fomentar el aprendizaje práctico de la programación en red a través de la integración de sistemas operativos diferentes (cliente en Windows y servidor en Linux), promoviendo el desarrollo multiplataforma.

### Descripción General

- **Servidor**: Implementado completamente en C utilizando el mecanismo poll() como estrategia de multiplexación no bloqueante, el servidor está diseñado para aceptar múltiples conexiones entrantes de clientes. A través de este mecanismo, es capaz de detectar eventos de lectura en cada uno de los sockets asociados a los clientes, y en función del estado del cliente (esperando nombre, eligiendo interlocutor o en conversación activa), ejecuta las acciones correspondientes, facilitando así el emparejamiento eficiente de usuarios en sesiones de chat uno-a-uno. Este diseño garantiza escalabilidad sin requerir la creación de múltiples hilos ni procesos, optimizando el uso de recursos del sistema y facilitando la gestión de estado para cada cliente conectado.

- **Cliente**: Implementado en C específicamente para sistemas Windows, este componente hace uso de la biblioteca Winsock2 para establecer la conexión con el servidor y de la función CreateThread para permitir la recepción de mensajes de forma asincrónica, asegurando una experiencia fluida de comunicación mientras el usuario escribe y envía mensajes. La implementación se ha probado en múltiples versiones de Windows, manteniendo compatibilidad desde Windows 7 hasta las versiones más recientes del sistema operativo.

- **Protocolo de Comunicación**: El protocolo empleado está basado en el intercambio de nombres entre clientes y el posterior envío de mensajes de texto a través de sockets TCP. El protocolo es sencillo pero efectivo, permite detectar comandos como /exit y controlar el flujo de mensajes dependiendo del estado de cada cliente, ofreciendo así una base sólida para futuras extensiones como comandos personalizados, encriptación, autenticación o transferencia de archivos.

- **Plataforma**: El sistema se ejecuta en una arquitectura híbrida: el servidor corre bajo un entorno Linux, aprovechando sus capacidades nativas de red y estabilidad, mientras que los clientes corren en sistemas Windows, facilitando la adopción por parte de estudiantes que utilizan este sistema operativo de forma habitual. La solución ha sido diseñada para asegurar compatibilidad cruzada y facilidad de uso, fomentando la comprensión de redes multiplataforma.

## Arquitectura del Proyecto

[ Cliente Windows A ] —-|

                      |—-> [ Servidor Linux (poll) ]

[ Cliente Windows B ] —-|

Esta arquitectura plantea un entorno sencillo pero funcional para el desarrollo de un sistema de mensajería en red. El servidor actúa como un intermediario centralizado que recibe conexiones desde múltiples clientes y gestiona la comunicación entre ellos. Cada cliente, implementado para sistemas Windows, se conecta directamente al servidor a través de un socket TCP apuntando a una dirección IP y un puerto predefinido (por ejemplo, el 8080). Una vez establecida la conexión, se inicia una serie de interacciones que determinan la dinámica del chat.

1. Cada cliente inicia su conexión hacia el servidor utilizando la IP del servidor Linux y el puerto TCP configurado. Esta conexión es persistente durante la sesión.

2. Al establecerse la conexión, al cliente se le solicita que envíe un nombre de usuario identificador. Con ese nombre se listará en el servidor como disponible para iniciar conversación.

3. El cliente, ya identificado, recibe un listado de otros usuarios conectados y disponibles. Puede elegir con cuál desea entablar una conversación. Esta elección se realiza mediante el envío del nombre de usuario deseado.

4. El servidor valida si el usuario de destino está libre (no chateando con otro) y, si está disponible, empareja a ambos usuarios, iniciando una sesión de chat uno a uno.

5. Durante la sesión, los mensajes que uno de los clientes envía son reenviados directamente al otro cliente por medio del servidor, que se limita a actuar como puente.

6. Si uno de los clientes decide finalizar la conversación, puede enviar el comando especial /exit. El servidor detecta este mensaje y rompe el vínculo de chat entre los dos usuarios, regresándolos al estado de selección para poder iniciar nuevas conversaciones con otros usuarios.

Esta arquitectura está diseñada para ser didáctica, permitiendo a los estudiantes comprender cómo funciona un servidor con múltiples clientes, cómo se maneja el estado de cada conexión y cómo se enrutan los mensajes entre pares. Además, sienta las bases para futuras ampliaciones, como salas de chat, chats grupales o autenticación de usuarios.

## Requisitos de Compilación

### Clientes Windows

- Compilar con MinGW o Visual Studio para asegurar la compatibilidad con las
bibliotecas del sistema.

- Es obligatorio incluir la biblioteca ws2_32.lib en el proceso de compilación, ya que proporciona las funciones necesarias para el uso de sockets en Windows
(Winsock).

- El archivo fuente principal se denomina cliente.c y contiene toda la lógica de
conexión, recepción y envío de mensajes.

- Ejemplo de compilación usando MinGW:

```bash
gcc cliente.c -o cliente.exe -lws2_32
```

- Asegurarse de tener instalado el SDK de Windows o el entorno adecuado si se
compila con Visual Studio.
bash


### Servidor Linux

- Utilizar GCC (GNU Compiler Collection) como compilador principal, ampliamente disponible en la mayoría de las distribuciones de Linux.
- El código fuente está contenido en el archivo servidor.c, el cual implementa el servidor con multiplexación mediante poll() y gestión de múltiples clientes.
- Ejemplo de compilación:

```bash
gcc servidor.c -o servidor
```

- Puede ser necesario usar privilegios de superusuario si se desea ejecutar en puertos inferiores al 1024 o para operaciones avanzadas de red.

## Flujo de Interacción

1. Cliente se conecta → Estado inicial: **STATE_NAME**. En esta etapa, el servidor espera la identificación del usuario.
2. Cliente envía su nombre de usuario → El servidor verifica que el nombre no esté en uso y actualiza el estado a **STATE_CHOOSING**, indicando que el usuario puede ahora elegir con quién desea chatear.
3. Cliente elige un compañero de chat entre los usuarios disponibles → Si el usuario solicitado también está en **STATE_CHOOSING**, ambos son emparejados y su estado cambia a **STATE_CHATTING**, habilitando la conversación privada entre los dos.
4. Mientras se encuentran en **STATE_CHATTING**, los mensajes escritos por uno de los participantes son automáticamente reenviados al otro por el servidor, quien actúa como intermediario y no modifica el contenido de los mensajes.
5. Si cualquiera de los usuarios escribe el comando /exit, el servidor interpreta esto como una solicitud de finalizar la conversación. En consecuencia, rompe el emparejamiento, devuelve a ambos usuarios al estado **STATE_CHOOSING**, y permite que elijan nuevos compañeros si lo desean.

## Retos Establecidos

### Básico

Comando /list para mostrar usuarios conectados al servidor, permitiendo que los clientes visualicen con quiénes pueden iniciar una conversación en tiempo real.

### Intermedio

Implementación de un historial local del chat para que el cliente pueda almacenar los mensajes intercambiados durante la sesión y eventualmente exportarlos a un archivo de texto.

### Avanzado

Soporte para salas de chat (por ejemplo, #general, #equipo1) donde múltiples usuarios puedan interactuar en un canal común, replicando el comportamiento de plataformas de mensajería modernas.

 
