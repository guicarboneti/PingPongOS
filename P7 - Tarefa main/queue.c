// Guilherme Carbonari Boneti GRR20196478

#include <stdio.h>
#include <stdlib.h>
#include "queue.h"

int queue_size (queue_t *queue) {
    if (queue==NULL || queue->prev==NULL || queue->next==NULL) {
        return 0;
    }
    else {
        queue_t *aux = queue->next;
        int cont = 1;
        for (; aux != queue ;) {
            if (aux==NULL || aux->prev==NULL || aux->next==NULL) {
                return 0;
            }
            cont++;
            aux = aux->next;
        }
        return cont;
    }
}

void queue_print (char *name, queue_t *queue, void print_elem (void*) ) {
    if (queue==NULL) {
        printf("%s: []\n", name);
        return;
    }
    printf("%s: [", name);
    print_elem(queue);
    queue_t *aux = queue->next;
    for (; aux != queue ;) {
        print_elem(aux);
        aux = aux->next;
    }
    printf("]");
}

int queue_append (queue_t **queue, queue_t *elem) {
    if (queue==NULL) {
        fprintf(stderr, "Erro: Fila não existe\n");
        return -1;
    }
    if (elem==NULL) {
        fprintf(stderr, "Erro: Elemento não existe\n");
        return -3;
    }
    if (elem->prev!=NULL || elem->next!=NULL) {
        fprintf(stderr, "Erro: Elemento já pertence a uma fila\n");
        return -4;
    }

    if (*queue==NULL) {     // fila vazia
        (*queue) = elem;
        elem->next = (*queue);
        elem->prev= (*queue);
    }
    else {
        elem->prev = (*queue)->prev;
        elem->next = (*queue);
        (*queue)->prev = elem;
        elem->prev->next = elem;
    }

    return 0;
}

int queue_remove (queue_t **queue, queue_t *elem) {
    if (queue==NULL) {
        fprintf(stderr, "Erro: Fila não existe\n");
        return -1;
    }
    if (*queue==NULL) {
        fprintf(stderr, "Erro: Fila vazia\n");
        return -2;
    }
    if (elem==NULL) {
        fprintf(stderr, "Erro: Elemento não existe\n");
        return -3;
    }

    if (*queue == elem) {
        if (elem->prev==elem && elem->next==elem) {
            (*queue)->prev=NULL;
            (*queue)->next=NULL;
            (*queue)=NULL;
        }
        else {
            elem->prev->next = elem->next;
            elem->next->prev = elem->prev;
            (*queue)=elem->next;
            elem->next=NULL;
            elem->prev=NULL;
        }
    }
    else {
        queue_t *aux = (*queue)->next;
        for (; aux != *queue ;) {
            if (aux == elem) {
                aux->prev->next = aux->next;
                aux->next->prev = aux->prev;
                aux->next=NULL;
                aux->prev=NULL;
                break;
            }
            aux = aux->next;
        }
        if (aux!=elem) {
            fprintf(stderr, "Erro: Elemento não pertence à fila\n");
            return -5;
        }
    }

    return 0;
}