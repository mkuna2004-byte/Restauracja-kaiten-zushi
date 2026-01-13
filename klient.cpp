#include "common.h"

int main(int argc, char* argv[]) {
    // klienci bêd¹ uruchamiani z argumentem - rozmiarem grupy
    int group_size = (argc > 1) ? atoi(argv[1]) : 2;

    std::cout << "[KLIENT] Grupa " << getpid() << " (osób: " << group_size << ") podchodzi do wejœcia..." << std::endl;

    key_t key = ftok("main_restaurant", 'R');
    //key_t key = 123456;
    int shmid = shmget(key, sizeof(SharedMemory), 0666);
    if (shmid == -1) { perror("Klient shmget"); return 1; }

    SharedMemory* shm = (SharedMemory*)shmat(shmid, NULL, 0);

    // Tu bedzie shm->tables (czy jest wolne miejsce)
    std::cout << "[KLIENT] ..." << std::endl;

    shmdt(shm);
    return 0;
}