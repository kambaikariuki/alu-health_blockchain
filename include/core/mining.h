#ifndef MINING_H
#define MINING_H

#define RETARGET_INTERVAL 10
#define RETARGET_TARGET_LOW_SEC  30.0
#define RETARGET_TARGET_HIGH_SEC 90.0
#define MIN_DIFFICULTY 1

#define MINE_OK                  0
#define MINE_NO_PENDING_TX       1
#define MINE_ADD_BLOCK_FAILED    2
#define MINE_NO_POOL_MINERS      3
#define MINE_INVALID_HASH_SHARE  4

typedef struct {
    char miner_id[64];
    long hashes_attempted;
} PoolMiner;

int mine_block_solo(const char *miner_id);
int mine_block_pool(const PoolMiner *miners, int miner_count);
void check_difficulty_retarget();

#endif