#pragma once

#ifndef COMMON_H
#define COMMON_H

#include <iostream>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <ctime>
#include <cstdio>
#include <thread>
#include <cstdlib>
#include <string>
#include <cstring>
#include <fstream>
#include <fcntl.h>
#include <termios.h>

#define MAX_BELT 15   // pojemnosc tasmy
#define MAX_TABLES 14
// Numery semaforow w zestawie
#define SEM_MUTEX 0 // Pilnuje dostepu do pamieci (0 lub 1)
#define SEM_EMPTY 1 // Liczy wolne miejsca na tasmie (startuje od P)
#define SEM_FULL  2 // Liczy talerzyki na tasmie (startuje od 0)
#define SEM_KITCHEN_FULL 3 // Komunikacja kuchnia - obsluga

struct Plate {
    int price;
    int target_id;
    bool is_active;
    int type; // 0: 10z³, 1: 15z³, 2: 20z³, 3: specjalne
};

struct Table {
    int id;
    int capacity;
    int occupied_seats;
    int group_size;
    bool is_counter; // true = lada, false = stolik
};

struct SpecialOrder {
    int table_id;
    int type; // 3 Ramen(40), 4 Tempura(50), 5 Sashimi(60)
    bool is_ready;
    bool is_active;
};

struct SharedMemory {
    Plate belt[MAX_BELT];
    Plate kitchen_prep[10];
    int plates_in_kitchen;
    Table tables[MAX_TABLES];
    bool isOpen;
    int total_revenue;
    int active_clients;
    // statystyki do raportu koncowego
    int produced_count[4]; // kucharz - indeksy dla cen 10, 15, 20, specjalne
    int sold_count[4];     // statystyki kasy
    SpecialOrder orders[5];
    int chef_sleep_time;   // sterowanie sygna³ami
    bool kucharzDone;
    bool obslugaDone;
};

inline int sem_op(int semid, int sem_num, int op, SharedMemory* shm_ptr = nullptr) {
    struct sembuf sops;
    sops.sem_num = sem_num;
    sops.sem_op = op;
    sops.sem_flg = SEM_UNDO;


    if (semop(semid, &sops, 1) == -1) {
        if (errno == EINTR) {
            return -1;
        }
        if (errno == EINVAL || errno == EIDRM) {
            return -1;
        }
        perror("B³¹d semop");
        return -1;
    }
    return 0;
}
#endif