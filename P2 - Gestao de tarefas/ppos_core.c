#include <stdio.h>
#include <stdlib.h>
#include <ucontext.h>
#include "ppos_data.h"
#include "ppos.h"
#define DEBUG
#define STACKSIZE 64*1024	/* tamanho de pilha das threads */

int last_id=0;
task_t *PrevTask, *CurrTask, MainTask;

// Inicializa o sistema operacional; deve ser chamada no inicio do main()
void ppos_init () {
    setvbuf (stdout, 0, _IONBF, 0); // desativa o buffer da saida padrao (stdout), usado pela função printf
    MainTask.id = 0;
    CurrTask = &MainTask;
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
    makecontext (&task->context, (void*)(*start_func), 1, arg) ;
    return task->id;
}

// Termina a tarefa corrente, indicando um valor de status encerramento
void task_exit (int exit_code) {
    task_switch(&MainTask);
    return;
}

// alterna a execução para a tarefa indicada
int task_switch (task_t *task) {
    if (!task) return -1;

    PrevTask = CurrTask;
    CurrTask = task;
    if (swapcontext(&PrevTask->context, &CurrTask->context) == -1)
        return -1;

    return 0;
}

// retorna o identificador da tarefa corrente (main deve ser 0)
int task_id () {
    return CurrTask->id;
}