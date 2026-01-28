#include "common.h"

int global_shmid = -1;
int global_semid = -1;
pid_t pid_kucharz = -1;
pid_t pid_obsluga = -1;
bool juz_sprzatam = false;
SharedMemory* shm = nullptr;

void cleanup(int sig) {
    
    if (juz_sprzatam) return;
    juz_sprzatam = true;
    std::cout << "\n[KIEROWNIK] Zamykanie restauracji (Sygnal " << sig << "). Czekam na raporty procesow." << std::endl;
    
    if (shm != nullptr) shm->isOpen = false;
    shm->obslugaDone = false;
    shm->kucharzDone = false;

    kill(0, SIGINT);

    if (pid_obsluga > 0) {
        kill(pid_obsluga, SIGINT);
        waitpid(pid_obsluga, NULL, 0);
        usleep(200000);
    }
    if (pid_kucharz > 0) {
        kill(pid_kucharz, SIGINT);
        waitpid(pid_kucharz, NULL, 0);
        usleep(200000);
    }
    
    usleep(700000);
    std::cout << std::flush;

    int cooked_sum = 0;
    int sold_sum = 0;
    if (shm != nullptr) {
        for (int i = 0; i < 4; i++) {
            cooked_sum += shm->produced_count[i];
            sold_sum += shm->sold_count[i];
        }
    }
    sleep(1);
    int status;
    pid_t p;
    while ((p = wait(&status)) > 0);

    std::cout << "\n[KASA] PODSUMOWANIE SPRZEDAZY:" << std::endl;
    for (int i = 0; i < 4; i++) {
        std::cout << "Typ " << i << " sprzedano: " << shm->sold_count[i] << " szt." << std::endl;
    }
    std::cout << "[KASA] Laczny utarg: " << shm->total_revenue << " PLN" << std::endl;
    sleep(1);

    //RAPORT
    std::ofstream file("raport_restauracji.txt", std::ios::app);
    if (shm != nullptr && file.is_open()) {
        file << "--- RAPORT Z DNIA " << time(NULL) << " ---\n";
        file << "Zysk: " << shm->total_revenue << " PLN\n";
        for (int i = 0; i < 4; i++) {
            file << "Typ " << i << "  Prod: " << shm->produced_count[i]
                << " Sprzed: " << shm->sold_count[i] << "\n";
        }
        file << "Dania pozostale na tasmie: " << (cooked_sum - sold_sum) << "\n";
        file << "------------------------------------------\n";
        file.close();
        std::cout << "[KIEROWNIK] Raport zapisany do raport_restauracji.txt" << std::endl;
    }
    kill(0, SIGQUIT);
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
    system("/bin/stty cooked echo");
    _exit(0);
}

void handle_pause(int sig) { // do CTRL^Z
    std::cout << "\n[SYSTEM] Restauracja wstrzymana (wszystkie procesy)." << std::endl;
    // STOP
    kill(0, SIGSTOP);
}

void handle_resume(int sig) { // fg
    std::cout << "[SYSTEM] Restauracja wznawia prace." << std::endl;
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

void sterowanie_klawiatura(pid_t pid_obsluga) {
    struct termios oldt, newt;
    tcgetattr(STDIN_FILENO, &oldt); // zapisz stare ustawienia
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO); // wy³¹cz buforowanie i echo

    while (true) {
        tcsetattr(STDIN_FILENO, TCSANOW, &newt); // w³acz RAW tylko na moment czytania
        char c = getchar();
        tcsetattr(STDIN_FILENO, TCSANOW, &oldt); // od razu przywróæ cooked

        if (c == '1') {
            kill(pid_obsluga, SIGUSR1);
        }
        else if (c == '2') {
            kill(pid_obsluga, SIGUSR2);
        }
        else if (c == '3') {
            cleanup(SIGINT);
        }
        usleep(10000);
    }
}

int main() {

    srand(time(NULL));
    signal(SIGINT, cleanup);    // Ctrl+C
    signal(SIGUSR1, przekaz_sygnal); // czeka na sygnaly
    signal(SIGUSR2, przekaz_sygnal);
    signal(SIGQUIT, SIG_IGN); // kierownik ignoruje SIGQUIT
    signal(SIGTSTP, handle_pause);
    signal(SIGCONT, handle_resume);

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
    shm->chef_sleep_time = 1000000; // 1 sekunda na start (do sygnalow)
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

    time_t start_time = time(NULL);
    int czas_otwarcia = 40;
    int counter = 0;

    std::cout << "Restauracja otwarta! [Kucharz PID: " << pid_kucharz
        << "] [Obsluga PID: " << pid_obsluga << "]" << std::endl;

    //watek do obslugi sygnalow z klawiatury
    std::thread input_thread(sterowanie_klawiatura, pid_obsluga);
    input_thread.detach(); // dziala niezaleznie

    while (difftime(time(NULL), start_time) < czas_otwarcia) {

        usleep(100000);
        counter++;

        if (counter % 10 != 0) {
            continue;
        }

        // petla klientow
        for (int i = 0; i < 40; i++) {
            if (!shm->isOpen) break;

            int is_vip = (rand() % 100 < 2) ? 1 : 0; // 2% szans na VIPa

            sem_op(global_semid, SEM_MUTEX, -1);
            int current = shm->active_clients;
            sem_op(global_semid, SEM_MUTEX, 1);

            if (is_vip == 0 && current >= 15) {
                sleep(1);
                continue;
            }

            if (is_vip) {
                std::cout << "\n[SYSTEM] >>> KLIENT VIP! Wchodzi bez kolejki! <<>> KLIENT VIP! Wchodzi bez kolejki! <<<\n" << std::endl;
            }

            while (waitpid(-1, NULL, WNOHANG) > 0) {} //usuwanie martwych procesow
            unsigned int seed = time(NULL) ^ (getpid() << i);
            int adults = (rand_r(&seed) % 2) + 1;
            int places_left = 4 - adults;
            int max_kids_allowed = (places_left < adults * 3) ? places_left : adults * 3;
            int kids = rand_r(&seed) % (max_kids_allowed + 2);
            int total_size = adults + kids;

            if (kids > adults * 3) {
                std::cout << "[KIEROWNIK] Grupa odrzucona: za duzo dzieci" << std::endl;
                sleep(1);
                continue;
            }

            pid_t client_pid = fork();
            if (client_pid == 0) {
                srand(time(NULL) ^ getpid());
                char size_str[10];
                char vip_arg[2];
                sprintf(size_str, "%d", total_size);
                sprintf(vip_arg, "%d", is_vip);
                execl("./klient", "./klient", size_str, vip_arg, NULL);
                perror("B³¹d execl");
                exit(1);
            }

            sleep(rand() % 3 + 1); // nowi klienci co 1-3 sekundy
        }
    }
    int log_frequency = 0;
    std::cout << "[KIEROWNIK] Koniec pracy restauracji, nowych klientow nie wpuszczamy" << std::endl;
    while (true) {
        sem_op(global_semid, SEM_MUTEX, -1);
        int occ = 0;
        std::string zajete_id = "";

        for (int i = 0; i < MAX_TABLES; i++) {
            if (shm->tables[i].occupied_seats > 0) {
                occ++;
                zajete_id += std::to_string(i) + " ";
            }
        }

        int pozostalo = shm->active_clients;
        sem_op(global_semid, SEM_MUTEX, 1);

        if (pozostalo <= 0) {
            break;
        }

        if (log_frequency % 12 == 0) {
            std::cout << "[KIEROWNIK] Czekam az " << pozostalo << " grup dokonczy posilek..." << std::endl;
        }
        log_frequency++;
        
        sleep(1);

        while (waitpid(-1, NULL, WNOHANG) > 0);
    }

    std::cout << "[KIEROWNIK] Wszyscy klienci obsluzeni. Zamykam kuchnie." << std::endl;
    shm->isOpen = false;

    cleanup(0);
    return 0;
}