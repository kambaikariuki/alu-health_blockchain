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

/* ASSUMPTION: block hash = sha256 of a pipe-delimited string of header
 * fields (block_id, timestamp, previous_hash, merkle_root, nonce,
 * difficulty, miner_id). This mirrors the same convention used in
 * blockchain.c's compute_block_hash - both need to agree on this format or
 * validate_chain() and validate_pow() would disagree about what a block's
 * "real" hash is. If there's a different specified format, this is the one
 * function to change. miner_id is included so two miners racing on
 * identical mempool contents don't produce identical headers. */
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

/* Pulls up to MAX_TX_PER_BLOCK highest-fee PENDING entries from the
 * mempool, converts each to a Transaction, and writes them into a
 * newly-malloc'd array assigned to block->transactions (caller/owner of
 * the Block now owns this allocation - mirrors add_block's existing
 * deep-copy-ownership pattern elsewhere in this codebase).
 *
 * Does NOT remove anything from the mempool or change entry status - that
 * is step 7 of the spec ("remove confirmed transactions"), which only
 * happens after the block is successfully mined AND added to the chain,
 * which is outside mine_block's scope (pow.h says nothing about mempool
 * cleanup or chain insertion - that belongs to whatever orchestration
 * layer calls mine_block, e.g. a future mining.c).
 *
 * Returns the number of transactions selected (0 if none PENDING).
 * Sets block->transactions to NULL and block->transaction_count to 0 if
 * none were available - caller must check before treating the block as
 * mineable (see mine_block below).
 *
 * ASSUMPTION: MempoolEntry.sender/receiver map to
 * Transaction.sender_address/receiver_address (documented field-naming gap
 * from when mempool.h was designed). Transaction.timestamp is set to "now"
 * at selection time since MempoolEntry doesn't track submission time;
 * sender_nonce/digital_signature aren't carried by MempoolEntry at all and
 * are zeroed here. */
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
        if (entry->status != TX_PENDING) {
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

/* Full mining pipeline per spec steps 1-6:
 *   1. select_block_transactions: top-priority PENDING transactions
 *   2. compute_merkle_root: merkle root from those transactions
 *   3. assemble remaining header fields (block expects block_id,
 *      timestamp, previous_hash, miner_id, difficulty to ALREADY be set
 *      by the caller before calling mine_block - see note below)
 *   4-6. nonce search loop via compute_pow_hash + meets_difficulty
 *
 * ASSUMPTION: the caller is responsible for pre-filling block->block_id,
 * block->previous_hash, and block->miner_id before calling mine_block,
 * since those depend on chain state (the current tip) and which miner is
 * mining, neither of which pow.h's narrow scope (pure PoW mechanics)
 * should reach into blockchain.h/account.h to determine itself. mine_block
 * DOES set block->timestamp (to the time mining starts) and
 * block->difficulty (from chain_state.difficulty, the current network
 * difficulty target) itself, since those are PoW-mechanics concerns.
 *
 * Returns 0 (MINE_BLOCK_OK) on success - block->transactions,
 * block->transaction_count, block->merkle_root, block->timestamp,
 * block->difficulty, and block->nonce are all populated, and the block's
 * hash (recomputable via compute_pow_hash) satisfies
 * chain_state.difficulty leading zero hex digits.
 *
 * Returns MINE_BLOCK_NO_PENDING_TX if the mempool has no PENDING
 * transactions to mine - per the spec's step 1, a block needs transactions
 * to select; mining an empty block isn't described as a supported case
 * here (unlike genesis, which is constructed separately and never goes
 * through mine_block at all).
 *
 * NOTE: the nonce-search loop is unbounded and synchronous - for the
 * difficulty values used in a teaching/demo context this resolves
 * quickly, but this function will block the caller for as long as the
 * search takes, by design (this mirrors how real PoW works: there's no
 * shortcut, only brute force). */
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

/* Recomputes block's hash from its own header fields and checks it meets
 * block.difficulty leading zeros - i.e. "is this block's recorded state
 * internally consistent with having actually been mined at the difficulty
 * it claims." Does NOT check previous_hash linkage to a prior block (that
 * is validate_chain's job in blockchain.c, which has access to the full
 * chain) - validate_pow only checks this one block's own PoW validity in
 * isolation, matching the header's signature of taking a single Block by
 * value with no chain context.
 *
 * Returns 1 if valid, 0 otherwise. */
int validate_pow(Block block)
{
    char hash[SHA256_HEX_SIZE];
    compute_pow_hash(&block, hash);
    return meets_difficulty(hash, block.difficulty);
}