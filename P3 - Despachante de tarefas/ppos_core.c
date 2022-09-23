#include <stdio.h>
#include <stdlib.h>
#include <ucontext.h>
#include "ppos_data.h"
#include "ppos.h"
#include "queue.h"

#define DEBUG
#define STACKSIZE 64*1024	/* tamanho de pilha das threads */

task_t *PrevTask, *CurrTask, MainTask, Dispatcher, *QueueReady;  // O dispatcher é implementado como uma tarefa
int TasksReadyCounter = -1;     // inicia em -1, tarefa Dispatcher não deve ser executada
int last_id=0;

// Estados das tarefas: Pronta (P), Terminada (T), Executando (E)
void dispatcher();   // declara função dispatcher

// Inicializa o sistema operacional; deve ser chamada no inicio do main()
void ppos_init () {
    setvbuf (stdout, 0, _IONBF, 0); // desativa o buffer da saida padrao (stdout), usado pela função printf
    
    MainTask.id = 0;
    MainTask.next = NULL;
    MainTask.prev = NULL;

    CurrTask = &MainTask;
    task_create(&Dispatcher, dispatcher, NULL);
}

// gerência de tarefas =========================================================

// Cria uma nova tarefa. Retorna um ID> 0 ou erro.
int task_create (task_t *task,			// descritor da nova tarefa
                 void (*start_func)(void *),	// funcao corpo da tarefa
                 void *arg) {       // argumentos para a tarefa
    getcontext (&task->context);

    char *stack ;
    stack = malloc (STACKSIZE) ;
    if (stack) {
        task->context.uc_stack.ss_sp = stack ;
        task->context.uc_stack.ss_size = STACKSIZE ;
        task->context.uc_stack.ss_flags = 0 ;
        task->context.uc_link = 0 ;
    } else {
        perror ("Erro na criação da pilha: \n");
        return -1;
    }

    task->id = ++last_id;
    makecontext (&task->context, (void*)(*start_func), 1, arg);
    task->status = 'P';

    // coloca tarefa no fim da fila de tarefas prontas
    queue_append((queue_t **) &QueueReady, (queue_t *) task);
    TasksReadyCounter++;

    return task->id;
}

// Termina a tarefa corrente, indicando um valor de status encerramento
void task_exit (int exit_code) {
    CurrTask->status = 'T';

    // quando uma tarefa encerra, retorna a execução para o dispatcher 
    if (CurrTask == &Dispatcher)
        task_switch(&MainTask);
    else
        task_switch(&Dispatcher);
    return;
}

// alterna a execução para a tarefa indicada
int task_switch (task_t *task) {
    if (!task) return -1;

    PrevTask = CurrTask;
    CurrTask = task;

    // atualiza o estado das tarefas
    CurrTask->status = 'E';
    if (PrevTask->status != 'T')
        PrevTask->status = 'P';

    if (swapcontext(&PrevTask->context, &CurrTask->context) == -1)
        return -1;

    return 0;
}

// retorna o identificador da tarefa corrente (main deve ser 0)
int task_id () {
    return CurrTask->id;
}

// coloca tarefa no final da fila de prontas e devolve o processador ao dispatcher:
void task_yield () {
    if (CurrTask->id != 0) { // não executa para tarefa Main
        queue_remove((queue_t **) &QueueReady, (queue_t *) CurrTask);   // remove da fila
        queue_append((queue_t **) &QueueReady, (queue_t *) CurrTask);   // recoloca no final da fila
    }

    task_switch(&Dispatcher);   // retorna ao dispatcher
}

// retorna a próxima tarefa a ativar, implementando política FCFS - First Come First Serve
task_t *scheduler() {
    return QueueReady->next;
}

void dispatcher() {
    task_t *nextTask;
    // enquanto houverem tarefas de usuário
    for (; TasksReadyCounter > 0 ;) {
        // escolhe a próxima tarefa a executar
        nextTask = scheduler();

        // escalonador escolheu uma tarefa?      
        if (nextTask != NULL) {

            // transfere controle para a próxima tarefa
            task_switch (nextTask);
            
            // voltando ao dispatcher, trata a tarefa de acordo com seu estado
            switch (PrevTask->status) {
                case ('P'):
                break;

                case ('T'):
                    queue_remove((queue_t **)&QueueReady, (queue_t *)PrevTask);     // remove da fila de prontas
                    TasksReadyCounter--;
                    free(PrevTask->context.uc_stack.ss_sp);     // libera estrutura da tarefa
                break;

                case ('E'):
                break;
            }       

        }
    }

    // encerra a tarefa dispatcher
    task_exit(0);
    return;
}