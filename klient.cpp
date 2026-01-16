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
    int semid = semget(key, 3, 0666);

    if (shmid == -1 || semid == -1) { perror("Klient shmget"); return 1; }

    SharedMemory* shm = (SharedMemory*)shmat(shmid, NULL, 0);
    srand(time(NULL) ^ getpid());

    //szukanie stolika
    int my_table = -1;
    sem_op(semid, SEM_MUTEX, -1);
    for (int i = 0; i < MAX_TABLES; i++) {
        // pusty lub taka sama grupa
        bool can_sit = (shm->tables[i].occupied_seats == 0) ||
            (shm->tables[i].group_size == group_size);

        if (can_sit && (shm->tables[i].capacity - shm->tables[i].occupied_seats >= group_size)) {
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

    // jedzenie
    int to_eat = (rand() % 8) + 3;
    for (int i = 0; i < to_eat; i++) {
        sem_op(semid, SEM_FULL, -1); // czekaj na talerzyk na tasmie
        sem_op(semid, SEM_MUTEX, -1);

        bool found = false;
        for (int j = 0; j < MAX_BELT; j++) {
            if (shm->belt[j].is_active) {
                std::cout << "[KLIENT " << getpid() << "] Zdejmuje sushi z pozycji " << j << " (Cena: " << shm->belt[j].price << "zl)" << std::endl;
                shm->sold_count[shm->belt[j].type]++;
                shm->total_revenue += shm->belt[j].price;
                
                //std::cout << "[KLIENT " << getpid() << "] Zjadam danie za " << shm->belt[j].price << " PLN." << std::endl;

                shm->belt[j].is_active = false;
                found = true;
                break;
            }
        }

        sem_op(semid, SEM_MUTEX, 1);
        if (found) {
            sem_op(semid, SEM_EMPTY, 1); // zwalnianie miejsca na tasmie
        }
        else {
            // jeœli nie znalezione, oddajemy full
            sem_op(semid, SEM_FULL, 1);
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