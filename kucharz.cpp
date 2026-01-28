#include "common.h"

SharedMemory* shm = nullptr;
void handle_sigterm(int sig) {}

int main() {
    std::cout << "[KUCHARZ] Startuje. Przygotowuje dania dla obslugi." << std::endl;
 
    signal(SIGINT, [](int) {}); // wybudza z semop
    signal(SIGALRM, SIG_IGN);
    key_t key = ftok("main_restaurant", 'R');
    int shmid = shmget(key, sizeof(SharedMemory), 0666);
    int semid = semget(key, 4, 0666);
    shm = (SharedMemory*)shmat(shmid, NULL, 0);

    srand(time(NULL) ^ getpid());

    int prices[] = { 10, 15, 20, 40, 50, 60 };
    

    while (shm->isOpen || shm->active_clients > 0) {
        // kucharz przygotowuje danie (1-3 sekundy)
        usleep(shm->chef_sleep_time);
        if (sem_op(semid, SEM_MUTEX, -1) == -1) break;

        if (!shm->isOpen) {
            sem_op(semid, SEM_MUTEX, 1);
            break;
        }

        int active_orders = 0;
        for (int i = 0; i < 5; i++) if (shm->orders[i].is_active) active_orders++;
        if (active_orders > 0) std::cout << "[DEBUG KUCHARZ] Widze aktywnych zamowien: " << active_orders << std::endl;

        if (shm->plates_in_kitchen < 10) {
            Plate new_plate;
            new_plate.target_id = -1;
            new_plate.is_active = true;
            // zam specjalne
            int order_idx = -1;
            for (int i = 0; i < 5; i++) {
                if (shm->orders[i].is_active) {
                    order_idx = i;
                    break;
                }
            }

            if (order_idx != -1) {
                // Gotujemy pod konkretny stolik
                new_plate.type = 3;
                new_plate.price = prices[rand() % 3 + 3];
                new_plate.target_id = shm->orders[order_idx].table_id;
                shm->orders[order_idx].is_active = false; // Usun zamowienie z tabletu
                std::cout << "[KUCHARZ] Robie ZAMOWIENIE SPECJALNE dla stolika " << new_plate.target_id << std::endl;
            }
            else {
                // Gotujemy zwyk³e sushi
                int dynamic_limit = shm->active_clients + 2;
                if (shm->active_clients <= 3) dynamic_limit = 1;
                if (dynamic_limit > 10) dynamic_limit = 10;

                if (shm->plates_in_kitchen >= dynamic_limit) {
                    sem_op(semid, SEM_MUTEX, 1);
                    std::cout << "[KUCHARZ] Duzo jedzenia na blacie (" << shm->plates_in_kitchen << "). Czekam na zamowienia..." << std::endl;
                    sleep(1);
                    continue;
                }
                new_plate.type = rand() % 3;
                new_plate.price = prices[new_plate.type];
                new_plate.target_id = -1;
            }
            shm->produced_count[new_plate.type]++;
            new_plate.is_active = true;

            // na blat
            shm->kitchen_prep[shm->plates_in_kitchen++] = new_plate;

            sem_op(semid, SEM_MUTEX, 1);
            sem_op(semid, SEM_KITCHEN_FULL, 1); // Powiadom obs³ugê
        }
        else {
            sem_op(semid, SEM_MUTEX, 1);
            std::cout << "[KUCHARZ] Blat pelny, czekam..." << std::endl;
            sleep(1);
        }
    }

    int sum = 0;
    sleep(2);
    std::cout << "\n[KUCHARZ] PODSUMOWANIE PRODUKCJI:" << std::endl;
    for (int i = 0; i < 4; i++) {
        int kwota = shm->produced_count[i] * prices[i];
        sum += kwota;
        std::cout << "Rodzaj " << i << " - " << shm->produced_count[i]
            << " szt. - " << kwota << " zl" << std::endl;
    }
    std::cout << "[KUCHARZ] Laczna kwota wytworzonych dan: " << sum << " zl" << std::endl;
    shm->kucharzDone = true;
    
    shmdt(shm);
    return 0;
}