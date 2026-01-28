#include "common.h"

SharedMemory* shm = nullptr;
int my_table = -1;
int group_size = 0;
int semid = -1;

void handle_exit(int sig) {
    if (shm != nullptr && semid != -1) {

        struct sembuf s_lock = { SEM_MUTEX, -1, IPC_NOWAIT };
        if (semop(semid, &s_lock, 1) != -1) {

            if (my_table != -1) {
                shm->tables[my_table].occupied_seats -= group_size;
                if (shm->tables[my_table].occupied_seats < 0) shm->tables[my_table].occupied_seats = 0;

                if (shm->tables[my_table].occupied_seats == 0) {
                    shm->tables[my_table].group_size = 0;
                }
            }
            if (shm->active_clients > 0) {
                shm->active_clients--;
            }

            struct sembuf s_unlock = { SEM_MUTEX, 1, 0 };
            semop(semid, &s_unlock, 1);
        }
    }
    std::cout << "[KLIENT " << getpid() << "] wychodzi" << std::endl;
    if (shm != nullptr) shmdt(shm);
    exit(0);
}

void handle_evacuation(int sig) {
    _exit(0);
}

int main(int argc, char* argv[]) {

    signal(SIGTERM, handle_exit);
    signal(SIGQUIT, handle_exit);
    signal(SIGALRM, handle_evacuation);
    // klienci uruchamiani z argumentem rozmiarem grupy
    group_size = (argc > 1) ? atoi(argv[1]) : 1;
    bool is_vip = (argc > 2) ? atoi(argv[2]) : 0; // odczyt flagi VIP

    key_t key = ftok("main_restaurant", 'R');
    int shmid = shmget(key, sizeof(SharedMemory), 0666);
    semid = semget(key, 4, 0666);

    if (shmid == -1 || semid == -1) { perror("Klient shmget"); return 1; }

    shm = (SharedMemory*)shmat(shmid, NULL, 0);
    srand(time(NULL) ^ getpid());

    int cennik[] = { 10, 15, 20, 0 };
    
    // sekcja szukanie stolika
    std::cout << "[KLIENT " << getpid() << "] Czeka na przydzielenie stolika dla grupy " << group_size << "..." << std::endl;
    usleep(200000);
    sem_op(semid, SEM_MUTEX, -1);
    

    for (int i = 0; i < MAX_TABLES; i++) {
        
        bool can_sit = false;

        if (is_vip && shm->tables[i].is_counter) {
            continue;
        }

        if (group_size == 1 && !is_vip) {
            if (shm->tables[i].is_counter && shm->tables[i].occupied_seats < shm->tables[i].capacity) {
                can_sit = true;
            }
        }
        else if (!shm->tables[i].is_counter) {
            // czy pusty lub z taka sama grupa
            bool match_group = (shm->tables[i].occupied_seats == 0) || (shm->tables[i].group_size == group_size);
            bool has_space = (shm->tables[i].capacity - shm->tables[i].occupied_seats >= group_size);

            if (match_group && has_space) {
                can_sit = true;
            }
        }

        if (can_sit) {
            shm->tables[i].occupied_seats += group_size;
            shm->tables[i].group_size = group_size;
            my_table = i;
            shm->active_clients++;
            break;
        }
    }
    
    sem_op(semid, SEM_MUTEX, 1);
    if (my_table == -1) {
        std::cout << "[SYSTEM] " << "Brak wolnych stolikow dla grupy [" << getpid() << "] " << group_size << std::endl;
        
        shmdt(shm);
        return 0;
    }
    if (group_size == 1 && !is_vip) {
        std::cout << "[SYSTEM] Obsluga przydzielila stolik przy ladzie nr " << my_table << " dla grupy " << getpid() << std::endl;
        
    }
    else {
        if (is_vip) {
            std::cout << "[SYSTEM] Klient VIP " << getpid() << " otrzyma³ stolik nr " << my_table << std::endl;
        }
        else {
            std::cout << "[SYSTEM] Obsluga przydzielila stolik nr " << my_table << " dla grupy " << getpid() << std::endl;
        }
    }
    sem_op(semid, SEM_MUTEX, 1);

    bool ordered_special = false;
    sem_op(semid, SEM_MUTEX, -1);
    bool is_shared = (shm->tables[my_table].occupied_seats > group_size);
    sem_op(semid, SEM_MUTEX, 1);

    if (!is_shared && rand() % 100 < 30) { // 30% szans na zamowienie specjalne
        sem_op(semid, SEM_MUTEX, -1);
        for (int k = 0; k < 5; k++) {
            if (!shm->orders[k].is_active) {
                shm->orders[k].table_id = my_table;
                shm->orders[k].is_active = true;
                shm->orders[k].type = 3;
                ordered_special = true;

                std::cout << "[KLIENT " << getpid() << "] zamowienie specjalne!" << std::endl;
                break;
            }
        }
        sem_op(semid, SEM_MUTEX, 1);
    }

    // sekcja jedzenie
    int eaten[4] = { 0, 0, 0, 0 };
    int eaten_special = 0;
    int to_eat = (rand() % 8) + 3;
    int eaten_count = 0;

    while (eaten_count < to_eat || ordered_special) {
        if (!shm->isOpen) break;

        sem_op(semid, SEM_MUTEX, -1);

        bool found = false;
        int my_slot = my_table % MAX_BELT;
        
        for (int k = 0; k < 3; k++) {
            int idx = (my_slot + k) % MAX_BELT;
            if (shm->belt[idx].is_active && shm->belt[idx].price > 0) {
               
                int typ = shm->belt[idx].type;

                if (shm->belt[idx].type == 3 && shm->belt[idx].target_id != my_table) {
                    continue;
                }

                if (typ == 3 && !ordered_special) {
                    continue;
                }

                if (ordered_special && typ != 3) {
                    continue;
                }
                
                eaten[typ]++;
                shm->sold_count[typ]++;
                if (typ == 3) {
                    eaten_special += shm->belt[idx].price;
                    ordered_special = false;
                }

                std::cout << "[KLIENT " << getpid() << "] Zdejmuje sushi z pozycji " << idx << " (Cena: " << shm->belt[idx].price << "zl)" << std::endl;
                    
                shm->belt[idx].is_active = false;
                shm->belt[idx].type = 0;      // reset typu na slocie
                shm->belt[idx].target_id = -1;
                shm->belt[idx].price = 0;

                struct sembuf s_empty = { SEM_EMPTY, 1, IPC_NOWAIT };
                semop(semid, &s_empty, 1);

                // jeœli nie znalezione, oddajemy full
                struct sembuf s_eat = { SEM_FULL, -1, IPC_NOWAIT };
                semop(semid, &s_eat, 1);

                found = true;
                eaten_count++;
                break;
            }
        }
        sem_op(semid, SEM_MUTEX, 1);

        if (found) {
            sleep(rand() % 2 + 3); // czas jedzenia
        }
        else {
            usleep(200000);
        }
    }

    // sekcja placenie i zwalnianie stolika
    std::cout << "[KLIENT " << getpid() << "] Prosimy o rachunek" << std::endl;
    sleep(1); // czas oczekiwania na kelnera
    sem_op(semid, SEM_MUTEX, -1);

    //mnozenie cen za dania i zliczanie rachunku
    int client_sum = 0;
    for (int t = 0; t < 3; t++) {
        if (eaten[t] > 0) {
            int type_cost = eaten[t] * cennik[t];
            client_sum += type_cost;
            std::cout << "Typ " << t << ": " << eaten[t] << " szt. * " << cennik[t] << " zl = " << type_cost << " zl" << std::endl;
        }
    }
    if (eaten[3] > 0) {
        client_sum += eaten_special;
        std::cout << "Typ 3 (specjalne): " << eaten[3] << " szt. (Suma cen: " << eaten_special << " zl)" << std::endl;
    }

    std::cout << "[KASA] Grupa " << getpid() << " placi lacznie: " << client_sum << " zl." << std::endl;
    shm->total_revenue += client_sum;

    shm->tables[my_table].occupied_seats -= group_size;
    if (shm->tables[my_table].occupied_seats == 0) {
        shm->tables[my_table].group_size = 0;
        std::cout << "[OBSLUGA] Stolik " << my_table << " jest teraz wolny." << std::endl;
    }
    shm->active_clients--;
    sem_op(semid, SEM_MUTEX, 1);

    shmdt(shm);
    return 0;
}