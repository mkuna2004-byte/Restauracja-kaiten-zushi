#pragma once

#ifndef COMMON_H
#define COMMON_H

#include <iostream>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <cstdio>
#include <cstdlib>

#define MAX_BELT 15   // pojemnosc tasmy
#define MAX_TABLES 10 // max stolikow

struct Plate {
    int price;
    int target_id;
    bool is_active;
};

struct Table {
    int id;
    int capacity;
    int occupied_seats;
    int group_size;
    pid_t group_pid;
};

struct SharedMemory {
    Plate belt[MAX_BELT];
    Table tables[MAX_TABLES];
    bool isOpen;
    int total_revenue;
};

// Numery semaforow w zestawie
#define SEM_MUTEX 0 // Pilnuje dostêpu do pamiêci (0 lub 1)
#define SEM_EMPTY 1 // Liczy wolne miejsca na taœmie (startuje od P)
#define SEM_FULL  2 // Liczy talerzyki na tasmie (startuje od 0)

// Fun pomocnicza do operacji na semaforach
inline void sem_op(int semid, int sem_num, int op) {
    struct sembuf sops;
    sops.sem_num = sem_num;
    sops.sem_op = op;
    sops.sem_flg = 0;
    if (semop(semid, &sops, 1) == -1) {
        perror("B³¹d semop");
        exit(1);
    }
}

#endif