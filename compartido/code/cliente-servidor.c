#include <stdlib.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include "cliente-servidor.h"
#include <commons/string.h>

int getMaxClients()
{
	return MAXCLIENTS;
}

int socket_w()
{
	return socket(AF_INET, SOCK_STREAM, 0);
}

void send_w(int cliente, char* msg, int msgSize)
{
	send(cliente, msg, msgSize, 0);
}

struct sockaddr_in crearDireccionParaCliente(unsigned short PORT, char* IP)
{
	struct sockaddr_in direccionServidor;
	direccionServidor.sin_family = AF_INET;
	direccionServidor.sin_addr.s_addr = inet_addr(IP);
	direccionServidor.sin_port = htons(PORT);
	return direccionServidor;
}

void connect_w(int cliente, struct sockaddr_in* direccionServidor)
{
	if (connect(cliente, (void*) direccionServidor, sizeof(*direccionServidor)) != 0)
	{
		perror("No se pudo conectar");
		exit(1);
	}
}

void permitirReutilizacion(int servidor, int* activado)
{
	(*activado)=1;
	setsockopt(servidor, SOL_SOCKET, SO_REUSEADDR, activado, sizeof(*activado));
}

char* recv_nowait_ws(int cliente, int msgSize)
{
	return recv_con_criterio_ws(cliente,msgSize,0);
}

char* recv_waitall_ws(int cliente, int msgSize)
{
	return recv_con_criterio_ws(cliente,msgSize,MSG_WAITALL);
}

char* recv_con_criterio_ws(int cliente, int msgSize, int msgCriterio)
{
	char* buffer = malloc(msgSize);
	int bytesRecibidos = recv(cliente, buffer, msgSize, msgCriterio);
	if (bytesRecibidos <= 0)
	{
		perror("recv devolvio un numero menor que cero");
		exit(1);
	}
	//buffer[msgSize-1]='\0';
	return buffer;
}

void listen_w(int servidor)
{
	listen(servidor, 100);
	//printf("Estoy escuchando...\n"); TODO deberia ser un log.
}

struct sockaddr_in crearDireccionParaServidor(unsigned short PORT)
{
	struct sockaddr_in direccionServidor;
	direccionServidor.sin_family = AF_INET;
	direccionServidor.sin_addr.s_addr = INADDR_ANY;
	direccionServidor.sin_port = htons(PORT);
	return direccionServidor;
}

void bind_ws(int servidor, struct sockaddr_in* direccionServidor)
{
	if( bind(servidor, (void*)direccionServidor, sizeof(*direccionServidor)) != 0 )
	{
		perror("Fallo el bind");
		exit(1);
	}
}

///-> Definidas por Ariel en nucleo

void configurarServidor(unsigned short PORT){
	// Definimos la direccion, creamos el socket, bindeamos y ponemos a escuchar nuevas conexiones
	direccion = crearDireccionParaServidor(PORT);
	socketNuevasConexiones = socket_w();
	permitirReutilizacion(socketNuevasConexiones,&activado);
	bind_ws(socketNuevasConexiones,&direccion);
	listen_w(socketNuevasConexiones);
	tamanioDireccion=sizeof(direccion);
}

void inicializarClientes(){
	// Inicializamos en desconectado = 0
	int i;
	for (i = 0; i < MAXCLIENTS; i++){
		clientes[i].socket=0;
		clientes[i].indice=i;
	}
}

int agregarCliente(t_cliente cliente){
	// Agregamos un cliente en la primera posicion libre del array de clientes
	int i=0;
	for (i = 0; i < MAXCLIENTS; i++) {
		if( clientes[i].socket == 0 )	{
			clientes[i].socket = cliente.socket;
			clientes[i].atentido=false;
			clientes[i].pid = (int)NULL;
			clientes[i].addr = cliente.addr;
			clientes[i].addrlen = cliente.addrlen;
			printf("Añadido a la lista de sockets como %d\n" , i);
			return i;
		}
	}
	printf("Se superó el numero maximo de clientes");
	close(cliente.socket);
	return -1;
}

void quitarCliente(int i){
	// Liberamos del array al cliente y cerramos su socket
	getpeername(clientes[i].socket , (struct sockaddr*)&clientes[i].addr , (socklen_t*)&clientes[i].addrlen);
	printf("Invitado desconectado , ip %s , puerto %d \n" , inet_ntoa(clientes[i].addr.sin_addr) , ntohs(clientes[i].addr.sin_port));
	close(clientes[i].socket);
	clientes[i].socket = 0;
}

void procesarNuevasConexiones(){
	// Aceptamos nueva conexion
	t_cliente cliente;
	cliente.addrlen=sizeof(cliente.addr);
	int socketNuevoCliente;
	socketNuevoCliente = accept(socketNuevasConexiones, (struct sockaddr *)&cliente.addr, (socklen_t*)&cliente.addrlen);
	printf("Nueva conexión , socket %d , ip is : %s , puerto : %d \n" , socketNuevoCliente , inet_ntoa(cliente.addr.sin_addr) , ntohs(cliente.addr.sin_port));
	agregarCliente(cliente);
}

int tieneLectura(int socket){
	return FD_ISSET(socket,&socketsParaLectura);
}

int incorporarSockets(){
	// Reseteamos y añadimos al set los sockets disponibles
	FD_ZERO(&socketsParaLectura);
	FD_SET(socketNuevasConexiones, &socketsParaLectura);
	mayorDescriptor=socketNuevasConexiones;
	return incorporarClientes();
}

int charToInt(char *c){
	return ((int)(*c));
}
char* intToChar(int i){
	return string_from_format("%c",i);
}

void configurarServidorExtendido(int* socket, struct sockaddr_in* dire, unsigned short PORT, unsigned int* tamanio, int* activado){
	// Definimos la direccion, creamos el socket, bindeamos y ponemos a escuchar nuevas conexiones
	(*dire) = crearDireccionParaServidor(PORT);
	(*socket) = socket_w();
	permitirReutilizacion((*socket),activado);
	bind_ws((*socket),dire);
	listen_w((*socket));
	(*tamanio)=sizeof(*dire);
}

int incorporarClientes(){
	// Reseteamos y añadimos al set los sockets disponibles, NO atendidos
	int i,filedes;
	for (i = 0; i < MAXCLIENTS; i++){
		filedes=clientes[i].socket;
		if (filedes>0){
			if (clientes[i].atentido==false){
				FD_SET(clientes[i].socket,&socketsParaLectura);
				if (filedes>mayorDescriptor)
					mayorDescriptor=filedes;
			}
		}
	}
	return mayorDescriptor;
}

int procesarNuevasConexionesExtendido(int* socket){
	// Aceptamos nueva conexion
	t_cliente cliente;
	cliente.addrlen=sizeof(cliente.addr);
	cliente.socket = accept((*socket), (struct sockaddr *)&cliente.addr, (socklen_t*)&cliente.addrlen);
	if ((cliente.indice = agregarCliente(cliente))>=0)
	printf("Nueva conexión , socket %d , ip is : %s , puerto : %d \n" , cliente.socket , inet_ntoa(cliente.addr.sin_addr) , ntohs(cliente.addr.sin_port));
	return cliente.indice;
}

char* intToChar4(int num){
	//RECORDAR: liberar el puntero con free()
	char* serial = malloc(sizeof(int));
	serializar_int(serial,&num);
	return serial;
}

int char4ToInt(char* chars){
	int a;
	deserializar_int(&a,chars);
	return a;
}
char* leerLargoYMensaje(int sock){
	char* serialLargo = malloc(sizeof(int));
	read(sock, serialLargo, sizeof(int));
	int largo = char4ToInt(serialLargo);
	char* mensaje = malloc(largo);
	read(sock, mensaje, largo);
	free(serialLargo);
	return mensaje;
}

void enviarLargoYString(int sock, char* mensaje){
	enviarLargoYSerial(sock, strlen(mensaje)+1, mensaje);
}
void enviarLargoYSerial(int sock, int largo, char* mensaje){
	char* serialLargo = intToChar4(largo);
	send_w(sock,serialLargo, sizeof(int));
	send_w(sock, mensaje, largo);
	free(serialLargo);
}

void enviarHeader(int cliente, int header){
	//printf("\n\n%s\n\n",headerToString(header));
	char* serialHeader = intToChar(header);
	send_w(cliente,serialHeader,1);
	free(serialHeader);
}

bool estaConectado(t_cliente cliente){
	getpeername(cliente.socket , (struct sockaddr*)&cliente.addr , (socklen_t*)&cliente.addrlen);
	return errno!=ENOTCONN;
}
