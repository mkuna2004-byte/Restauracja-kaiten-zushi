#include "common.h"

void handle_exit(int sig) {
    std::cout << "[KLIENT " << getpid() << "] wychodzi" << std::endl;
    // odepnij pamiêæ przed wyjœciem!!!!
    exit(0);
}

int main(int argc, char* argv[]) {

    signal(SIGQUIT, handle_exit);
    // klienci bêd¹ uruchamiani z argumentem - rozmiarem grupy
    int group_size = (argc > 1) ? atoi(argv[1]) : 1;

    key_t key = ftok("main_restaurant", 'R');
    int shmid = shmget(key, sizeof(SharedMemory), 0666);
    int semid = semget(key, 4, 0666);

    if (shmid == -1 || semid == -1) { perror("Klient shmget"); return 1; }

    SharedMemory* shm = (SharedMemory*)shmat(shmid, NULL, 0);
    srand(time(NULL) ^ getpid());

    //szukanie stolika
    int my_table = -1;
    sem_op(semid, SEM_MUTEX, -1);
    for (int i = 0; i < MAX_TABLES; i++) {
        // pusty lub taka sama grupa
        bool can_sit = false;

        if (group_size == 1) {
            if (shm->tables[i].is_counter && shm->tables[i].occupied_seats < shm->tables[i].capacity) {
                can_sit = true;
            }
        }
        else if (group_size > 1 && !shm->tables[i].is_counter) {
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
            break;
        }
    }
    sem_op(semid, SEM_MUTEX, 1);

    if (my_table == -1) {
        std::cout << "[KLIENT " << getpid() << "] Brak wolnych stolikow dla grupy " << group_size << std::endl;
        shmdt(shm);
        return 0;
    }

    std::cout << "[KLIENT " << getpid() << "] Usiedlismy ("<< group_size << ") przy stoliku nr " << my_table << std::endl;

    if (rand() % 100 < 30) { // 30% szans na zamowienie specjalne
        sem_op(semid, SEM_MUTEX, -1);
        for (int k = 0; k < 5; k++) {
            if (!shm->orders[k].is_active) {
                shm->orders[k].table_id = my_table;
                shm->orders[k].is_active = true;
                shm->orders[k].type = 3;
                std::cout << "[KLIENT " << getpid() << "] zamowienie specjalne!" << std::endl;
                break;
            }
        }
        sem_op(semid, SEM_MUTEX, 1);
    }

    // jedzenie
    int to_eat = (rand() % 8) + 3;
    for (int i = 0; i < to_eat; i++) {
        sem_op(semid, SEM_FULL, -1); // czekaj na talerzyk na tasmie
        sem_op(semid, SEM_MUTEX, -1);

        bool found = false;
        int vision_range = 3;
        for (int k = 0; k < vision_range; k++) {
            int j = (my_table + k) % MAX_BELT;
            if (shm->belt[k].is_active) {
                if (shm->belt[k].target_id == -1 || shm->belt[k].target_id == my_table) {
                    shm->sold_count[shm->belt[k].type]++;
                    shm->total_revenue += shm->belt[k].price;
                    shm->belt[k].is_active = false;
                    found = true;
                    std::cout << "[KLIENT " << getpid() << "] Zdejmuje sushi z pozycji " << k << " (Cena: " << shm->belt[k].price << "zl)" << std::endl;
                    break;
                }
            }
        }

        sem_op(semid, SEM_MUTEX, 1);
        if (found) {
            sem_op(semid, SEM_EMPTY, 1); // zwalnianie miejsca na tasmie
        }
        else {
            // jeœli nie znalezione, oddajemy full
            sem_op(semid, SEM_FULL, 1);
            i--;
            usleep(500000);
        }

        sleep(rand() % 2 + 1); // czas jedzenia
    }

    // zwalnianie stolika
    sem_op(semid, SEM_MUTEX, -1);
    shm->tables[my_table].occupied_seats -= group_size;
    if (shm->tables[my_table].occupied_seats == 0) {
        shm->tables[my_table].group_size = 0;
    }

    sem_op(semid, SEM_MUTEX, 1);
    //PLATNOSC!!
    std::cout << "[KLIENT " << getpid() << "] Stolik " << my_table << " wolny." << std::endl;

    shmdt(shm);
    return 0;
}