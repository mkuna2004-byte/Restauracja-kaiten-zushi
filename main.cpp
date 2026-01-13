#include <signal.h>
#include "common.h"

int global_shmid = -1;
int global_semid = -1;

void cleanup(int sig) {
    std::cout << "\n[KIEROWNIK] Otrzymano sygna³ " << sig << ". Sprz¹tanie..." << std::endl;

    // usuwamy pamiêæ, tylko jeœli zosta³a utworzona
    if (global_shmid != -1) {
        shmctl(global_shmid, IPC_RMID, NULL);
        std::cout << "[KIEROWNIK] Pamiêæ wspó³dzielona usuniêta." << std::endl;
    }

    // usuwamy semafory
    if (global_semid != -1) {
        semctl(global_semid, 0, IPC_RMID);
        std::cout << "[KIEROWNIK] Semafory usuniête." << std::endl;
    }

    exit(0);
}

int main() {

    signal(SIGINT, cleanup);
    // klucze IPC
    key_t key = ftok("main_restaurant", 'R');
    //key_t key = 123456;
    if (key == -1) { perror("main ftok"); exit(1); }

    global_shmid = shmget(key, sizeof(SharedMemory), IPC_CREAT | 0666);
    global_semid = semget(key, 3, IPC_CREAT | 0666);

    if (global_shmid == -1 || global_semid == -1) {
        perror("B³¹d tworzenia zasobów");
        exit(1);
    }
    // pamiec wspoldzielona
    SharedMemory* shm = (SharedMemory*)shmat(global_shmid, NULL, 0);
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
    // inicjalizacja stolikow
    for (int i = 0; i < MAX_TABLES; i++) {
        shm->tables[i].id = i;
        shm->tables[i].capacity = 4; // stoliki maja 4 miejsca
        shm->tables[i].occupied_seats = 0;
    }

    shm->total_revenue = 0;
    for (int i = 0; i < MAX_BELT; i++) shm->belt[i].is_active = false;

    std::cout << "--- RESTAURACJA OTWARTA ---" << std::endl;
    std::cout << "Pamiêæ SHM ID: " << global_shmid << ", Semafory ID: " << semid << std::endl;
    std::cout << "Restauracja dzia³a. Naciœnij Ctrl+C, aby zamkn¹æ." << std::endl;

    std::cout << "Kierownik: Uruchamiam kucharza..." << std::endl;
    system("ls -l");
    pid_t pid = fork(); // kopia procesu

    if (pid == -1) {
        perror("fork failed");
        exit(1);
    }

    if (pid == 0) {
        // To PROCES POTOMNY
        // execl zamienia obecny proces na program ./chef
        execl("./szef", "./szef", NULL);

        perror("execl failed");
        exit(1);
    }
    else {
        // To wykonuje PROCES RODZIC (Kierownik)
        std::cout << "Kierownik: Kucharz wystartowa³ z PID: " << pid << std::endl;

        sleep(2);

        std::cout << "Kierownik: Otwieram drzwi dla klientów..." << std::endl;
        //test klientow!!
        for (int i = 0; i < 5; i++) {
            pid_t client_pid = fork();
            if (client_pid == 0) {
                // Proces klienta z rozmiarem grupy 2
                execl("./klient", "./klient", "2", NULL);
                perror("execl klient failed");
                exit(1);
            }
            sleep(1); // Kolejna grupa przychodzi po sekundzie
        }

        // test...:
        std::cout << "Kierownik: Restauracja w pe³ni dzia³aj¹ca. Czekam..." << std::endl;
        sleep(6);
    }
    // fork() exec() ....

    // testowo
    shmdt(shm);

    // usuwanie shmctl/semctl
    return 0;
}