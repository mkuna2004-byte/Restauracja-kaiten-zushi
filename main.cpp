#include "common.h"

int main() {
    // klucze IPC
    key_t key = ftok("main_restaurant", 'R');
    if (key == -1) { perror("main ftok"); exit(1); }

    // pamiec wspoldzielona
    int shmid = shmget(key, sizeof(SharedMemory), IPC_CREAT | 0600);
    if (shmid == -1) { perror("shmget"); exit(1); }

    SharedMemory* shm = (SharedMemory*)shmat(shmid, NULL, 0);
    if (shm == (void*)-1) { perror("shmat"); exit(1); }

    // semafory
    int semid = semget(key, 3, IPC_CREAT | 0600);
    if (semid == -1) { perror("semget"); exit(1); }

    // inicjalizacja semaforow
    semctl(semid, SEM_MUTEX, SETVAL, 1);     // mutex = 1 (wolne)
    semctl(semid, SEM_EMPTY, SETVAL, MAX_BELT); // P wolnych miejsc
    semctl(semid, SEM_FULL, SETVAL, 0);     // 0 talerzyków na start

    // inicjalizacja danych w pamieci
    shm->isOpen = true;
    shm->total_revenue = 0;
    for (int i = 0; i < MAX_BELT; i++) shm->belt[i].is_active = false;

    std::cout << "--- RESTAURACJA OTWARTA ---" << std::endl;
    std::cout << "Pamiêæ SHM ID: " << shmid << ", Semafory ID: " << semid << std::endl;

    std::cout << "Kierownik: Uruchamiam kucharza..." << std::endl;
    system("ls -l");
    pid_t pid = fork(); // kopia procesu

    if (pid == -1) {
        perror("fork failed");
        exit(1);
    }

    if (pid == 0) {
        // To PROCES POTOMNY
        // execl zamienia obecny proces na program "./chef"
        execl("./szef", "./szef", NULL);

        perror("execl failed");
        exit(1);
    }
    else {
        // To wykonuje PROCES RODZIC (Kierownik)
        std::cout << "Kierownik: Kucharz wystartowa³ z PID: " << pid << std::endl;

        sleep(2);
    }
    // fork() exec() ....

    // testowo
    shmdt(shm);

    // usuwanie shmctl/semctl
    return 0;
}