#ifndef BLOCK_H
#define BLOCK_H

#include <time.h>

#define HASH_SIZE 65

typedef struct
{
    int block_id;
    long timestamp;
    int transaction_count;

    char previous_hash[HASH_SIZE];
    char merkle_root[HASH_SIZE];

    long nonce;
    char miner_id[64];
    int difficulty;

    char block_size[HASH_SIZE];
} Block;

#endif
