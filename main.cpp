#include <signal.h>
#include <fstream>
#include "common.h"

int global_shmid = -1;
int global_semid = -1;
pid_t pid_kucharz = -1;
pid_t pid_obsluga = -1;
SharedMemory* shm = nullptr;

void cleanup(int sig) {
    std::cout << "\n[KIEROWNIK] Zamykanie restauracji (Sygnal " << sig << ")." << std::endl;
    kill(0, SIGQUIT);
    sleep(2);
    int cooked_sum = 0;
    int sold_sum = 0;

    if (shm != nullptr) {
        for (int i = 0; i < 4; i++) {
            cooked_sum += shm->produced_count[i];
            sold_sum += shm->sold_count[i];
        }
    }

    //RAPORT
    std::ofstream file("raport_restauracji.txt", std::ios::app);
    if (shm != nullptr && file.is_open()) {
        file << "--- RAPORT Z DNIA " << time(NULL) << " ---\n";
        file << "Zysk: " << shm->total_revenue << " PLN\n";
        //file << "Dania 10zl: Prod: " << shm->produced_count[0] << " Sprzed: " << shm->sold_count[0] << "\n";
        //file << "Dania 15zl: Prod: " << shm->produced_count[1] << " Sprzed: " << shm->sold_count[1] << "\n";
        //file << "Dania 20zl: Prod: " << shm->produced_count[2] << " Sprzed: " << shm->sold_count[2] << "\n";
        for (int i = 0; i < 4; i++) {
            file << "Typ " << i << "  Prod: " << shm->produced_count[i]
                << " Sprzed: " << shm->sold_count[i] << "\n";
        }
        file << "Dania pozostale na tasmie: " << (cooked_sum - sold_sum) << "\n";
        file << "------------------------------------------\n";
        file.close();
        std::cout << "[KIEROWNIK] Raport zapisany do raport_restauracji.txt" << std::endl;
    }

    if (shm != nullptr) shmdt(shm);
    // usuwamy pamiêæ, tylko jeœli zosta³a utworzona
    if (global_shmid != -1) {
        shmctl(global_shmid, IPC_RMID, NULL);
        std::cout << "[KIEROWNIK] Pamiec wspoldzielona usunieta." << std::endl;
    }

    // usuwamy semafory
    if (global_semid != -1) {
        semctl(global_semid, 0, IPC_RMID);
        std::cout << "[KIEROWNIK] Semafory usuniete." << std::endl;
    }
    exit(0);
}

void przekaz_sygnal(int sig) {
    if (sig == SIGUSR1) {
        std::cout << "[KIEROWNIK] Polecenie: PRZYSPIESZYC (SIGUSR1)" << std::endl;
        if (pid_obsluga > 0) kill(pid_obsluga, SIGUSR1);
    }
    else if (sig == SIGUSR2) {
        std::cout << "[KIEROWNIK] Polecenie: ZWOLNIC (SIGUSR2)" << std::endl;
        if (pid_obsluga > 0) kill(pid_obsluga, SIGUSR2);
    }
}

int main() {


    srand(time(NULL));
    signal(SIGINT, cleanup);    // Ctrl+C konczy pracê
    signal(SIGUSR1, przekaz_sygnal); // kierownik nas³uchuje sygna³ów steruj¹cych
    signal(SIGUSR2, przekaz_sygnal);
    signal(SIGQUIT, SIG_IGN); // kierownik ignoruje SIGQUIT

    // klucze IPC
    key_t key = ftok("main_restaurant", 'R');
    if (key == -1) { perror("main ftok"); exit(1); }

    global_shmid = shmget(key, sizeof(SharedMemory), IPC_CREAT | 0666);
    global_semid = semget(key, 4, IPC_CREAT | 0666);

    if (global_shmid == -1 || global_semid == -1) {
        perror("Blad tworzenia zasobow");
        exit(1);
    }
    // pamiec wspoldzielona
    shm = (SharedMemory*)shmat(global_shmid, NULL, 0);
    if (shm == (void*)-1) { perror("shmat"); exit(1); }

    memset(shm, 0, sizeof(SharedMemory));

    // inicjalizacja danych w pamieci
    shm->isOpen = true;
    // tworzenie stolikow
    for (int i = 0; i < MAX_TABLES; i++) {
        shm->tables[i].id = i;
        shm->tables[i].occupied_seats = 0;
        shm->tables[i].group_size = 0;

        if (i < 4) {
            shm->tables[i].capacity = 1;
            shm->tables[i].is_counter = true;
        }
        else if (i < 7) {
            shm->tables[i].capacity = 2;
            shm->tables[i].is_counter = false;
        }
        else if (i < 10) {
            shm->tables[i].capacity = 3;
            shm->tables[i].is_counter = false;
        }
        else {
            shm->tables[i].capacity = 4;
            shm->tables[i].is_counter = false;
        }
    }
    shm->active_clients = 0;
    shm->total_revenue = 0;
    shm->chef_sleep_time = 1000000; // 1 sekunda na start
    for (int i = 0; i < 4; i++) {
        shm->produced_count[i] = 0;
        shm->sold_count[i] = 0;
    }

    // inicjalizacja semaforow
    semctl(global_semid, SEM_MUTEX, SETVAL, 1);     // mutex = 1 (wolne)
    semctl(global_semid, SEM_EMPTY, SETVAL, MAX_BELT); // P wolnych miejsc
    semctl(global_semid, SEM_FULL, SETVAL, 0);     // 0 talerzyków na start
    semctl(global_semid, SEM_KITCHEN_FULL, SETVAL, 0);

    // kucharz
    pid_kucharz = fork();
    if (pid_kucharz == 0) {
        execl("./kucharz", "./kucharz", NULL);
        perror("Blad execl kucharz"); exit(1);
    }

    // obsluga
    pid_obsluga = fork();
    if (pid_obsluga == 0) {
        execl("./obsluga", "./obsluga", NULL);
        perror("Blad execl obsluga"); exit(1);
    }

    std::cout << "Restauracja otwarta! [Kucharz PID: " << pid_kucharz
        << "] [Obsluga PID: " << pid_obsluga << "]" << std::endl;

    // petla klientow
    for (int i = 0; i < 30; i++) {
        if (!shm->isOpen) break;
        
        unsigned int seed = time(NULL) ^ (getpid() << i);
        int adults = (rand_r(&seed) % 2) + 1; // 1-2 doros³ych
        int kids = rand_r(&seed) % 5;        // 0-4 dzieci

        if (kids > adults * 3) {
            std::cout << "[KIEROWNIK] Grupa odrzucona: za duzo dzieci" << std::endl;
            sleep(1);
            continue;
        }
        int total_size = adults + kids;
        pid_t client_pid = fork();
        if (client_pid == 0) {
            srand(time(NULL) ^ getpid());
            char size_str[4];
            sprintf(size_str, "%d", total_size);
            execl("./klient", "./klient", size_str, NULL);
            perror("B³¹d execl");
            exit(0);
        }

        sleep(rand() % 3 + 1); // nowi klienci co 1-3 sekundy
    }
    
    sleep(20);
    shm->isOpen = false;
    
    kill(0, SIGTERM);
    
    while (true) {
        sem_op(global_semid, SEM_MUTEX, -1);
        int count = shm->active_clients;
        sem_op(global_semid, SEM_MUTEX, 1);

        if (count == 0) break; // Wszyscy wyszli!
        std::cout << "[KIEROWNIK] Czekam na ostatnich " << count << " klientów..." << std::endl;
        sleep(2);
    }
    //kill(pid_kucharz, SIGTERM);
    //kill(pid_obsluga, SIGTERM);
    while (wait(NULL) > 0);
    cleanup(0);

    // testy
    /*
    while (true) {
        sleep(8);
        if (shm != nullptr) {
            std::cout << "[KIEROWNIK] Stan kasy: " << shm->total_revenue << " PLN" << std::endl;
        }
    }
    */
    return 0;
}