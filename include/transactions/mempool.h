#ifndef MEMPOOL_H
#define MEMPOOL_H

#include <transaction.h>

typedef struct {
    Transaction tx;
    double fee;
    char status[16]; // pending, confirmed, suspiscious
} MempoolEntry;

#define MEMPOOL_SIZE 5000

void add_to_mempool(MempoolEntry entry);
void remove_confirmed_transactions();
void sort_mempool();
void print_mempool();

#endif