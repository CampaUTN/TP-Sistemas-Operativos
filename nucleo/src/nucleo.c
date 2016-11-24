/*
 * nucleo.c
 *
 *  Created on: 16/4/2016
 *      Author: utnso
 */
#include "nucleo.h"

/* ---------- INICIO PARA UMC ---------- */
bool pedirPaginas(t_proceso* proceso, char* codigo) {
	char* serialPaginas = intToChar4(proceso->PCB->cantidad_paginas);
	char* serialPid = intToChar4(proceso->PCB->PID);
	char* respuesta = malloc(1);
	if (DEBUG_IGNORE_UMC || DEBUG_IGNORE_UMC_PAGES) { // Para DEBUG
		log_warning(activeLogger,
				"DEBUG_IGNORE_UMC_PAGES está en true! Se supone que no hay paginas");
	} else { /* Para curso normal del programa
	 Estos mutex garantizan que por ejemplo no haga cada hilo un send (esto podria solucionarse tambien juntando los send, pero es innecesario porque si o si hay que sincronizar)
	 y que no se van a correr los sends de un hilo 1, los del hilo 2, umc responde por hilo 1 (lo primero que le llego) y como corre hilo 2, esa respuesta llega al hilo 2 en vez de al 1. */
		pthread_mutex_lock(&mutexUMC);
		enviarHeader(umc, HeaderScript);
		send_w(umc, serialPid, sizeof(int));
		send_w(umc, serialPaginas, sizeof(int));
		enviarLargoYString(umc, codigo);
		read(umc, respuesta, 1);
		printf("Rta: %d\n",charToInt(respuesta));
		pthread_mutex_unlock(&mutexUMC);
		if (charToInt(respuesta) == 1)
			log_info(activeLogger, "Hay memoria disponible para el proceso %d.",
					proceso->PCB->PID);
		else if (charToInt(respuesta) == 0)
			log_info(activeLogger, "No hay memoria disponible para el proceso %d.",
					proceso->PCB->PID);
		else
			log_warning(activeLogger, "Umc debería enviar (0 o 1) y envió %s",
					charToInt(respuesta));
	}
	free(serialPid);
	free(serialPaginas);
	return charToInt(respuesta);
}
int getHandshake() {
	char* handshake = recv_nowait_ws(umc, 1);
	return charToInt(handshake);
}
void handshakearUMC() {
	char *hand = string_from_format("%c%c", HeaderHandshake, SOYNUCLEO);
	send_w(umc, hand, 2);

	log_info(bgLogger, "UMC handshakeo.");
	if (getHandshake() != SOYUMC) {
		perror("Se esperaba conectarse a la UMC.");
	} else
		log_info(bgLogger, "Núcleo recibió handshake de UMC.");
}
void establecerConexionConUMC() {
	direccionUMC = crearDireccionParaCliente(config.puertoUMC, config.ipUMC);
	umc = socket_w();
	connect_w(umc, &direccionUMC);
}
void warnDebug() {
	log_warning(activeLogger, "--- CORRIENDO EN MODO DEBUG!!! ---");
	log_info(activeLogger, "NO SE ESTABLECE CONEXION CON UMC EN ESTE MODO!");
	log_info(activeLogger,
			"Para correr nucleo en modo normal, settear en false el define DEBUG_IGNORE_UMC.");
	log_warning(activeLogger, "--- CORRIENDO EN MODO DEBUG!!! ---");
}
void enviarStackSize(){
	char* serialStackSize = intToChar4(config.stack_size);
	send_w(umc,serialStackSize,sizeof(int));
	free(serialStackSize);
}
void conectarAUMC() {
	if (!DEBUG_IGNORE_UMC) {
		log_info(bgLogger, "Iniciando conexion con UMC...");
		establecerConexionConUMC();
		log_info(activeLogger, "Conexion a la UMC correcta :).");
		handshakearUMC();
		log_info(activeLogger, "Handshake con UMC finalizado exitosamente.");
		recibirTamanioPagina();
		enviarStackSize();
		log_info(activeLogger, "Enviado stack size %d.",config.stack_size);
	} else {
		warnDebug();
	}
}

void recibirTamanioPagina(){
	char* serialTamanio = malloc(sizeof(int));
	serialTamanio = recv_waitall_ws(umc,sizeof(int));
	tamanio_pagina = char4ToInt(serialTamanio);
	log_info(activeLogger,"Recibido el tamaño de pagina: %d",tamanio_pagina);
	free(serialTamanio);
}
/*  ----------INICIO CONSOLA ---------- */
char* getScript(int consola,bool* exploto) {
	log_info(bgLogger, "Recibiendo archivo de consola %d...", consola);
	pthread_mutex_lock(&mutexClientes);
	char* script = leerLargoYMensaje(clientes[consola].socket);
	clientes[consola].atentido = false; //En true se bloquean, incluso si mando muchos de una consola usando un FOR para mandar el comando (leer wikia)

	if (script!=NULL)
	log_info(activeLogger, "Script de consola %d recibido:\n%s", consola,
			script);
	else{
		*exploto=true;
		log_error(activeLogger, "Llego mal el codigo de consola:%d",consola);
	}

	pthread_mutex_unlock(&mutexClientes);
	return script;
}
/*  ----------INICIO NUCLEO ---------- */
void inicializar() {
	system("rm -rf *.log");
	crearLogs("Nucleo", "Nucleo",0);
//	testear(test_serializacion);
	log_info(activeLogger, "INICIALIZANDO");
	tamanio_pagina=100;
	listaProcesos = list_create();
	colaCPU = queue_create();
	colaListos = queue_create();
	colaSalida = queue_create();
	tablaIO = dictionary_create();
	tablaSEM = dictionary_create();
	tablaGlobales = dictionary_create();

	int i;
	for (i=0;i<getMaxClients();i++){
		clientes[i].proceso=NULL;
	}

	cargarConfiguracion();
	iniciarVigilanciaConfiguracion();
	iniciarAtrrYMutexs(4,&mutexUMC,&mutexClientes,&mutexEstados, &mutexPlanificacion);
	crearSemaforos();
	crearIOs();
	crearCompartidas();
	algoritmo = FIFO;
	configurarServidorExtendido(&socketConsola, &direccionConsola,
			config.puertoConsola, &tamanioDireccionConsola, &activadoConsola);
	configurarServidorExtendido(&socketCPU, &direccionCPU, config.puertoCPU,
			&tamanioDireccionCPU, &activadoCPU);
	inicializarClientes();
	conectarAUMC();
//	testear(test_nucleo);
//	crearHilo(&hiloPlanificacion, planificar);
}
void cargarConfiguracion() {
	log_info(bgLogger, "Cargando archivo de configuracion");
	t_config* configNucleo;
	configNucleo = config_create("nucleo.cfg");
	config.puertoConsola = config_get_int_value(configNucleo, "PUERTO_PROG");
	config.puertoCPU = config_get_int_value(configNucleo, "PUERTO_CPU");
	config.puertoUMC = config_get_int_value(configNucleo, "PUERTO_UMC");
	config.ipUMC = string_duplicate(
			config_get_string_value(configNucleo, "IP_UMC"));
	config.quantum = config_get_int_value(configNucleo, "QUANTUM");
	config.queantum_sleep = config_get_int_value(configNucleo, "QUANTUM_SLEEP");
	config.sem_ids = config_get_array_value(configNucleo, "SEM_ID");
	config.semInit = config_get_array_value(configNucleo, "SEM_INIT");
	config.io_ids = config_get_array_value(configNucleo, "IO_ID");
	config.ioSleep = config_get_array_value(configNucleo, "IO_SLEEP");
	config.sharedVars = config_get_array_value(configNucleo, "SHARED_VARS");
	config.stack_size = config_get_int_value(configNucleo, "STACK_SIZE");

	config_destroy(configNucleo);
}
void recargarConfiguracion() {
	log_info(bgLogger, "Recargando archivo de configuracion");
	t_config* configNucleo;
	configNucleo = config_create("nucleo.cfg");
	config.quantum = config_get_int_value(configNucleo, "QUANTUM");
	config.queantum_sleep = config_get_int_value(configNucleo, "QUANTUM_SLEEP");
	config_destroy(configNucleo);
}
void iniciarVigilanciaConfiguracion(){
	cambiosConfiguracion = inotify_init();
	inotify_add_watch(cambiosConfiguracion,".",IN_CLOSE_WRITE);
}
void procesarCambiosConfiguracion(){
	char buffer[EVENT_BUF_LEN];
	int length = read(cambiosConfiguracion, buffer, EVENT_BUF_LEN);
	int e = 0;
	while (e < length) {
		struct inotify_event *event =
				(struct inotify_event *) &buffer[e];
		if (event->len) {
			if (event->mask & IN_CLOSE_WRITE) {
				if (strcmp(event->name, "nucleo.cfg") == 0) {
					log_info(activeLogger,
							"Se modifico el archivo %s y se releera",
							event->name);
					recargarConfiguracion();
				}
			}
		}
		e += EVENT_SIZE + event->len;
	}
}
void finalizar() {
	log_info(activeLogger, "FINALIZANDO");
	destruirLogs();
	list_destroy(listaProcesos);
	queue_destroy(colaCPU);
	queue_destroy(colaListos);
	queue_destroy(colaSalida);
	finalizarAtrrYMutexs();
	destruirSemaforos();
	destruirIOs();
	destruirCompartidas();
}
void crearIOs() {
	int i = 0;
	while (config.io_ids[i] != '\0') {
		t_IO* io = malloc(sizeof(t_IO));
		io->retardo = atoi(config.ioSleep[i])*1000;
		io->cola = queue_create();
		io->estado = INACTIVE;
		dictionary_put(tablaIO, config.io_ids[i], io);
		log_info(bgLogger, "Creando IO id:%s sleep:%d", config.io_ids[i],
				io->retardo);
		i++;
	}
}
void crearSemaforos() {
	int i = 0;
	while (config.sem_ids[i] != '\0') {
		t_semaforo* sem = malloc(sizeof(t_semaforo));
		sem->valor = atoi(config.semInit[i]);
		sem->cola = queue_create();
		dictionary_put(tablaSEM, config.sem_ids[i], sem);
		log_info(bgLogger, "Creando Semaforo:%s valor:%d", config.sem_ids[i],
				sem->valor);
		i++;
	}
}
void crearCompartidas() {
	int i = 0;
	while (config.sharedVars[i] != '\0') {
		int* init = malloc(sizeof(int));
		*init = 0;
		dictionary_put(tablaGlobales, config.sharedVars[i], init);
		log_info(bgLogger, "Creando Compartida:%s valor:%d",
				config.sharedVars[i], *init);
		i++;
	}
}
void destruirSemaforo(t_semaforo* sem) {
	queue_destroy(sem->cola);
	free(sem);
}
void destruirSemaforos() {
	dictionary_destroy_and_destroy_elements(tablaSEM, (void*) destruirSemaforo);
}
void destruirIO(t_IO* io) {
	queue_destroy(io->cola);
	free(io);
}
void destruirIOs() {
	dictionary_destroy_and_destroy_elements(tablaIO, (void*) destruirIO);
}
void destruirCompartida(int* compartida) {
	free(compartida);
}
void destruirCompartidas() {
	dictionary_destroy_and_destroy_elements(tablaGlobales,
			(void*) destruirSemaforo);
}
void atenderHandshake(int cliente){
	log_info(bgLogger, "Llego un handshake");
	char* header = malloc(sizeof(char));
	read(clientes[cliente].socket, header, sizeof(char));
	if ((charToInt(header) == SOYCONSOLA) || (charToInt(header) == SOYCPU)) {
		log_info(bgLogger, "Es un cliente apropiado! Respondiendo handshake");
		clientes[cliente].identidad = charToInt(header);
		enviarHeader(clientes[cliente].socket, SOYNUCLEO);
		if (charToInt(header) == SOYCPU)
			ingresarCPU(cliente);
	} else {
		log_error(activeLogger,
				"No es un cliente apropiado! rechazada la conexion");
		log_warning(activeLogger, "Se quitará al cliente %d.", cliente);
		quitarCliente(cliente);
	}
	free(header);
	clientes[cliente].atentido = false;
}
void recibirFinalizacion(int cliente) {
	t_proceso* proceso = obtenerProceso(cliente);
	if (proceso != NULL) {
		if (!proceso->abortado)
			finalizarProceso(cliente);
		desasignarCPU(proceso);
		clientes[cliente].atentido = false;
	}
}

void procesarHeader(int cliente, char *header) {
	// mutexClientes SAFE
	log_info(activeLogger, "Procesando:" ANSI_COLOR_YELLOW "%s" ANSI_COLOR_RESET " Cliente:%d", headerToString(charToInt(header)),cliente);
	clientes[cliente].atentido = true;

	switch (charToInt(header)) {

	case HeaderError:
		log_error(activeLogger, "Header de Error");
		quitarCliente(cliente);
		break;

	case HeaderHandshake:
		atenderHandshake(cliente);
		break;

	case HeaderImprimirVariableNucleo:
		imprimirVariable(cliente);
		break;

	case HeaderImprimirTextoNucleo:
		imprimirTexto(cliente);
		break;

	case HeaderScript:
		// Thread mutexClientes UNSAFE
		crearHiloConParametro(&clientes[cliente].hilo, (HILO)crearProceso ,(void*) cliente);
		break;

	case HeaderPedirValorVariableCompartida:
		recibirDevolverCompartida(cliente);
		break;

	case HeaderAsignarValorVariableCompartida:
		recibirAsignarCompartida(cliente);
		break;

	case HeaderSignal:
		recibirSignal(cliente);
		break;

	case HeaderWait:
		recibirWait(cliente);
		break;

	case headerNoTermineQuantumPeroToma:
		sigusr1(cliente);
		break;

	case headerTermineInstruccion:
		MUTEXPLANIFICACION(rafagaProceso(cliente));
		break;

	case HeaderEntradaSalida:
		entradaSalida(cliente);
		break;

	case HeaderTerminoProceso:
		MUTEXPLANIFICACION(recibirFinalizacion(cliente));
		break;

	default:
		log_error(activeLogger,
				"Llego el header numero %d y no hay una acción definida para él.\nSe quitará al cliente %d.",
				charToInt(header), cliente);
		quitarCliente(cliente);
		break;
	}
}

void sigusr1(int cpu){
	t_proceso* proceso = obtenerProceso(cpu);
	log_info(activeLogger,"CPU:%d ha sido víctima de " ANSI_COLOR_RED "SIGUSR1" ANSI_COLOR_RESET,cpu);
	if (proceso!=NULL){
		proceso->sigusr1=true;
	}
	else{
		quitarCliente(cpu);
		limpiarColaCPU();
	}
	clientes[cpu].atentido=false;
}

void finalizarConsola(int cliente) {
	log_info(activeLogger, "Consola:%d se desconectó.", cliente);
	t_proceso* proceso = clientes[cliente].proceso;
	if (proceso != NULL) {
		proceso->abortado = true;
		if (proceso->estado == READY || proceso->estado == BLOCK) {
			liberarRecursos(proceso);
			finalizarProceso(cliente);
		}
		else if (proceso->estado == EXEC){
			if (!clienteExiste(proceso->cpu)){
				liberarRecursos(proceso);
				finalizarProceso(cliente);
			}
			else
			log_info(activeLogger,"PID:%d finalizará al terminar el quantum actual",proceso->PCB->PID);
		}
	}
}
void finalizarCPU(int cliente){
	log_info(activeLogger, "CPU:%d se desconectó.",cliente);
	t_proceso* proceso = obtenerProceso(cliente);
	if (proceso!=NULL && !proceso->sigusr1){
		log_info(activeLogger, "PID:%d finalizará por desconexion de su CPU.",proceso->PCB->PID);
		enviarHeader(proceso->socketConsola,HeaderConsolaFinalizarMuerteCPU);
		return;}

}
void finalizarCliente(int cliente) {
	switch (clientes[cliente].identidad) {
	case SOYCONSOLA:
		finalizarConsola(cliente);
		break;
	case SOYCPU:
		finalizarCPU(cliente);
		break;
	default:
		log_error(activeLogger, "Cliente indefinido se desconectó.",cliente);
		break;
	}
	quitarCliente(cliente);
}


void hayLectura(int value) {
  fd_set rfd;
  FD_ZERO(&rfd);
  FD_SET(umc, &rfd);
  FD_SET(socketConsola, &rfd);
  FD_SET(socketCPU, &rfd);
  FD_SET(cambiosConfiguracion, &rfd);

  select(value+1, &rfd, 0, 0, NULL);
  int i;
  for (i = 0; i < getMaxClients(); i++) {
  			pthread_mutex_lock(&mutexClientes);
  			if (tieneLectura(clientes[i].socket)) {
  				pthread_mutex_unlock(&mutexClientes);
  				return;
  			}
  			pthread_mutex_unlock(&mutexClientes);
  	 }
}

void reiniciarTimeout(){
	espera.tv_sec = 1;
	espera.tv_usec = 5	;
}
int main(void) {
	system("clear");
	int i;
	char header[1];
	inicializar();
	procesos=0;
//	nice(15);

	log_info(activeLogger, "Esperando conexiones ...");
	while (1) {
		FD_ZERO(&socketsParaLectura);
		FD_SET(umc, &socketsParaLectura);
		FD_SET(socketConsola, &socketsParaLectura);
		FD_SET(socketCPU, &socketsParaLectura);
		FD_SET(cambiosConfiguracion, &socketsParaLectura);

		mayorDescriptor =
				(((socketConsola > socketCPU) ? socketConsola : socketCPU)
						< cambiosConfiguracion) ?
						cambiosConfiguracion :
						((socketConsola > socketCPU) ? socketConsola : socketCPU);
		MUTEXCLIENTES(incorporarClientes());
		//hayLectura(mayorDescriptor+1);
		reiniciarTimeout();
		select(mayorDescriptor + 1, &socketsParaLectura, NULL, NULL, &espera);
		if (tieneLectura(socketConsola))
			MUTEXCLIENTES(procesarNuevasConexionesExtendido(&socketConsola));
		if (tieneLectura(socketCPU))
			MUTEXCLIENTES(procesarNuevasConexionesExtendido(&socketCPU));
		if (tieneLectura(cambiosConfiguracion))
			procesarCambiosConfiguracion();

		if (tieneLectura(umc)){
			if (read(umc, header, 1) == 0){
				log_error(activeLogger,ANSI_COLOR_RED "Finalizando por desconexión de UMC" ANSI_COLOR_RESET);
				exit(EXIT_FAILURE);
			}
		}

		for (i = 0; i < getMaxClients(); i++) {
			pthread_mutex_lock(&mutexClientes);
			if (tieneLectura(clientes[i].socket)) {
				if (read(clientes[i].socket, header, 1) == 0) {
					finalizarCliente(i);
				} else
					procesarHeader(i, header);
			}
			pthread_mutex_unlock(&mutexClientes);
		}
		//
		//Planificacion IO
		pthread_mutex_lock(&mutexPlanificacion);
		planificacionFIFO();
		pthread_mutex_unlock(&mutexPlanificacion);
		//Planificacion IO
		dictionary_iterator(tablaIO, (void*) planificarIO);
		//
	}
	finalizar();
	return EXIT_SUCCESS;
}
