#include "../../include/core/chainstate.h"

void init_chain_state()
{
    chain_state.difficulty = 2;
    chain_state.block_reward = 50;
    chain_state.last_retarget_block = 0;
    chain_state.reinsurance_balance = 0.0;
}