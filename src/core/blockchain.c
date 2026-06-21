#include "../../include/core/block.h"
#include "../../include/core/blockchain.h"
#include "../../include/core/chainstate.h"
#include "../../include/core/merkle.h"
#include "../../include/security/sha256.h"

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>

#define ADD_BLOCK_OK                0
#define ADD_BLOCK_CHAIN_FULL        1
#define ADD_BLOCK_INVALID_BLOCK_ID  2
#define ADD_BLOCK_INVALID_PREV_HASH 3
#define ADD_BLOCK_OUT_OF_MEMORY     4 

/* Global blockchain state */
Blockchain blockchain;
ChainState chain_state;

void init_blockchain()
{
    blockchain.height = 0;

    init_chain_state();

    Block genesis = create_genesis_block();

    int result = add_block(genesis);
    if (result != ADD_BLOCK_OK) {
        fprintf(stderr, "FATAL: failed to add genesis block (code %d)\n", result);
        exit(EXIT_FAILURE);
    }
}

Block create_genesis_block()
{
    Block genesis;

    genesis.block_id = 0;
    genesis.timestamp = time(NULL);
    genesis.transaction_count = 0;
    genesis.transactions = NULL;

    strcpy(genesis.previous_hash, "0");
    strcpy(genesis.merkle_root, "0");

    genesis.nonce = 0;
    strcpy(genesis.miner_id, "SYSTEM");


    genesis.difficulty = chain_state.difficulty;

    return genesis;
}


Block* get_latest_block()
{
    if (blockchain.height <= 0) {
        return NULL;
    }
    return &blockchain.blocks[blockchain.height - 1];
}

static void compute_block_hash(const Block *block, char output[SHA256_HEX_SIZE])
{
    char buffer[SHA256_HEX_SIZE * 2 + 128];
 
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

int add_block(Block new_block)
{
    if (blockchain.height >= MAX_BLOCKS) {
        fprintf(stderr, "add_block: chain is full (MAX_BLOCKS=%d)\n", MAX_BLOCKS);
        return ADD_BLOCK_CHAIN_FULL;
    }
 
    if (blockchain.height > 0) {
        Block *tip = get_latest_block();
 
        if (new_block.block_id != tip->block_id + 1) {
            fprintf(stderr, "add_block: rejected, block_id %d is not sequential (expected %d)\n",
                    new_block.block_id, tip->block_id + 1);
            return ADD_BLOCK_INVALID_BLOCK_ID;
        }
 
        char tip_hash[SHA256_HEX_SIZE];
        compute_block_hash(tip, tip_hash);
 
        if (strcmp(new_block.previous_hash, tip_hash) != 0) {
            fprintf(stderr, "add_block: rejected, previous_hash does not match chain tip\n");
            return ADD_BLOCK_INVALID_PREV_HASH;
        }
    }
 
    Block *slot = &blockchain.blocks[blockchain.height];
 
    /* shallow copy first: copies every fixed-size field, plus a
     * transactions pointer that currently aliases new_block's own array */
    *slot = new_block;
 
    /* break the alias with a deep copy, so slot fully owns its own memory
     * independent of whatever new_block.transactions pointed to */
    if (new_block.transaction_count > 0 && new_block.transactions != NULL) {
        slot->transactions = malloc(sizeof(Transaction) * new_block.transaction_count);
        if (slot->transactions == NULL) {
            fprintf(stderr, "add_block: out of memory copying transactions\n");
            return ADD_BLOCK_OUT_OF_MEMORY;
        }
        memcpy(slot->transactions, new_block.transactions,
               sizeof(Transaction) * new_block.transaction_count);
    } else {
        slot->transactions = NULL;
    }
 
    blockchain.height++;
    return ADD_BLOCK_OK;
}

int validate_chain()
{
    if (blockchain.height <= 0) {
        return 1;
    }
 
    for (int i = 1; i < blockchain.height; i++) {
        Block *prev = &blockchain.blocks[i - 1];
        Block *curr = &blockchain.blocks[i];
 
        char prev_hash[SHA256_HEX_SIZE];
        compute_block_hash(prev, prev_hash);
 
        if (strcmp(curr->previous_hash, prev_hash) != 0) {
            return 2;
        }
 
        char recomputed_root[SHA256_HEX_SIZE];
        compute_merkle_root(curr->transactions, curr->transaction_count, recomputed_root);
 
        if (strcmp(curr->merkle_root, recomputed_root) != 0) {
            return 3;
        }
    }
 
    return 0;
}

void view_blockchain()
{
    if (blockchain.height <= 0) {
        printf("Blockchain is empty.\n");
        return;
    }

    printf("===== Blockchain (%d block%s) =====\n\n",
           blockchain.height, blockchain.height == 1 ? "" : "s");

    for (int i = 0; i < blockchain.height; i++) {
        Block *b = &blockchain.blocks[i];

        printf("--- Block %d ---\n", b->block_id);
        printf("  timestamp:         %ld (%s", b->timestamp, ctime(&b->timestamp));
        printf("  transaction_count: %d\n", b->transaction_count);
        printf("  previous_hash:     %s\n", b->previous_hash);
        printf("  merkle_root:       %s\n", b->merkle_root);
        printf("  nonce:             %ld\n", b->nonce);
        printf("  miner_id:          %s\n", b->miner_id);
        printf("  difficulty:        %d\n", b->difficulty);

        if (b->transaction_count > 0 && b->transactions != NULL) {
            printf("  transactions:\n");
            for (int t = 0; t < b->transaction_count; t++) {
                Transaction *tx = &b->transactions[t];
                printf("    [%d] id:        %s\n", t, tx->transaction_id);
                printf("        sender:    %s\n", tx->sender_address);
                printf("        receiver:  %s\n", tx->receiver_address);
                printf("        amount:    %.8f\n", tx->amount);
                printf("        type:      %s\n", tx->transaction_type);
                printf("        timestamp: %ld (%s", tx->timestamp, ctime(&tx->timestamp));
                printf("        nonce:     %ld\n", tx->sender_nonce);
                printf("        signature: %s\n", tx->digital_signature);
            }
        } else {
            printf("  transactions:      (none)\n");
        }

        printf("\n");
    }

    printf("===== End of chain =====\n");
}