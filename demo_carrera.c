#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sched.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <semaphore.h>

#define N 100000

typedef struct {
    volatile long contador;
    sem_t mutex;
} Compartido;

int main(int argc, char **argv) {
    int usar_sem = (argc > 1 && argv[1][0] == 'c');   /* "c" = con semáforo */

    int id = shmget(IPC_PRIVATE, sizeof(Compartido), IPC_CREAT | 0600);
    Compartido *c = (Compartido *) shmat(id, NULL, 0);
    c->contador = 0;
    sem_init(&c->mutex, 1, 1);

    for (int p = 0; p < 2; p++) {
        if (fork() == 0) {
            for (long i = 0; i < N; i++) {
                if (usar_sem) sem_wait(&c->mutex);

                long tmp = c->contador;
                sched_yield();
                c->contador = tmp + 1;

                if (usar_sem) sem_post(&c->mutex);
            }
            shmdt(c);
            exit(0);
        }
    }
    wait(NULL);
    wait(NULL);

    printf("%s | esperado: %d | obtenido: %ld\n",
           usar_sem ? "CON semaforo" : "SIN semaforo", 2 * N, c->contador);

    sem_destroy(&c->mutex);
    shmdt(c);
    shmctl(id, IPC_RMID, NULL);
    return 0;
}
