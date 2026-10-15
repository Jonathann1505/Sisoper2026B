#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <sys/wait.h>
#include <semaphore.h>

#define FIFO_FILE "canal_fifo"
#define BUFFER_SIZE 128
#define MAX_PARES 5

typedef struct {
    int a;
    int b;
    int resultado_mul;
    int resultado_sum;
    int listo;
    int fin;        // 1 = no habrá más pares
    sem_t vacio;    // 1 = el espacio está libre para escribir
    sem_t lleno;    // 1 = hay un par disponible para leer
} DatosCompartidos;

int main() {
    int shm_id;
    DatosCompartidos *mem;
    pid_t pid_prod, pid_cons1, pid_cons2;

    // Crear memoria compartida
    shm_id = shmget(IPC_PRIVATE, sizeof(DatosCompartidos), IPC_CREAT | 0666);
    if (shm_id < 0) {
        perror("Error creando memoria compartida");
        exit(1);
    }

    mem = (DatosCompartidos *) shmat(shm_id, NULL, 0);
    if (mem == (DatosCompartidos *) -1) {
        perror("Error asociando memoria compartida");
        exit(1);
    }

    // Inicializar datos y semáforos (antes de los fork)
    memset(mem, 0, sizeof(*mem));
    sem_init(&mem->vacio, 1, 1);
    sem_init(&mem->lleno, 1, 0);

    // Crear FIFO si no existe
    mkfifo(FIFO_FILE, 0666);

    pid_prod = fork();

    if (pid_prod == 0) {
        // Proceso Productor
        int fd = open(FIFO_FILE, O_WRONLY);
        if (fd < 0) {
            perror("Error abriendo FIFO para escritura");
            exit(1);
        }

        for (int i = 1; i <= MAX_PARES; i++) {
            int a = i, b = i + 2;
            char buffer[BUFFER_SIZE];
            snprintf(buffer, BUFFER_SIZE, "%d %d", a, b);
            printf("[Productor] Enviado: %s\n", buffer);
            fflush(stdout);
            write(fd, buffer, strlen(buffer) + 1);
            sleep(1);
        }

        close(fd);
        shmdt(mem);
        exit(0);
    } else {
        pid_cons1 = fork();
        if (pid_cons1 == 0) {
            // Consumidor 1: lee el FIFO, publica el par y calcula a*b
            int fd = open(FIFO_FILE, O_RDONLY);
            if (fd < 0) {
                perror("Error abriendo FIFO para lectura");
                exit(1);
            }

            int *resultados = malloc(MAX_PARES * sizeof(int));
            if (resultados == NULL) {
                perror("Error en malloc");
                exit(1);
            }

            char buffer[BUFFER_SIZE];
            int n = 0;

            while (n < MAX_PARES) {
                // leer un mensaje completo (hasta el '\0')
                int i = 0;
                char c;
                while (i < BUFFER_SIZE - 1 && read(fd, &c, 1) > 0) {
                    buffer[i++] = c;
                    if (c == '\0') break;
                }
                if (i == 0) break;      // el productor cerró el FIFO
                buffer[i] = '\0';

                int a, b;
                sscanf(buffer, "%d %d", &a, &b);

                // publicar el par para el consumidor 2
                sem_wait(&mem->vacio);
                mem->a = a;
                mem->b = b;
                sem_post(&mem->lleno);

                // calcular a*b
                resultados[n] = a * b;
                mem->resultado_mul = resultados[n];
                printf("[Consumidor 1] %d * %d = %d\n", a, b, resultados[n]);
                fflush(stdout);
                n++;
            }

            // avisar al consumidor 2 que ya no hay más datos
            sem_wait(&mem->vacio);
            mem->fin = 1;
            sem_post(&mem->lleno);

            printf("[Consumidor 1] Resultados finales: ");
            for (int i = 0; i < n; i++) {
                printf("%d ", resultados[i]);
            }
            printf("\n");
            fflush(stdout);

            free(resultados);
            close(fd);
            shmdt(mem);
            exit(0);
        } else {
            pid_cons2 = fork();
            if (pid_cons2 == 0) {
                // Consumidor 2: toma los pares de la memoria compartida y calcula a+b
                int *resultados = malloc(MAX_PARES * sizeof(int));
                if (resultados == NULL) {
                    perror("Error en malloc");
                    exit(1);
                }

                int n = 0;

                for (;;) {
                    sem_wait(&mem->lleno);      // espera sin gastar CPU
                    if (mem->fin) break;

                    int a = mem->a;
                    int b = mem->b;
                    sem_post(&mem->vacio);      // libera el espacio

                    if (n < MAX_PARES) {
                        resultados[n] = a + b;
                        mem->resultado_sum = resultados[n];
                        printf("[Consumidor 2] %d + %d = %d\n", a, b, resultados[n]);
                        fflush(stdout);
                        n++;
                    }
                }

                printf("[Consumidor 2] Resultados finales: ");
                for (int i = 0; i < n; i++) {
                    printf("%d ", resultados[i]);
                }
                printf("\n");
                fflush(stdout);

                free(resultados);
                shmdt(mem);
              exit(0);
           }
        }
    }

    // El padre espera a los 3 hijos y limpia
    for (int i = 0; i < 3; i++) wait(NULL);

    sem_destroy(&mem->vacio);
    sem_destroy(&mem->lleno);
    shmdt(mem);
    shmctl(shm_id, IPC_RMID, NULL);
    unlink(FIFO_FILE);

	return 0;
}
