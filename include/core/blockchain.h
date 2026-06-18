#ifndef BLOCKCHAIN_H
#define BLOCKCHAIN_H

#include <block.h>

#define MAX_BLOCKS 10000

typedef struct {
    Block blocks[MAX_BLOCKS];
    int height;
} Blockchain;

// functions

void init_blockchain();
Block create_genesis_block();
void add_block();
Block* get_latest_block();
int validate_chain();

#endif