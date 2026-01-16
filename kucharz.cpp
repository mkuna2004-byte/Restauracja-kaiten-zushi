#include "common.h"

int main() {
    std::cout << "[KUCHARZ] Startuje. Przygotowuje dania dla obslugi." << std::endl;

    key_t key = ftok("main_restaurant", 'R');
    int shmid = shmget(key, sizeof(SharedMemory), 0666);
    int semid = semget(key, 3, 0666);
    SharedMemory* shm = (SharedMemory*)shmat(shmid, NULL, 0);

    srand(time(NULL) ^ getpid());

    while (shm->isOpen) {
        // kucharz przygotowuje danie (1-3 sekundy)
        sleep(rand() % 3 + 1);

        sem_op(semid, SEM_MUTEX, -1);
        int type = rand() % 3; // 0, 1 lub 2 (ceny 10, 15, 20)
        shm->produced_count[type]++; // zwieksza statystyke produkcji
        std::cout << "[KUCHARZ] Gotowe danie typu " << type << ". Czeka na obsluge." << std::endl;
        sem_op(semid, SEM_MUTEX, 1);
    }

    shmdt(shm);
    return 0;
}