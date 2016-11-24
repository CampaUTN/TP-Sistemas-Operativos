/*
 * base.h
 *
 *  Created on: 25/5/2016
 *      Author: utnso
 */
#ifndef NUCLEO_H_
#define NUCLEO_H_

#include <stdbool.h>
#include <commons/string.h>
#include <commons/log.h>
#include <commons/config.h>
#include <commons/collections/queue.h>
#include "handshake.h"
#include "header.h"
#include "cliente-servidor.h"
#include "log.h"
#include "commonTypes.h"
#include "serializacion.h"
#include "hilos.h"
#include <sys/inotify.h>
#define EVENT_SIZE  ( sizeof (struct inotify_event) )
#define EVENT_BUF_LEN     ( 1024 * ( EVENT_SIZE + 16 ) )


#define ANSI_COLOR_YELLOW  "\t" // "\x1b[33m"
#define ANSI_COLOR_RED     "\t"//\x1b[31m
#define ANSI_COLOR_GREEN   "\t"//\x1b[32m
#define ANSI_COLOR_RESET   "\t"//\x1b[0m

int procesos;

#define SIN_ASIGNAR -1
/* ---------- INICIO DEBUG ---------- */
// Es util para debugear sin tener una consola extra con UMC abierto.
#define DEBUG_IGNORE_UMC false
#define DEBUG_IGNORE_UMC_PAGES false
/* ---------- INICIO DEBUG ---------- */
int socketConsola, socketCPU, activadoCPU, activadoConsola, umc, cambiosConfiguracion;
struct sockaddr_in direccionConsola, direccionCPU, direccionUMC;
unsigned int tamanioDireccionConsola, tamanioDireccionCPU, tamanio_pagina;
// Hilos
pthread_t hiloPlanificacion;
pthread_mutex_t mutexUMC, mutexClientes, mutexEstados, mutexPlanificacion;
// defino la palabra clave THREAD para reconocer las funciones que son main de un hilo
#define HILO void*
// MACROS DE MUTEXS
#define MUTEXPLANIFICACION(CONTENIDO) \
	MUTEX(CONTENIDO,mutexPlanificacion);
#define MUTEXCLIENTES(CONTENIDO) \
	MUTEX(CONTENIDO,mutexClientes);
#define MUTEXESTADOS(CONTENIDO) \
	MUTEX(CONTENIDO,mutexEstados);
#define MUTEXUMC(CONTENIDO) \
	MUTEX(CONTENIDO,mutexUMC);
#define MUTEXBLOQUEOS(CONTENIDO) \
	MUTEX(CONTENIDO,mutexBloqueos);
// Tipos
typedef enum t_proceso_estado {
	NEW, READY, EXEC, BLOCK, EXIT
} t_proceso_estado;
typedef enum t_planificacion {
	FIFO, RR
} t_planificacion;
typedef enum t_IO_estado {
	ACTIVE, INACTIVE
} t_IO_estado;
typedef struct t_proceso {
	int consola; // Indice de socketCliente
	int cpu; // Indice de socketCliente, legible solo cuando estado sea EXEC
	t_proceso_estado estado;
	t_PCB* PCB;
	int socketConsola;
	int socketCPU;
	int rafagas;
	bool abortado;
	bool sigusr1;
	char* io;
	char* semaforo;
} t_proceso;

//t_proceso* procesos[MAXCLIENTS];
char* strCola;

typedef struct t_IO {
	int retardo;
	t_IO_estado estado;
	t_queue* cola;
	pthread_t hilo;
} t_IO;
typedef struct t_semaforo {
	int valor;
	t_queue* cola;
} t_semaforo;
typedef struct t_bloqueo {
	t_IO* IO;
	t_proceso* proceso;
	int tiempo;
} t_bloqueo;
typedef struct customConfig {
	int puertoConsola;
	int puertoCPU;
	int quantum;
	int queantum_sleep; //TODO que sea modificable en tiempo de ejecucion si el archivo cambia
	char** sem_ids;
	char** semInit;
	char** io_ids;
	char** ioSleep;
	char** sharedVars;
	int puertoUMC;
	char* ipUMC;
	int stack_size;
} customConfig_t;
// Estructuras Administrativas
t_queue* colaListos;
t_queue* colaSalida;
t_queue* colaCPU;
t_list* listaProcesos;
t_dictionary* tablaGlobales;
t_dictionary* tablaSEM;
t_dictionary* tablaIO;
//Configuraciones
customConfig_t config;
t_config* configNucleo;
t_planificacion algoritmo;
struct timeval espera;
// Nucleo
void cargarConfiguracion();
void procesarHeader(int cliente, char *header);
void finalizar();
int getHandshake();
void warnDebug();
void crearSemaforos();
void crearIOs();
void crearCompartidas();
void destruirSemaforo(t_semaforo* sem);
void destruirSemaforos();
void destruirIO(t_IO* io);
void destruirIOs();
void destruirCompartida(int* compartida);
void destruirCompartidas();
void atenderHandshake(int cliente);
void iniciarVigilanciaConfiguracion();
void procesarCambiosConfiguracion();
void finalizarConsola(int cliente);
void finalizarCPU(int cliente);
void finalizarCliente(int cliente);
void sigusr1(int cpu);
// UMC
bool pedirPaginas(t_proceso* proceso, char* codigo);
void establecerConexionConUMC();
void conectarAUMC();
void handshakearUMC();
void recibirTamanioPagina();
void enviarStackSize();
// Consola
char* getScript(int consola,bool* exploto);
// CPU
void ingresarCPU(int cliente);
// Procesos
void liberarRecursos(t_proceso* proceso);
HILO crearProceso(int consola);
void rechazarProceso(t_proceso* proceso);
void bloquearProceso(t_proceso* proceso);
void bloquearProcesoIO(int cliente, char* IO, int tiempo);
void bloquearProcesoSem(int cliente, char* semid);
void desbloquearProceso(t_proceso* proceso);
void finalizarProceso(int PID);
void destruirProceso(t_proceso* proceso);
void actualizarPCB(t_proceso* proceso, t_PCB* PCB);
void asignarMetadataProceso(t_proceso* p, char* codigo);
t_proceso* obtenerProceso(int PID);
// Planificacion
HILO planificar();
void planificacionFIFO();
void planificacionRR();
void planificarExpulsion();
void planificarProcesos();
void planificarIO(char* io_id, t_IO* io);
bool terminoQuantum(t_proceso* proceso);
void asignarCPU(t_proceso* proceso, int cpu);
void desasignarCPU(t_proceso* proceso);
HILO bloqueo(t_bloqueo* info);
void cambiarEstado(t_proceso* proceso, int estado);
void continuarProceso(t_proceso* proceso);
void expulsarProceso(t_proceso* proceso);
void ejecutarProceso(t_proceso* proceso, int cpu);
void rafagaProceso(int cliente);
bool procesoExiste(t_proceso* proceso);
bool clienteExiste(int cliente);
void limpiarColaListos();
void limpiarColaCPU();
// Primitivas
void recibirSignal(int cliente);
void recibirWait(int cliente);
void primitivaSignal(int cliente, char* semid);
void primitivaWait(int cliente, char* semid);
void recibirAsignarCompartida(int cliente);
void recibirDevolverCompartida(int cliente);
void primitivaAsignarCompartida(char* compartida, int valor);
int primitivaDevolverCompartida(char* compartida);
void imprimirVariable(int cliente);
void imprimirTexto(int cliente);
void entradaSalida(int cliente);
// Tests
int testear(int (*suite)(void));
int test_nucleo();
void test_cicloDeVidaProcesos();
void test_obtenerMetadata();

// Imprimir
void queue_iterate(t_queue* self, void (*closure)(void*));
void imprimirPIDenCola(t_proceso* procesoEnCola);
void imprimirBloqueo(t_bloqueo* bloqueo);
void imprimirColasIO(char* key, t_IO* io);
void imprimirColasSemaforos(char* key, t_semaforo* sem);
void imprimirColaListos();
void imprimirColas();
#endif /* NUCLEO_H_ */
