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

unsigned int n_ticks;           // contador de ticks de relogio
unsigned int begin_task_time;   // início do tempo de processamento da tarefa a cada bloco de execucao
struct sigaction action;        // estrutura que define um tratador de sinal
struct itimerval timer;         // estrutura de inicialização to timer

// Estados das tarefas: Pronta (P), Terminada (T), Executando (E), Suspensa (S), Dormindo (D)
task_t *PrevTask, *CurrTask, MainTask, Dispatcher, *QueueReady, *QueueSleep;  // O dispatcher é implementado como uma tarefa
int TasksReadyCounter=0, last_id=0, temporizador;

static void dispatcher();
static void tratador();
static void setTemporizador();
unsigned int systime();
int task_join (task_t *task);
void task_suspend (task_t **queue);
void task_resume (task_t * task, task_t **queue);

// Inicializa o sistema operacional; deve ser chamada no inicio do main()
void ppos_init () {
    setvbuf (stdout, 0, _IONBF, 0); // desativa o buffer da saida padrao (stdout), usado pela função printf

    getcontext(&MainTask.context);

    last_id=1;
    MainTask.id = 0;
    MainTask.status = 'E';
    MainTask.next = NULL;
    MainTask.prev = NULL;
    MainTask.dynamic_prio = 0;
    MainTask.static_prio = 0;
    MainTask.begin_execution_time = systime();
    MainTask.processor_time = 0;
    MainTask.join_queue = NULL;
    MainTask.activations=0;
    CurrTask = &MainTask;

    TasksReadyCounter++;

    task_create(&Dispatcher, dispatcher, NULL);

    n_ticks = 0;
    begin_task_time = 0;
    setTemporizador();

    task_yield();
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

    task->id = last_id;
    last_id++;
    task->prev = NULL;
    task->next = NULL;
    task->status = 'P';
    task->static_prio = 0;
    task->dynamic_prio = 0;
    makecontext (&task->context, (void*)(*start_func), 1, arg);
    task->activations = 0;
    task->begin_execution_time = systime();
    task->processor_time = 0;
    task->join_queue = NULL;

    // coloca tarefa no fim da fila de tarefas prontas se a task não for o dispatcher
    if (task != &Dispatcher) {
        queue_append((queue_t **) &QueueReady, (queue_t *) task);
        TasksReadyCounter++;
    }

    return task->id;
}

// Termina a tarefa corrente, indicando um valor de status encerramento
void task_exit (int exit_code) {
    // imprime informacoes da tarefa finalizada
    unsigned int execution_time = (systime() - CurrTask->begin_execution_time);
    CurrTask->processor_time += (systime() - begin_task_time);
    printf("Task %d exit: execution time %u ms, processor time %u ms, %d activations\n", CurrTask->id, execution_time, CurrTask->processor_time, CurrTask->activations);
    
    CurrTask->status = 'T';
    CurrTask->exit_code = exit_code;

    while (queue_size((queue_t *)CurrTask->join_queue) > 0)
        task_resume(CurrTask->join_queue, &CurrTask->join_queue);

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

    if (task == &Dispatcher)
        Dispatcher.activations++;

    // atualiza o estado das tarefas
    CurrTask->status = 'E';
    if (PrevTask->status != 'T') {
        PrevTask->processor_time += (systime() - begin_task_time);
    }

    begin_task_time = systime();

    if (swapcontext(&PrevTask->context, &CurrTask->context) == -1)
        return -1;

    return 0;
}

// retorna o identificador da tarefa corrente (main deve ser 0)
int task_id () {
    return CurrTask->id;
}

// devolve o processador ao dispatcher:
void task_yield () {
    CurrTask->status = 'P';     // volta pra fila de prontas
    queue_append((queue_t **)&QueueReady, (queue_t *)CurrTask);
    task_switch(&Dispatcher);   // retorna ao dispatcher
}

// retorna a próxima tarefa a ativar - tarefa mais prioritária
task_t *scheduler() {
    task_t *task_max_prio = QueueReady;
    if (!task_max_prio)
        return NULL;

    // percorre a fila e encontra a tarefa mais prioritária
    int max_prio = task_max_prio->dynamic_prio;
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

void awake_tasks () {
    int i;
    task_t *aux = QueueSleep;
    for (i=0; i<queue_size((queue_t *) QueueSleep); i++, aux=aux->next) {
        if (systime() >= aux->awake_time) {    // verifica se tarefa ja pode ser acordada
            queue_remove((queue_t **)&QueueSleep, (queue_t *)aux);
            aux->status = 'P';
            queue_append((queue_t **)&QueueReady, (queue_t *)aux);
        }
    }
}

void dispatcher() {
    task_t *nextTask;
    // enquanto houverem tarefas de usuário
    for (; TasksReadyCounter > 0 ;) {
        awake_tasks();  // acorda tarefas

        // escolhe a próxima tarefa a executar
        nextTask = scheduler();

        // escalonador escolheu uma tarefa?
        if (nextTask != NULL) {
            queue_remove((queue_t **)&QueueReady, (queue_t *)nextTask);
            nextTask->activations++;
            temporizador = QUANTUM;
            // transfere controle para a próxima tarefa
            task_switch (nextTask);
            
            // voltando ao dispatcher, trata a tarefa de acordo com seu estado
            switch (PrevTask->status) {
                case ('P'):
                break;

                case ('T'):
                    queue_remove((queue_t **)&QueueReady, (queue_t *)PrevTask);
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
        temporizador--;
        if (temporizador == 0)
            task_yield();
    }
    n_ticks++;
}

// informar às tarefas o valor corrente do relógio
unsigned int systime () {
    return n_ticks;
}

int task_join (task_t *task) {

    if (!task)
        return -1;

    if (task->status != 'T')
        task_suspend(&task->join_queue);

    return task->exit_code;
}

void task_suspend (task_t **queue) {
    queue_remove((queue_t **)&QueueReady, (queue_t *)CurrTask);
    CurrTask->status = 'S';
    queue_append((queue_t **)queue, (queue_t *)CurrTask);
    task_switch(&Dispatcher);
}

void task_resume (task_t * task, task_t **queue) {
    queue_remove((queue_t **)queue, (queue_t *)task);
    task->status = 'P';
    queue_append((queue_t **)&QueueReady, (queue_t *)task);
}

// suspende apenas a tarefa corrente durante o intervalo t (em milissegundos)
void task_sleep (int t) {
    queue_remove((queue_t **)&QueueReady, (queue_t *)CurrTask);
    CurrTask->status = 'D';
    CurrTask->awake_time = systime () + t;
    queue_append((queue_t **)&QueueSleep, (queue_t *)CurrTask);
    task_switch(&Dispatcher);
}

void enter_cs (int *lock) {
    // atomic OR (Intel macro for GCC)
    while (__sync_fetch_and_or (lock, 1)) ;   // busy waiting
}

void leave_cs (int *lock) {
    (*lock) = 0 ;
}

/*
    Inicializa um semáforo apontado por s com o valor inicial value e uma fila vazia. 
    A chamada retorna 0 em caso de sucesso ou -1 em caso de erro.
*/
int sem_create (semaphore_t *s, int value) {
    if (!s) return -1;

    s->counter = value;
    s->queue = NULL;
    s->lock = 0;
    return 0;
}

/*
    Realiza a operação Down no semáforo apontado por s.
    A chamada retorna 0 em caso de sucesso ou -1 em caso de erro (semáforo não existe ou foi destruído).
*/
int sem_down (semaphore_t *s) {
    if (!s) return -1;

    enter_cs(&s->lock);
    s->counter--;
    leave_cs(&s->lock);
    if (s->counter < 0)
        task_suspend(&s->queue);
    return 0;
}

/*
    Realiza a operação Up no semáforo apontado por s.
    A chamada retorna 0 em caso de sucesso ou -1 em caso de erro (semáforo não existe ou foi destruído).
*/
int sem_up (semaphore_t *s) {
    if (!s || !s->queue) return -1;

    enter_cs(&s->lock);
    s->counter++;
    leave_cs(&s->lock);
    if (s->counter <= 0) {
        task_resume(s->queue, &s->queue);
    }
    return 0;
}

/*
    Destrói o semáforo apontado por s, acordando todas as tarefas que aguardavam por ele.
    A chamada retorna 0 em caso de sucesso ou -1 em caso de erro. 
*/
int sem_destroy (semaphore_t *s) {
    if (!s || !s->queue) return -1;
    
    for (;s->queue;) {
        task_resume(s->queue, &s->queue);
    }

    s->counter=0;
    s->lock=0;
    s->queue = NULL;
    s = NULL;
    return 0;
}