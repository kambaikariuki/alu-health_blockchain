#ifndef POW_H
#define POW_H

#include <block.h>

int mine_block(Block* block);
int validate_pow(Block block);
int meets_difficulty(const char* hash, int difficulty);

#endif