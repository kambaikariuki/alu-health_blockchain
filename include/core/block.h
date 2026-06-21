#ifndef BLOCK_H
#define BLOCK_H

#include <time.h>
#include "../transactions/transaction.h"

#define HASH_SIZE 65
#define MAX_TRANSACTIONS_PER_BLOCK 100

typedef struct
{
    int block_id;
    long timestamp;
    int transaction_count;
    Transaction *transactions;

    char previous_hash[HASH_SIZE];
    char merkle_root[HASH_SIZE];

    long nonce;
    char miner_id[64];
    int difficulty;

} Block;

#endif
