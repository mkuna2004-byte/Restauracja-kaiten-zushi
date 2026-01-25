#include "common.h"

SharedMemory* shm_ptr = nullptr; // globalny wskaznik dla handlera sygna³ow

void handle_sigterm(int sig) {}

// fun obslugujaca sygnaly
void handle_speed(int sig) {
    if (shm_ptr == nullptr) return;
    if (sig == SIGUSR1) {
        shm_ptr->chef_sleep_time /= 2; // przyspiesz
        if (shm_ptr->chef_sleep_time < 50000) shm_ptr->chef_sleep_time = 50000;
        std::cout << "[OBSLUGA] Przyspieszamy, nowe tempo: " << shm_ptr->chef_sleep_time / 1000 << "ms" << std::endl;
    }
    else if (sig == SIGUSR2) {
        shm_ptr->chef_sleep_time *= 2; // zwolnij
        if (shm_ptr->chef_sleep_time > 2000000) shm_ptr->chef_sleep_time = 2000000;
        std::cout << "[OBSLUGA] Zwalniamy, nowe tempo: " << shm_ptr->chef_sleep_time / 1000 << "ms" << std::endl;
    }
}
/*
void handle_exit(int sig) {
    if (shm_ptr) shmdt(shm_ptr);
    exit(0);
}
*/
int main() {

    //signal(SIGTERM, [](int) {}); 
    signal(SIGINT, [](int) {}); // wybudza z semop
    signal(SIGUSR1, handle_speed); // klawisz 1
    signal(SIGUSR2, handle_speed); // klawisz 2
    signal(SIGALRM, SIG_IGN);

    // pobranie klucza i podpiêcie siê pod pamiêæ
    key_t key = ftok("main_restaurant", 'R');
    int shmid = shmget(key, sizeof(SharedMemory), 0666);
    int semid = semget(key, 4, 0666);

    if (shmid == -1 || semid == -1) { perror("Kierownik: b³¹d IPC"); exit(1); }

    shm_ptr = (SharedMemory*)shmat(shmid, NULL, 0);

    signal(SIGUSR1, handle_speed);
    signal(SIGUSR2, handle_speed);

    while (shm_ptr->isOpen || shm_ptr->active_clients > 0) {

        usleep(shm_ptr->chef_sleep_time);

        if (sem_op(semid, SEM_MUTEX, -1) == -1) break;

        if (!shm_ptr->isOpen) {
            sem_op(semid, SEM_MUTEX, 1);
            break;
        }

        int last_idx = MAX_BELT - 1;
        if (shm_ptr->belt[last_idx].is_active) {
            // danie do kosza
            struct sembuf s_waste = { SEM_FULL, -1, IPC_NOWAIT };
            semop(semid, &s_waste, 1);

            shm_ptr->belt[last_idx].is_active = false;
            shm_ptr->belt[last_idx].price = 0;
            std::cout << "[OBSLUGA] Danie spadlo z tasmy (zrobiono miejsce)." << std::endl;
        }

        for (int i = MAX_BELT - 1; i > 0; i--) {
            if (!shm_ptr->belt[i].is_active && shm_ptr->belt[i - 1].is_active) {
                shm_ptr->belt[i] = shm_ptr->belt[i - 1];
            }
        }
            /*
            shm_ptr->belt[i - 1].is_active = false;
            shm_ptr->belt[i - 1].price = 0;
            shm_ptr->belt[i - 1].type = 0;
            shm_ptr->belt[i - 1].target_id = -1;
            */
        shm_ptr->belt[0].is_active = false;
        shm_ptr->belt[0].price = 0;
        shm_ptr->belt[0].type = 0;
        shm_ptr->belt[0].target_id = -1;
        
       

        if (shm_ptr->plates_in_kitchen > 0) {

            // Próba zdjêcia 1 sztuki z licznika "KITCHEN_FULL" (bez blokowania)
            struct sembuf s_take = { SEM_KITCHEN_FULL, -1, IPC_NOWAIT };

            if (semop(semid, &s_take, 1) != -1) {
                // Jeœli siê uda³o (by³o danie), to k³adziemy na taœmê
                Plate taken_plate = shm_ptr->kitchen_prep[--shm_ptr->plates_in_kitchen];

                shm_ptr->belt[0] = taken_plate;
                shm_ptr->belt[0].is_active = true;

                // Aktualizujemy semafory taœmy
                struct sembuf s_op_empty = { SEM_EMPTY, -1, IPC_NOWAIT }; // Mniej pustego miejsca
                semop(semid, &s_op_empty, 1);

                struct sembuf s_op_full = { SEM_FULL, 1, IPC_NOWAIT };   // Wiêcej jedzenia
                semop(semid, &s_op_full, 1);

                std::cout << "[OBSLUGA] Klade danie na slot 0" << std::endl;
            }
        }
        sem_op(semid, SEM_MUTEX, 1);
    }
    std::cout << "\n[OBSLUGA] PODSUMOWANIE STRAT (na tasmie):" << std::endl;
    for (int i = 0; i < 4; i++) {
        int na_tasmie = shm_ptr->produced_count[i] - shm_ptr->sold_count[i];
        std::cout << "Typ " << i << " pozostalo: " << na_tasmie << " szt." << std::endl;
    }
    std::cout << std::flush;
    shm_ptr->obslugaDone = true;

    shmdt(shm_ptr);
    return 0;
}