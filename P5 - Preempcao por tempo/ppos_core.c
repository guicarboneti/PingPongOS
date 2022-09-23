// Guilherme Carbonari Boneti GRR20196478

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/time.h>
#include "ppos_data.h"
#include "ppos.h"
#include "queue.h"

#define DEBUG
#define STACKSIZE 64*1024	/* tamanho de pilha das threads */
#define TASKAGING_ALPHA -1
#define MIN_PRIO 20     // menos prioritária
#define MAX_PRIO -20    // mais prioritária
#define QUANTUM 20;     // fatia de tempo de cada tarefa em ticks

struct sigaction action;   // estrutura que define um tratador de sinal
struct itimerval timer;    // estrutura de inicialização to timer

// Estados das tarefas: Pronta (P), Terminada (T), Executando (E)
task_t *PrevTask, *CurrTask, MainTask, Dispatcher, *QueueReady;  // O dispatcher é implementado como uma tarefa
int TasksReadyCounter=0, last_id=0, temporizador;

static void dispatcher();
static void tratador();
static void setTemporizador();

// Inicializa o sistema operacional; deve ser chamada no inicio do main()
void ppos_init () {
    setvbuf (stdout, 0, _IONBF, 0); // desativa o buffer da saida padrao (stdout), usado pela função printf

    MainTask.id = 0;
    MainTask.next = NULL;
    MainTask.prev = NULL;
    MainTask.dynamic_prio = 0;
    MainTask.static_prio = 0;

    CurrTask = &MainTask;
    task_create(&Dispatcher, dispatcher, NULL);

    setTemporizador();
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
    task->status = 'P';
    task->static_prio = 0;
    task->dynamic_prio = 0;
    makecontext (&task->context, (void*)(*start_func), 1, arg);

    // coloca tarefa no fim da fila de tarefas prontas se a task não for o dispatcher
    if (task != &Dispatcher) {
        queue_append((queue_t **) &QueueReady, (queue_t *) task);
        TasksReadyCounter++;
    }

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

    temporizador = QUANTUM;

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
    task_switch(&Dispatcher);   // retorna ao dispatcher
}

// retorna a próxima tarefa a ativar - tarefa mais prioritária
task_t *scheduler() {
    task_t *task_max_prio = QueueReady;
    int max_prio = task_max_prio->dynamic_prio;
    if (!task_max_prio)
        return NULL;

    // percorre a fila e encontra a tarefa mais prioritária
    task_max_prio->dynamic_prio += TASKAGING_ALPHA;     // aplica fator de envelhecimento na primeira tarefa da fila
    task_t *aux = QueueReady->next;
    for (; aux != QueueReady; aux=aux->next) {
        if (aux->dynamic_prio < max_prio) {
            task_max_prio = aux;
            max_prio = task_max_prio->dynamic_prio;
        }
        aux->dynamic_prio += TASKAGING_ALPHA;   // aplica o fator de envelhecimento na tarefa
    }

    task_max_prio->dynamic_prio = task_max_prio->static_prio;
    return task_max_prio;
}

void dispatcher() {
    task_t *nextTask;

    while (1) {
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
    }
}

/* ajusta a prioridade estática da tarefa task para o valor prio (que deve estar entre -20 e +20) 
   caso task seja nulo, ajusta a prioridade da tarefa atual. */
void task_setprio (task_t *task, int prio) {
    if (!task)
        task = CurrTask;
    if (prio > MIN_PRIO)
        task->static_prio = MIN_PRIO;
    else if (prio < MAX_PRIO)
        task->static_prio = MAX_PRIO;
    else
        task->static_prio = prio;
    task->dynamic_prio = task->static_prio;
}

// devolve o valor da prioridade estática da tarefa task (ou da tarefa corrente, se task for nulo).
int task_getprio (task_t *task) {
    if (!task)
        return CurrTask->static_prio;
    return task->static_prio;
}

// inicializa e ajusta temporizador
void setTemporizador() {

    // quantum recebido pela tarefa
    temporizador = QUANTUM;

    // registra a ação para o sinal de timer SIGALRM
    action.sa_handler = tratador ;
    sigemptyset (&action.sa_mask) ;
    action.sa_flags = 0 ;
    if (sigaction (SIGALRM, &action, 0) < 0) {
        perror ("Erro em sigaction: ") ;
        exit (1) ;
    }

    // ajusta valores do temporizador
    timer.it_value.tv_usec = 1000 ;      // primeiro disparo, em micro-segundos
    timer.it_value.tv_sec  = 0 ;      // primeiro disparo, em segundos
    timer.it_interval.tv_usec = 1000 ;   // disparos subsequentes, em micro-segundos
    timer.it_interval.tv_sec  = 0 ;   // disparos subsequentes, em segundos

    // arma o temporizador ITIMER_REAL (vide man setitimer)
    if (setitimer (ITIMER_REAL, &timer, 0) < 0) {
        perror ("Erro em setitimer: ") ;
        exit (1) ;
    }
}

// rotina de tratamento de ticks
void tratador() {
    if (CurrTask != &Dispatcher) {
        if (temporizador == 0)
            task_yield();
        temporizador--;
    }
}