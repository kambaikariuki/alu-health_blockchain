#ifndef BLOCKCHAIN_H
#define BLOCKCHAIN_H

#include "./block.h"

#define MAX_BLOCKS 10000

typedef struct {
    Block blocks[MAX_BLOCKS];
    int height;
} Blockchain;

// Global blockchain
extern Blockchain blockchain;

// functions

void init_blockchain();
Block create_genesis_block();
int add_block(Block new_block);
Block* get_latest_block();
int validate_chain();
void view_blockchain();

#endif