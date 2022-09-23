#include <stdio.h>
#include <stdlib.h>
#include "ppos_data.h"
#include "ppos.h"
#include "queue.h"
#define BUF_SIZE 5
#define N_ITEMS 100000

semaphore_t *s_buffer ; // controla o acesso ao buffer
semaphore_t *s_item ; // controla os itens no buffer (inicia em 0)
semaphore_t *s_vaga ; // controla as vagas no buffer (inicia em N)
typedef struct filaint_t
{
    struct filaint_t *prev ;  // ptr para usar cast com queue_t
    struct filaint_t *next ;  // ptr para usar cast com queue_t
    int value ;
   // outros campos podem ser acrescidos aqui
} filaint_t ;
filaint_t *buffer;
filaint_t *aux;
filaint_t item[N_ITEMS];
task_t p1, p2, p3, c1, c2;
int count=0;

void print_elem (void *ptr)
{
   filaint_t *elem = ptr ;

    if (!elem)
        return ;

    printf ("<%d>", elem->value) ;
}



void produtor(void * task_name) {
    for (;;) {
        task_sleep (1000);
        item[count].value = random() % 100;

        sem_down (s_vaga);

        sem_down (s_buffer);
        queue_append ((queue_t **) &buffer, (queue_t *) &item[count]);
        if (queue_size((queue_t *) buffer) > 5)
            queue_remove((queue_t **) &buffer, (queue_t *) buffer);
        sem_up (s_buffer);

        sem_up (s_item);
        printf("%s produziu %d\n", (char *)task_name, item[count++].value);
    }
}

void consumidor(void * task_name) {
    for (;;) {
        aux = buffer ;
        sem_down (s_item);

        sem_down (s_buffer);
            queue_remove((queue_t **) &buffer, (queue_t *) aux);
        sem_up (s_buffer);

        sem_up (s_vaga);

        if (aux)
            printf("%s consumiu %d\n", (char *) task_name, aux->value);
        task_sleep(1000);
    }
}

int main() {
    ppos_init();

    sem_create(s_buffer, 1);
    sem_create(s_vaga, 5);
    sem_create(s_item, 0);

    // inicializa os N elementos
    for (int i=0; i<N_ITEMS; i++)
    {
        item[i].prev = NULL ;
        item[i].next = NULL ;
    }
    buffer=NULL;

    task_create(&p1, produtor, "p1");
    task_create(&p2, produtor, "p2");
    task_create(&p3, produtor, "p3");
    task_create(&c1, consumidor, "                             c1");
    task_create(&c2, consumidor, "                             c2");

    task_join(&p1);
    return 0;
}