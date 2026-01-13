#include "common.h"

int main() {
    std::cout << "[KUCHARZ] Jestem! PID: " << getpid() << std::endl;

    // pobranie klucza i podpiêcie siê pod pamiêæ
    key_t key = ftok("main_restaurant", 'R');
    //key_t key = 123456;
    if (key == -1) { perror("szef ftok"); exit(1); }
    int shmid = shmget(key, sizeof(SharedMemory), 0600);

    if (shmid == -1) {
        perror("Kucharz: b³¹d shmget");
        return 1;
    }

    SharedMemory* shm = (SharedMemory*)shmat(shmid, NULL, 0);
    std::cout << "[KUCHARZ] Widzê restauracjê. Status otwarcia: " << (shm->isOpen ? "TAK" : "NIE") << std::endl;

    shmdt(shm);
    return 0;
}