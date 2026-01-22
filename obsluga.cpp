#include "common.h"

SharedMemory* shm_ptr = nullptr; // globalny wskaznik dla handlera sygna³ow
void handle_sigterm(int sig) {}

// fun obslugujaca sygnaly
void handle_speed(int sig) {
    if (shm_ptr == nullptr) return;
    if (sig == SIGUSR1) {
        shm_ptr->chef_sleep_time /= 2; // Przyspiesz
        if (shm_ptr->chef_sleep_time < 200000) shm_ptr->chef_sleep_time = 200000;
        std::cout << "[OBSLUGA] Nowe tempo: " << shm_ptr->chef_sleep_time / 1000 << "ms" << std::endl;
    }
    else if (sig == SIGUSR2) {
        shm_ptr->chef_sleep_time *= 2; // Zwolnij
        std::cout << "[OBSLUGA] Nowe tempo: " << shm_ptr->chef_sleep_time / 1000 << "ms" << std::endl;
    }
}

int main() {

    signal(SIGTERM, handle_sigterm);
    // pobranie klucza i podpiêcie siê pod pamiêæ
    key_t key = ftok("main_restaurant", 'R');
    int shmid = shmget(key, sizeof(SharedMemory), 0666);
    int semid = semget(key, 4, 0666);

    if (shmid == -1 || semid == -1) { perror("Kierownik: b³¹d IPC"); exit(1); }

    shm_ptr = (SharedMemory*)shmat(shmid, NULL, 0);

    signal(SIGUSR1, handle_speed);
    signal(SIGUSR2, handle_speed);

    while (shm_ptr->isOpen) {

        // czekaj na wolne miejsce na tasmie
        sem_op(semid, SEM_KITCHEN_FULL, -1, shm_ptr);
        //sem_op(semid, SEM_EMPTY, -1, shm_ptr);
        sem_op(semid, SEM_MUTEX, -1);

        for (int i = MAX_BELT - 1; i > 0; i--) {
            if (!shm_ptr->belt[i].is_active && shm_ptr->belt[i - 1].is_active) {
                shm_ptr->belt[i] = shm_ptr->belt[i - 1];
                shm_ptr->belt[i - 1].is_active = false;
                shm_ptr->belt[i - 1].price = 0;
                shm_ptr->belt[i - 1].type = 0;
                shm_ptr->belt[i - 1].target_id = -1;
            }
        }

        if (shm_ptr->plates_in_kitchen > 0) {
            // Jeœli slot 0 jest wolny, k³adziemy
            if (!shm_ptr->belt[0].is_active) {
                struct sembuf s_op = { SEM_EMPTY, -1, IPC_NOWAIT };
                if (semop(semid, &s_op, 1) != -1) {
                    Plate taken_plate = shm_ptr->kitchen_prep[--shm_ptr->plates_in_kitchen];
                    shm_ptr->belt[0] = taken_plate;
                    shm_ptr->belt[0].is_active = true;
                    // Skoro pobieramy, musimy "skonsumowaæ" semafor KITCHEN_FULL bez czekania
                    struct sembuf s_op = { SEM_KITCHEN_FULL, -1, IPC_NOWAIT };
                    semop(semid, &s_op, 1);

                    std::cout << "[OBSLUGA] Klade danie na slot 0" << std::endl;

                    // Powiadom klientów, ¿e przyby³o jedzenie
                    sem_op(semid, SEM_FULL, 1);
                }
                else {
                    // Taœma pe³na
                    sem_op(semid, SEM_KITCHEN_FULL, 1);
                }
            }
            else {
                // Slot 0 zajêty - oddaj danie do puli i poczekaj
                sem_op(semid, SEM_KITCHEN_FULL, 1);
            }



            /*


                    for (int i = MAX_BELT - 1; i > 0; i--) {
                        if (!shm_ptr->belt[i].is_active && shm_ptr->belt[i - 1].is_active) {
                            shm_ptr->belt[i] = shm_ptr->belt[i - 1];
                            shm_ptr->belt[i - 1].is_active = false;
                        }
                    }

                    Plate taken_plate = shm_ptr->kitchen_prep[--shm_ptr->plates_in_kitchen];

                    int insert_pos = 0;
                    while (insert_pos < MAX_BELT && shm_ptr->belt[insert_pos].is_active) {
                        insert_pos++;
                    }
                    // znajdz pierwszy wolny slot na tasmie

                    if (insert_pos < MAX_BELT) {
                        shm_ptr->belt[insert_pos] = taken_plate;
                        shm_ptr->belt[insert_pos].is_active = true;
                        std::cout << "[OBSLUGA] Klade danie na tasmie (slot " << insert_pos << ")" << std::endl;
                    }

                    sem_op(semid, SEM_MUTEX, 1);
                    sem_op(semid, SEM_FULL, 1); // powiadom klienta

                    usleep(shm_ptr->chef_sleep_time); // czas wydawania dania
                }
                */
            
            usleep(shm_ptr->chef_sleep_time);
        }
        sem_op(semid, SEM_MUTEX, 1);
    }
    shmdt(shm_ptr);
    return 0;
}