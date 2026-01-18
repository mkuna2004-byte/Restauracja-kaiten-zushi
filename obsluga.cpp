#include "common.h"

SharedMemory* shm_ptr = nullptr; // globalny wskaznik dla handlera sygna³ow

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

    // pobranie klucza i podpiêcie siê pod pamiêæ
    key_t key = ftok("main_restaurant", 'R');
    int shmid = shmget(key, sizeof(SharedMemory), 0666);
    int semid = semget(key, 4, 0666);

    if (shmid == -1 || semid == -1) { perror("Kierownik: b³¹d IPC"); exit(1); }

    shm_ptr = (SharedMemory*)shmat(shmid, NULL, 0);

    signal(SIGUSR1, handle_speed);
    signal(SIGUSR2, handle_speed);

    while (shm_ptr->isOpen) {

        sem_op(semid, SEM_KITCHEN_FULL, -1);
        // czekaj na wolne miejsce na tasmie
        sem_op(semid, SEM_EMPTY, -1);
        sem_op(semid, SEM_MUTEX, -1);

        for (int i = MAX_BELT - 1; i > 0; i--) {
            if (!shm_ptr->belt[i].is_active && shm_ptr->belt[i - 1].is_active) {
                shm_ptr->belt[i] = shm_ptr->belt[i - 1];
                shm_ptr->belt[i - 1].is_active = false;
            }
        }

        Plate taken_plate = shm_ptr->kitchen_prep[--shm_ptr->plates_in_kitchen];

        static int current_pos = 0;
        while (shm_ptr->belt[current_pos].is_active) {
            current_pos = (current_pos + 1) % MAX_BELT;
        }
        // znajdz pierwszy wolny slot na tasmie
        shm_ptr->belt[current_pos] = taken_plate;
        shm_ptr->belt[current_pos].is_active = true;
        std::cout << "[OBSLUGA] Klade danie na tasmie (slot " << current_pos << ")" << std::endl;
        
        current_pos = (current_pos + 1) % MAX_BELT;

        sem_op(semid, SEM_MUTEX, 1);
        sem_op(semid, SEM_FULL, 1); // powiadom klienta

        usleep(shm_ptr->chef_sleep_time); // czas wydawania dania
    }

    shmdt(shm_ptr);
    return 0;
}