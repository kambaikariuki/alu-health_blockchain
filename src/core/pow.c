#include "../../include/core/block.h"
#include "../../include/core/chainstate.h"
#include "../../include/core/mempool.h"
#include "../../include/core/merkle.h"
#include "../../include/core/pow.h"
#include "../../include/security/sha256.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MINE_BLOCK_OK             0
#define MINE_BLOCK_NO_PENDING_TX  1
#define MINE_BLOCK_OUT_OF_MEMORY  2

/* Fixed cap on transactions pulled from the mempool per block, per your
 * earlier "fixed cap" design choice for mempool selection. */
#define MAX_TX_PER_BLOCK 100

/* True if hash starts with `difficulty` leading '0' hex characters. */
int meets_difficulty(const char *hash, int difficulty)
{
    for (int i = 0; i < difficulty; i++) {
        if (hash[i] != '0') {
            return 0;
        }
    }
    return 1;
}

static void compute_pow_hash(const Block *block, char output[SHA256_HEX_SIZE])
{
    char buffer[SHA256_HEX_SIZE * 2 + 256];

    snprintf(buffer, sizeof(buffer), "%d|%ld|%s|%s|%ld|%d|%s",
              block->block_id,
              block->timestamp,
              block->previous_hash,
              block->merkle_root,
              block->nonce,
              block->difficulty,
              block->miner_id);

    sha256(buffer, output);
}

static int select_block_transactions(Block *block)
{
    mempool_sort_by_fee();

    Transaction *selected = malloc(sizeof(Transaction) * MAX_TX_PER_BLOCK);
    if (selected == NULL) {
        block->transactions = NULL;
        block->transaction_count = 0;
        return 0;
    }

    int count = 0;
    for (int i = 0; i < mempool.count && count < MAX_TX_PER_BLOCK; i++) {
        MempoolEntry *entry = &mempool.entries[i];
        if (entry->status == TX_SUSPICIOUS) {
            continue;
        }

        Transaction *tx = &selected[count];
        strncpy(tx->transaction_id, entry->transaction_id, TX_ID_LEN - 1);
        tx->transaction_id[TX_ID_LEN - 1] = '\0';
        strncpy(tx->sender_address, entry->sender_address, sizeof(tx->sender_address) - 1);
        tx->sender_address[sizeof(tx->sender_address) - 1] = '\0';
        strncpy(tx->receiver_address, entry->receiver_address, sizeof(tx->receiver_address) - 1);
        tx->receiver_address[sizeof(tx->receiver_address) - 1] = '\0';
        tx->amount = entry->amount;
        strncpy(tx->transaction_type, entry->transaction_type, sizeof(tx->transaction_type) - 1);
        tx->transaction_type[sizeof(tx->transaction_type) - 1] = '\0';
        tx->timestamp = time(NULL);
        tx->sender_nonce = 0;
        tx->digital_signature[0] = '\0';

        count++;
    }

    if (count == 0) {
        free(selected);
        block->transactions = NULL;
        block->transaction_count = 0;
        return 0;
    }

    block->transactions = selected;
    block->transaction_count = count;
    return count;
}
int mine_block(Block *block)
{
    int tx_count = select_block_transactions(block);
    if (tx_count == 0) {
        fprintf(stderr, "mine_block: no PENDING transactions available to mine\n");
        return MINE_BLOCK_NO_PENDING_TX;
    }

    compute_merkle_root(block->transactions, block->transaction_count, block->merkle_root);

    block->timestamp = time(NULL);
    block->difficulty = chain_state.difficulty;
    block->nonce = 0;

    char hash[SHA256_HEX_SIZE];
    compute_pow_hash(block, hash);

    while (!meets_difficulty(hash, block->difficulty)) {
        block->nonce++;
        compute_pow_hash(block, hash);
    }

    return MINE_BLOCK_OK;
}


int validate_pow(Block block)
{
    char hash[SHA256_HEX_SIZE];
    compute_pow_hash(&block, hash);
    return meets_difficulty(hash, block.difficulty);
}