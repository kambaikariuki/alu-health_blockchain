#include "../../include/models/account.h"
#include "../../include/core/block.h"
#include "../../include/core/blockchain.h"
#include "../../include/core/chainstate.h"
#include "../../include/core/mempool.h"
#include "../../include/core/persistence.h"
#include "../../include/insurance/policy.h"
#include "../../include/core/token.h"
#include "../../include/models/utxo.h"
#include "../../include/insurance/policy_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int write_u32(FILE *f, unsigned int value)
{
    return fwrite(&value, sizeof(value), 1, f) == 1;
}

static int read_u32(FILE *f, unsigned int *out)
{
    return fread(out, sizeof(*out), 1, f) == 1;
}

static int write_i32(FILE *f, int value)
{
    return fwrite(&value, sizeof(value), 1, f) == 1;
}

static int read_i32(FILE *f, int *out)
{
    return fread(out, sizeof(*out), 1, f) == 1;
}

/* Generic "write count then count records" helper, used for every flat
 * (pointer-free) array: MempoolEntry, Account, UTXO, Policy, and
 * Transaction (per-block). */
static int write_array(FILE *f, const void *data, size_t elem_size, int count)
{
    if (!write_i32(f, count)) return 0;
    if (count <= 0) return 1;
    return fwrite(data, elem_size, (size_t)count, f) == (size_t)count;
}

/* Reads a count-prefixed array into a freshly malloc'd buffer (or NULL if
 * count is 0). Caller takes ownership of *out_data. Returns 1 on success
 * (including the count==0/NULL case), 0 on read failure or allocation
 * failure. *out_count is always set on success. */
static int read_array(FILE *f, void **out_data, size_t elem_size, int *out_count)
{
    int count;
    if (!read_i32(f, &count) || count < 0) {
        return 0;
    }

    if (count == 0) {
        *out_data = NULL;
        *out_count = 0;
        return 1;
    }

    void *buf = malloc(elem_size * (size_t)count);
    if (buf == NULL) {
        return 0;
    }

    if (fread(buf, elem_size, (size_t)count, f) != (size_t)count) {
        free(buf);
        return 0;
    }

    *out_data = buf;
    *out_count = count;
    return 1;
}

int chain_save(const char *path)
{
    FILE *f = fopen(path, "wb");
    if (f == NULL) {
        fprintf(stderr, "chain_save: failed to open %s for writing\n", path);
        return PERSIST_FILE_ERROR;
    }

    int ok = 1;
    ok = ok && write_u32(f, PERSISTENCE_MAGIC);
    ok = ok && write_u32(f, PERSISTENCE_VERSION);

    ok = ok && (fwrite(&chain_state, sizeof(ChainState), 1, f) == 1);
    ok = ok && (fwrite(&aht_token, sizeof(Token), 1, f) == 1);

    ok = ok && write_i32(f, blockchain.height);
    for (int i = 0; ok && i < blockchain.height; i++) {
        Block *b = &blockchain.blocks[i];
        ok = ok && (fwrite(b, sizeof(Block), 1, f) == 1);
        ok = ok && write_array(f, b->transactions, sizeof(Transaction), b->transaction_count);
    }

    ok = ok && write_array(f, mempool.entries, sizeof(MempoolEntry), mempool.count);
    ok = ok && write_array(f, account_set.accounts, sizeof(Account), account_set.count);
    ok = ok && write_array(f, utxo_set.outputs, sizeof(UTXO), utxo_set.count);

    int policy_count = policy_count_internal();
    ok = ok && write_i32(f, policy_count);
    for (int i = 0; ok && i < policy_count; i++) {
        Policy *p = policy_at_internal(i);
        ok = ok && (fwrite(p, sizeof(Policy), 1, f) == 1);
    }

    fclose(f);

    if (!ok) {
        fprintf(stderr, "chain_save: write error while saving to %s\n", path);
        remove(path);   /* don't leave a half-written, misleading file behind */
        return PERSIST_FILE_ERROR;
    }

    printf("Saved chain snapshot to %s (%d blocks, %d mempool entries, %d accounts, "
           "%d UTXOs, %d policies).\n",
           path, blockchain.height, mempool.count, account_set.count, utxo_set.count, policy_count);

    return PERSIST_OK;
}

static void free_temp_block_transactions(Transaction **tx_arrays, int up_to)
{
    for (int i = 0; i < up_to; i++) {
        free(tx_arrays[i]);
    }
}

int chain_load(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        fprintf(stderr, "chain_load: failed to open %s for reading\n", path);
        return PERSIST_FILE_ERROR;
    }

    unsigned int magic, version;
    if (!read_u32(f, &magic) || magic != PERSISTENCE_MAGIC) {
        fprintf(stderr, "chain_load: %s is not a valid chain snapshot file\n", path);
        fclose(f);
        return PERSIST_BAD_MAGIC;
    }
    if (!read_u32(f, &version) || version != PERSISTENCE_VERSION) {
        fprintf(stderr, "chain_load: %s has unsupported format version\n", path);
        fclose(f);
        return PERSIST_BAD_VERSION;
    }

    ChainState temp_chain_state;
    Token temp_token;
    if (fread(&temp_chain_state, sizeof(ChainState), 1, f) != 1 ||
        fread(&temp_token, sizeof(Token), 1, f) != 1) {
        fprintf(stderr, "chain_load: truncated file (chain_state/token)\n");
        fclose(f);
        return PERSIST_TRUNCATED;
    }

    int temp_height;
    if (!read_i32(f, &temp_height) || temp_height < 0 || temp_height > MAX_BLOCKS) {
        fprintf(stderr, "chain_load: truncated or invalid file (blockchain height)\n");
        fclose(f);
        return PERSIST_TRUNCATED;
    }

    Block *temp_blocks = malloc(sizeof(Block) * MAX_BLOCKS);
    Transaction **temp_tx_arrays = malloc(sizeof(Transaction *) * (temp_height > 0 ? temp_height : 1));
    if (temp_blocks == NULL || temp_tx_arrays == NULL) {
        fprintf(stderr, "chain_load: out of memory\n");
        free(temp_blocks);
        free(temp_tx_arrays);
        fclose(f);
        return PERSIST_OUT_OF_MEMORY;
    }

    int blocks_ok = 1;
    int blocks_read = 0;
    for (int i = 0; blocks_ok && i < temp_height; i++) {
        if (fread(&temp_blocks[i], sizeof(Block), 1, f) != 1) {
            blocks_ok = 0;
            break;
        }
        void *tx_data = NULL;
        int tx_count = 0;
        if (!read_array(f, &tx_data, sizeof(Transaction), &tx_count)) {
            blocks_ok = 0;
            break;
        }
        temp_blocks[i].transactions = (Transaction *)tx_data;
        temp_blocks[i].transaction_count = tx_count;
        temp_tx_arrays[i] = (Transaction *)tx_data;
        blocks_read++;
    }

    if (!blocks_ok) {
        fprintf(stderr, "chain_load: truncated or corrupt file (block data)\n");
        free_temp_block_transactions(temp_tx_arrays, blocks_read);
        free(temp_blocks);
        free(temp_tx_arrays);
        fclose(f);
        return PERSIST_TRUNCATED;
    }

    void *temp_mempool_entries = NULL;
    int temp_mempool_count = 0;
    void *temp_accounts = NULL;
    int temp_account_count = 0;
    void *temp_utxos = NULL;
    int temp_utxo_count = 0;
    void *temp_policies = NULL;
    int temp_policy_count = 0;

    int rest_ok = 1;
    rest_ok = rest_ok && read_array(f, &temp_mempool_entries, sizeof(MempoolEntry), &temp_mempool_count);
    rest_ok = rest_ok && read_array(f, &temp_accounts, sizeof(Account), &temp_account_count);
    rest_ok = rest_ok && read_array(f, &temp_utxos, sizeof(UTXO), &temp_utxo_count);
    rest_ok = rest_ok && read_array(f, &temp_policies, sizeof(Policy), &temp_policy_count);

    fclose(f);

    if (!rest_ok) {
        fprintf(stderr, "chain_load: truncated or corrupt file (mempool/account/utxo/policy data)\n");
        free_temp_block_transactions(temp_tx_arrays, blocks_read);
        free(temp_blocks);
        free(temp_tx_arrays);
        free(temp_mempool_entries);
        free(temp_accounts);
        free(temp_utxos);
        free(temp_policies);
        return PERSIST_TRUNCATED;
    }

    for (int i = 0; i < blockchain.height; i++) {
        free(blockchain.blocks[i].transactions);
    }
    for (int i = 0; i < temp_height; i++) {
        blockchain.blocks[i] = temp_blocks[i];
    }
    blockchain.height = temp_height;
    free(temp_blocks);
    free(temp_tx_arrays);   

    chain_state = temp_chain_state;
    aht_token = temp_token;

    free(mempool.entries);
    mempool.entries = (MempoolEntry *)temp_mempool_entries;
    mempool.count = temp_mempool_count;
    mempool.capacity = temp_mempool_count > 0 ? temp_mempool_count : MEMPOOL_INITIAL_CAPACITY;
    if (mempool.count == 0) {
        /* read_array returned NULL data for a zero-count array - need a
           real allocated buffer with real capacity for future mempool_add
           calls to grow from, not a NULL/0 starting point (see the
           capacity-doubling bug this avoids: 0 * 2 stays 0 forever). */
        mempool.entries = malloc(sizeof(MempoolEntry) * MEMPOOL_INITIAL_CAPACITY);
    }

    free(account_set.accounts);
    account_set.accounts = (Account *)temp_accounts;
    account_set.count = temp_account_count;
    account_set.capacity = temp_account_count > 0 ? temp_account_count : ACCOUNT_INITIAL_CAPACITY;
    if (account_set.count == 0) {
        account_set.accounts = malloc(sizeof(Account) * ACCOUNT_INITIAL_CAPACITY);
    }

    free(utxo_set.outputs);
    utxo_set.outputs = (UTXO *)temp_utxos;
    utxo_set.count = temp_utxo_count;
    utxo_set.capacity = temp_utxo_count > 0 ? temp_utxo_count : UTXO_INITIAL_CAPACITY;
    if (utxo_set.count == 0) {
        utxo_set.outputs = malloc(sizeof(UTXO) * UTXO_INITIAL_CAPACITY);
    }

    policy_load_internal((Policy *)temp_policies, temp_policy_count);
    free(temp_policies);   /* policy_load_internal copies into its own
                               storage, so this temporary buffer is safe
                               to free immediately afterward */

    printf("Loaded chain snapshot from %s (%d blocks, %d mempool entries, %d accounts, "
           "%d UTXOs, %d policies).\n",
           path, blockchain.height, mempool.count, account_set.count, utxo_set.count, temp_policy_count);

    return PERSIST_OK;
}