#include <stdio.h>          /* printf()                 */
#include <stdlib.h>         /* exit(), malloc(), free() */
#include <sys/types.h>      /* key_t, sem_t, pid_t      */
#include <sys/shm.h>        /* shmat(), IPC_RMID        */
#include <errno.h>          /* errno, ECHILD            */
#include <semaphore.h>      /* sem_open(), sem_destroy(), sem_wait().. */
#include <fcntl.h>          /* O_CREAT, O_EXEC          */
#include <pthread.h>

#include <time.h>

#include "gol.h"

/******************************************************/
/******************   PARENT PROCESS   ****************/
/******************************************************/

pthread_mutex_t lock;

struct Qsem {
    int value; // keeps the amount of processes
    sem_t *sem1;
    sem_t *sem2;
    int amount_of_started;
    int amount_of_ended;
} qse;

// Кладем процессы только вошедшие в работу в сон
void Qsem_sem_start(struct Qsem *qsem){
    pthread_mutex_lock(&lock);
    qsem->amount_of_started++;
    if (qsem->amount_of_started == qsem->value) { Qsem_clear1(qsem); }
    sem_wait(qsem->sem1);
    pthread_mutex_unlock(&lock);
}

// Когда все процессы пришли уснуть (не уснул только тот, что вызывал эту функцию последний)
// Добавляем в семафор значения, тем самым будим процессы и даем им работать
void Qsem_clear1(struct Qsem *qsem){
    int i = 0;
    for (i = 0; i < qsem->value; i++)
        sem_post(qsem->sem1);
    qsem->amount_of_started = 0;

}

// Кладем процессы только вошедшие в работу в сон
void Qsem_sem_end(struct Qsem *qsem){
    pthread_mutex_lock(&lock);
    qsem->amount_of_ended++;
    if (qsem->amount_of_ended == qsem->value) { Qsem_clear2(qsem); }
    sem_wait(qsem->sem2);
    pthread_mutex_unlock(&lock);
}

// Когда все процессы пришли уснуть (не уснул только тот, что вызывал эту функцию последний)
// Добавляем в семафор значения, тем самым будим процессы и даем им работать
void Qsem_clear2(struct Qsem *qsem){
    int i = 0;
    for (i = 0; i < qsem->value; i++)
        sem_post(qsem->sem2);
    qsem->amount_of_ended = 0;
    printf("\n\n Each process has ended it's journey --------------------------------- \n\n");
}

int main (int argc, char **argv){

/**************************** shared memory related stuff *****************************/

    key_t shmkey;           /*      shared memory key       */
    int shmid;              /*      shared memory id        */
    int *p;                 /*      shared variable         */

    int m;
    printf ("How big plaing field do you want to get (m*m)?\nPlease, enter 'm': ");
    scanf ("%u", &m);

    int needed_size = m*m*2;
    shmkey = ftok ("/dev/null", 5);
    printf ("shmkey for p = %d\n", shmkey);
    struct Qsem* ptr3;
    shmid = shmget (shmkey, needed_size*sizeof (int)+sizeof(ptr3), 0644 | IPC_CREAT);
    if (shmid < 0) {
        perror ("shmget\n");
        exit (1);
    }

    p = (int *) shmat (shmid, NULL, 0);   /* attach p to shared memory */
    *p = 0;
    printf ("p=%d is allocated in shared memory.\n\n", *p);
    int* ptr1 = p;
    int* ptr2 = p + m*m;
    ptr3 = p + m*m*2;

/**************************** shared memory related stuff *****************************/

    int i;                        /*      loop variables          */
    sem_t *sem1;                   /*      synch semaphore         *//*shared */
    sem_t *sem2;
    pid_t pid;                    /*      fork pid                */
    unsigned int n;               /*      fork count              */
    unsigned int value;           /*      semaphore value         */

    printf ("How many processes you want to work (n*n)?\n");
    printf ("Fork count: ");
    scanf ("%u", &n);

    int amount_of_forks = n*n;
    value = amount_of_forks;

    /* initialize semaphores for shared processes */
    sem1 = sem_open ("pSem1", O_CREAT | O_EXCL, 0644, 0);
    sem2 = sem_open ("pSem2", O_CREAT | O_EXCL, 0644, 0);
    /* name of semaphore is "pSem", semaphore is reached using this name */
    sem_unlink ("pSem1");
    sem_unlink ("pSem2");
    /* unlink prevents the semaphore existing forever */
    /* if a crash occurs during the execution         */
    printf ("semaphores initialized.\n\n");

    ptr3->sem1 = sem1;
    ptr3->sem2 = sem2;
    ptr3->value = value;
    ptr3->amount_of_started = 0;
    ptr3->amount_of_ended = 0;

    srand(time(NULL));
    int r = 0;
    /* fork child processes */
    for (i = 0; i < amount_of_forks; i++) {
        pid = fork ();  // should only be called once
        r = rand();      // returns a pseudo-random integer between 0 and RAND_MAX
        if (pid < 0)              /* check for error      */
            printf ("Fork error.\n");
        else if (pid == 0)
            break;                  /* child processes */
    }

    /******************************************************/
    /******************   PARENT PROCESS   ****************/
    /******************************************************/
    if (pid != 0){
        /* wait for all children to exit */
        while (pid = waitpid (-1, NULL, 0)){
            if (errno == ECHILD)
                break;
        }

        printf ("\nParent: All children have exited.\n");

        printf ("\nShowing ptr1 and ptr2:\n");

        int z = 0;
        int k = 0;
        for (z = 0; z < m; z++)
        {
            for (k = 0; k < m; k++)
            {
                printf("%i ",ptr1[z*m+k]);
            }
        }
        for (z = 0; z < m; z++)
        {
            for (k = 0; k < m; k++)
            {
                printf("%i ",ptr2[z*m+k]);
            }
        }

        /* shared memory detach */
        shmdt (p);
        shmctl (shmid, IPC_RMID, 0);

        /* cleanup semaphores */
        sem_destroy (sem1);
        sem_destroy (sem2);
        exit (0);
    }

    /******************************************************/
    /******************   CHILD PROCESS   *****************/
    /******************************************************/
    else{
        int a = 0;
        for ( a = 0; a < 3; a++)
        {
            Qsem_sem_start(ptr3);
//            sleep(1);
            printf("Child(%d) has done what he had to be having done.\n", i);

            int z = 0;
            int k = 0;
            for (z = 0; z < m; z++)
            {
                for (k = 0; k < m; k++)
                {
                    ptr1[i] = 1;
                    ptr2[i] = -1;
                }
            }


            Qsem_sem_end(ptr3);
        }
        exit(0);
    }
}


//            sleep(1);
//            ptr1[0] += i * 10;              /* increment *p by 0, 1 or 2 based on i */
//            ptr2[0] += i * 10000;              /* increment *p by 0, 1 or 2 based on i */
//            printf("  Child(%d) new value of *ptr1=%d.\n", i, ptr1[0]);
//            printf("  Child(%d) new value of *ptr2=%d.\n", i, ptr2[0]);