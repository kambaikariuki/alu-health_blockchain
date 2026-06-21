#ifndef MEMPOOL_H
#define MEMPOOL_H

#include "transaction.h"

#define MEMPOOL_INITIAL_CAPACITY 16

typedef enum {
    TX_PENDING = 0,    /* waiting to be mined */
    TX_CONFIRMED,      /* included in a block */
    TX_SUSPICIOUS,      /* flagged by fraud detection, withheld until reviewed */
} TxStatus;

typedef struct {
    char transaction_id[TX_ID_LEN];
    char sender_address[64];
    char receiver_address[64];
    double amount;
    char transaction_type[32];
    double fee;
    TxStatus status;
    long submitted_at;
    int flag_reason;
} MempoolEntry;

typedef struct {
    MempoolEntry *entries;
    int count;               
    int capacity;            
} Mempool;

/* Global mempool, defined once in mempool.c */
extern Mempool mempool;

void init_mempool();
int mempool_add(MempoolEntry entry);
void mempool_remove(const char *transaction_id);
void mempool_sort_by_fee();
MempoolEntry* mempool_get_next_for_mining();
void mempool_mark_status(const char *transaction_id, TxStatus status);
void free_mempool();
void view_mempool();

#endif