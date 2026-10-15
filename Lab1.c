  GNU nano 8.7.1                                  lab1.c                                           
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
