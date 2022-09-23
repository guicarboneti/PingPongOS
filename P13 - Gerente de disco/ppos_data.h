// PingPongOS - PingPong Operating System
// Prof. Carlos A. Maziero, DINF UFPR
// Versão 1.4 -- Janeiro de 2022

// Estruturas de dados internas do sistema operacional

#ifndef __PPOS_DATA__
#define __PPOS_DATA__

#include <ucontext.h>		// biblioteca POSIX de trocas de contexto
#include "queue.h"

// Estrutura que define um Task Control Block (TCB)
typedef struct task_t
{
  struct task_t *prev, *next ;		// ponteiros para usar em filas
  int id ;				// identificador da tarefa
  ucontext_t context ;			// contexto armazenado da tarefa
  short status ;			// pronta, rodando, suspensa, ...
  short preemptable ;			// pode ser preemptada?
  int dynamic_prio, static_prio;
  unsigned int begin_execution_time, processor_time;
  int activations;
  int exit_code;
  struct task_t *join_queue;
  unsigned int awake_time;
} task_t ;

// estrutura que define um semáforo
typedef struct
{
  int counter;
  struct task_t *queue;
  int lock;
} semaphore_t ;

// estrutura que define um mutex
typedef struct
{
  // preencher quando necessário
} mutex_t ;

// estrutura que define uma barreira
typedef struct
{
  // preencher quando necessário
} barrier_t ;

// estrutura que define uma fila de mensagens
typedef struct
{
  void *buffer;
  int buffer_size, msg_size;
  int first, last;
  short active;
  semaphore_t sendSem, recvSem, bufferSem;
} mqueue_t ;

#endif