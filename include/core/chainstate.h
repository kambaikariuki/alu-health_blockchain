#ifndef CHAINSTATE_H
#define CHAINSTATE_H

typedef struct {
    double block_reward;
    int difficulty;
    int last_retarget_block;
    double reinsurance_balance;
} Chainstate;

extern Chainstate chain_state;

void init_chain_state();

#endif