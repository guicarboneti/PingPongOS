#include <stdlib.h>
#include <signal.h>
#include <stdio.h>
#include "ppos.h"
#include "ppos_data.h"
#include "ppos_disk.h"
#include "disk.h"

extern task_t *QueueReady;
disk_t disk;
task_t mngDiskTask;
extern task_t *CurrTask;
int signal_disk;
struct sigaction action2;
void handle_signal();

static void handleDiskDriver () {
    mngDiskTask.status = 'R';
    queue_append((queue_t **) &QueueReady, (queue_t *) &mngDiskTask);
}

static void taskSuspend (task_t *task) {

    task->status = 'S';
    queue_remove((queue_t **) &QueueReady, (queue_t *) task);
    queue_append((queue_t **) &disk.queue, (queue_t *) task);
}

static void diskManagerSuspend () {
    mngDiskTask.status = 'S';
    queue_remove((queue_t **) &QueueReady, (queue_t *) &mngDiskTask);
}

static void taskEnable (task_t *task) {

    task->status = 'R';
    queue_remove((queue_t **) &disk.queue, (queue_t *) task);
    queue_append((queue_t **) &QueueReady, (queue_t *) task);
}

static request_t *request(char reqType, void *buf, int block) {

    request_t *req = malloc (sizeof (request_t));
    if (!req) return 0;

    req->req = CurrTask;
	req->type = reqType;
	req->buffer = buf;
	req->block = block;
    req->next = req->prev = NULL;

    return req;
}


void diskDriverBody() {
    
    while (1) {
        
        sem_down(&disk.sem);

        if (signal_disk) {
            task_t *task = disk.request->req;
            taskEnable(task);

            queue_remove((queue_t **) &disk.req_queue, (queue_t *) disk.request);

            signal_disk = 0;
            free(disk.request);
        }
        int status_disk = disk_cmd (DISK_CMD_STATUS, 0, 0);
        if ((status_disk == 1) && (disk.req_queue)) {
            disk.request = disk.req_queue;
            if (disk.request->type == 'R')
                disk_cmd (DISK_CMD_READ, disk.request->block, disk.request->buffer);
            else
                disk_cmd (DISK_CMD_WRITE, disk.request->block, disk.request->buffer);
        }

        sem_up(&disk.sem);
        diskManagerSuspend();

        task_yield ();
    }
}

int disk_mgr_init (int *numBlocks, int *blockSize) {

    if (disk_cmd(DISK_CMD_INIT, 0, 0) || ((*numBlocks = disk_cmd(DISK_CMD_DISKSIZE, 0, 0)) < 0) || ((*blockSize = disk_cmd(DISK_CMD_BLOCKSIZE, 0, 0)) < 0))
        return -1;

    signal_disk = 0;
    sem_create(&disk.sem, 1);
    task_create(&mngDiskTask, diskDriverBody, NULL);
    diskManagerSuspend();

    action2.sa_handler = handle_signal;
    sigemptyset (&action2.sa_mask) ;
    action2.sa_flags = 0 ;
    if (sigaction (SIGUSR1, &action2, 0) < 0) {
        perror ("Erro em sigaction: ");
        exit(1);
    }

    return 0;
}

int disk_block_read (int block, void *buf) {
    sem_down(&disk.sem);
    
    request_t *req = request('R', buf, block);
    if (!req) return -1;
    queue_append((queue_t **) &disk.req_queue, (queue_t *) req);
    if (mngDiskTask.status == 'S')
        handleDiskDriver();

    sem_up(&disk.sem);
    taskSuspend(CurrTask);

    task_yield();
    return 0;
}

int disk_block_write (int block, void *buf) {
    sem_down(&disk.sem);

    request_t *newRequest = request('W', buf, block);
    if (!newRequest) return -1;
    queue_append((queue_t **) &disk.req_queue, (queue_t *) newRequest);
    if (mngDiskTask.status == 'S')
        handleDiskDriver();

    sem_up(&disk.sem);
    taskSuspend(CurrTask);

    task_yield();
    return 0;
}

void handle_signal () {
    signal_disk = 1;
    if (mngDiskTask.status == 'S')
        handleDiskDriver();
}