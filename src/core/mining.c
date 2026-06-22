#include "../../include/models/account.h"
#include "../../include/core/block.h"
#include "../../include/core/blockchain.h"
#include "../../include/core/chainstate.h"
#include "../../include/core/mempool.h"
#include "../../include/core/mining.h"
#include "../../include/core/pow.h"
#include "../../include/core/token.h"
#include "../../include/security/sha256.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define COINBASE_SENDER "SYSTEM"

/* IMPORTANT: this hash format must stay byte-for-byte identical to
 * compute_pow_hash (static, in pow.c) and compute_block_hash (static, in
 * blockchain.c). All three exist because pow.h's fixed signature gives
 * mining.c no way to call pow.c's version, and blockchain.c's is similarly
 * file-local. This is a known fragility: if you ever change the hash
 * format, it must be changed in all three places identically, or
 * add_block()/validate_chain()/validate_pow() will disagree about what a
 * block's "real" hash is. Strongly recommend consolidating these into one
 * shared, non-static function (e.g. in a small hashing.c) once you're free
 * to adjust the headers - flagging rather than silently leaving this. */
static void recompute_tip_hash(const Block *block, char output[SHA256_HEX_SIZE])
{
    char buffer[SHA256_HEX_SIZE * 2 + 256];
    snprintf(buffer, sizeof(buffer), "%d|%ld|%s|%s|%ld|%d|%s",
              block->block_id, block->timestamp, block->previous_hash,
              block->merkle_root, block->nonce, block->difficulty, block->miner_id);
    sha256(buffer, output);
}

/* ---------------------------------------------------------------------
 * Shared helpers
 * --------------------------------------------------------------------- */

/* Ensures an account exists for receiving a reward payout, creating it as
 * an ACCOUNT_MINER account if it doesn't exist yet. Mining rewards need
 * somewhere to land even for a brand-new miner_id nobody has registered. */
static void ensure_miner_account(const char *miner_id)
{
    if (account_find(miner_id) == NULL) {
        account_create(miner_id, ACCOUNT_MINER);
    }
}

/* Pays `amount` AHT to receiver_address as a coinbase-style reward: newly
 * created value, not transferred from an existing balance, so this credits
 * the receiver directly rather than going through account_apply_transaction
 * (which requires a real, already-existing sender account to debit - there
 * is no "SYSTEM" account in the account-balance model, by design, since the
 * reward is minted, not transferred). Also mints the same amount into the
 * token's total_supply, since this is the one place new AHT enters
 * circulation. */
static void pay_reward(const char *receiver_address, double amount)
{
    ensure_miner_account(receiver_address);
    Account *receiver = account_find(receiver_address);
    receiver->balance += amount;
    token_mint(amount);
}

/* Builds a record-keeping Transaction for a reward payout / pool
 * membership event and appends it as one more entry in the block's
 * transactions array. ASSUMPTION: transaction_id format
 * "reward-<block_id>-<receiver>" / "pool-member-<block_id>-<receiver>" -
 * no format was specified for these, since they're synthetic
 * record-keeping transactions rather than ones a user submitted through
 * the mempool. */
static void append_record_transaction(Block *block, int *capacity, const char *id_prefix,
                                       const char *sender, const char *receiver,
                                       double amount, const char *type)
{
    int idx = block->transaction_count;

    if (idx >= *capacity) {
        int new_capacity = (*capacity == 0) ? 4 : (*capacity * 2);
        Transaction *resized = realloc(block->transactions, sizeof(Transaction) * new_capacity);
        if (resized == NULL) {
            fprintf(stderr, "append_record_transaction: out of memory, dropping record tx\n");
            return;
        }
        block->transactions = resized;
        *capacity = new_capacity;
    }

    Transaction *tx = &block->transactions[idx];
    snprintf(tx->transaction_id, TX_ID_LEN, "%s-%d-%s", id_prefix, block->block_id, receiver);
    strncpy(tx->sender_address, sender, sizeof(tx->sender_address) - 1);
    tx->sender_address[sizeof(tx->sender_address) - 1] = '\0';
    strncpy(tx->receiver_address, receiver, sizeof(tx->receiver_address) - 1);
    tx->receiver_address[sizeof(tx->receiver_address) - 1] = '\0';
    tx->amount = amount;
    strncpy(tx->transaction_type, type, sizeof(tx->transaction_type) - 1);
    tx->transaction_type[sizeof(tx->transaction_type) - 1] = '\0';
    tx->timestamp = block->timestamp;
    tx->sender_nonce = 0;
    tx->digital_signature[0] = '\0';

    block->transaction_count++;
}

/* After a block is successfully added to the chain, marks each of its
 * (originally mempool-sourced) transactions CONFIRMED and removes them
 * from the mempool - spec step 7. Record-keeping transactions added by
 * append_record_transaction (rewards, pool membership) were never in the
 * mempool to begin with, so this only needs to touch the transactions
 * that mine_block() originally selected - mined_tx_count captures that
 * boundary, since reward/membership transactions are appended afterward
 * and would have no matching mempool entry to remove anyway (mempool_remove
 * is a no-op for unknown ids, so calling it on them would be harmless, but
 * we avoid the wasted lookups by stopping at the original count). */
static void finalize_mempool(const Block *block, int mined_tx_count)
{
    for (int i = 0; i < mined_tx_count; i++) {
        mempool_mark_status(block->transactions[i].transaction_id, TX_CONFIRMED);
        mempool_remove(block->transactions[i].transaction_id);
    }
}

/* ---------------------------------------------------------------------
 * Difficulty retargeting (spec 1.iii)
 * --------------------------------------------------------------------- */

void check_difficulty_retarget()
{
    if (blockchain.height < RETARGET_INTERVAL) {
        return;
    }
    if (blockchain.height - chain_state.last_retarget_block < RETARGET_INTERVAL) {
        return;
    }

    /* average_time = sum of (block[i].timestamp - block[i-1].timestamp)
       for the last RETARGET_INTERVAL blocks, divided by RETARGET_INTERVAL.
       "the last 10 blocks" worth of consecutive deltas means we need 10
       pairs, i.e. blocks from (height - RETARGET_INTERVAL) to (height - 1)
       inclusive, each compared to its immediate predecessor. */
    int newest = blockchain.height - 1;
    int oldest = newest - RETARGET_INTERVAL;   /* one before the window, for the first delta */

    long total_time = 0;
    for (int i = oldest + 1; i <= newest; i++) {
        total_time += blockchain.blocks[i].timestamp - blockchain.blocks[i - 1].timestamp;
    }
    double average_time = (double)total_time / RETARGET_INTERVAL;

    int old_difficulty = chain_state.difficulty;
    int new_difficulty = old_difficulty;

    if (average_time < RETARGET_TARGET_LOW_SEC) {
        new_difficulty = old_difficulty + 1;
    } else if (average_time > RETARGET_TARGET_HIGH_SEC) {
        new_difficulty = old_difficulty - 1;
        if (new_difficulty < MIN_DIFFICULTY) {
            new_difficulty = MIN_DIFFICULTY;
        }
    }

    chain_state.difficulty = new_difficulty;
    chain_state.last_retarget_block = blockchain.height;

    printf("=== Difficulty Retarget ===\n");
    printf("  old difficulty:     %d\n", old_difficulty);
    printf("  new difficulty:     %d\n", new_difficulty);
    printf("  average block time: %.2f sec\n", average_time);
    printf("============================\n");
}

/* ---------------------------------------------------------------------
 * Solo mining (spec 1.iv)
 * --------------------------------------------------------------------- */

int mine_block_solo(const char *miner_id)
{
    Block block;
    Block *tip = get_latest_block();

    block.block_id = (tip != NULL) ? tip->block_id + 1 : 0;
    if (tip != NULL) {
        recompute_tip_hash(tip, block.previous_hash);
    } else {
        strcpy(block.previous_hash, "0");
    }
    strncpy(block.miner_id, miner_id, sizeof(block.miner_id) - 1);
    block.miner_id[sizeof(block.miner_id) - 1] = '\0';

    int pow_result = mine_block(&block);
    if (pow_result != 0) {
        return MINE_NO_PENDING_TX;
    }

    int mined_tx_count = block.transaction_count;
    int tx_capacity = mined_tx_count;   /* select_block_transactions in
                                            pow.c allocates exactly
                                            MAX_TX_PER_BLOCK slots, but we
                                            don't have that constant here -
                                            treat current count as the
                                            known-used capacity and let
                                            append_record_transaction grow
                                            it as needed via realloc. */

    append_record_transaction(&block, &tx_capacity, "reward", COINBASE_SENDER,
                               miner_id, chain_state.block_reward, "BLOCK_REWARD");

    int add_result = add_block(block);
    if (add_result != 0) {
        fprintf(stderr, "mine_block_solo: add_block failed (code %d)\n", add_result);
        free(block.transactions);
        return MINE_ADD_BLOCK_FAILED;
    }

    finalize_mempool(&block, mined_tx_count);
    pay_reward(miner_id, chain_state.block_reward);

    check_difficulty_retarget();

    return MINE_OK;
}

/* ---------------------------------------------------------------------
 * Pool mining (spec 1.v)
 * --------------------------------------------------------------------- */

int mine_block_pool(const PoolMiner *miners, int miner_count)
{
    if (miners == NULL || miner_count <= 0) {
        return MINE_NO_POOL_MINERS;
    }

    long total_hashes = 0;
    for (int i = 0; i < miner_count; i++) {
        total_hashes += miners[i].hashes_attempted;
    }
    if (total_hashes <= 0) {
        fprintf(stderr, "mine_block_pool: rejected, no miner reported any hashes attempted\n");
        return MINE_INVALID_HASH_SHARE;
    }

    Block block;
    Block *tip = get_latest_block();

    block.block_id = (tip != NULL) ? tip->block_id + 1 : 0;
    if (tip != NULL) {
        recompute_tip_hash(tip, block.previous_hash);
    } else {
        strcpy(block.previous_hash, "0");
    }
    /* the block's recorded miner_id for a pool block: the first miner in
       the list represents the pool for header-hashing purposes. Individual
       attribution happens via the per-miner payout/membership transactions
       appended below, not via this single header field. */
    strncpy(block.miner_id, miners[0].miner_id, sizeof(block.miner_id) - 1);
    block.miner_id[sizeof(block.miner_id) - 1] = '\0';

    int pow_result = mine_block(&block);
    if (pow_result != 0) {
        return MINE_NO_PENDING_TX;
    }

    int mined_tx_count = block.transaction_count;
    int tx_capacity = mined_tx_count;

    for (int i = 0; i < miner_count; i++) {
        double share = (double)miners[i].hashes_attempted / (double)total_hashes;
        double payout = chain_state.block_reward * share;

        append_record_transaction(&block, &tx_capacity, "pool-member", "POOL",
                                   miners[i].miner_id, (double)miners[i].hashes_attempted,
                                   "POOL_MEMBERSHIP");
        append_record_transaction(&block, &tx_capacity, "reward", COINBASE_SENDER,
                                   miners[i].miner_id, payout, "BLOCK_REWARD");
    }

    int add_result = add_block(block);
    if (add_result != 0) {
        fprintf(stderr, "mine_block_pool: add_block failed (code %d)\n", add_result);
        free(block.transactions);
        return MINE_ADD_BLOCK_FAILED;
    }

    finalize_mempool(&block, mined_tx_count);

    for (int i = 0; i < miner_count; i++) {
        double share = (double)miners[i].hashes_attempted / (double)total_hashes;
        double payout = chain_state.block_reward * share;
        pay_reward(miners[i].miner_id, payout);
    }

    check_difficulty_retarget();

    return MINE_OK;
}