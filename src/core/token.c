#include "../../include/core/token.h"

#include <stdio.h>
#include <string.h>

Token aht_token;

void init_token()
{
    strncpy(aht_token.token_name, "ALU Health Token", sizeof(aht_token.token_name) - 1);
    aht_token.token_name[sizeof(aht_token.token_name) - 1] = '\0';

    strncpy(aht_token.token_symbol, "AHT", sizeof(aht_token.token_symbol) - 1);
    aht_token.token_symbol[sizeof(aht_token.token_symbol) - 1] = '\0';

    aht_token.total_supply = 0.0;
}

void token_mint(double amount)
{
    if (amount <= 0.0) {
        fprintf(stderr, "token_mint: warning, ignoring non-positive amount %.8f\n", amount);
        return;
    }
    aht_token.total_supply += amount;
}

void view_token()
{
    printf("===== Token =====\n");
    printf("  name:          %s\n", aht_token.token_name);
    printf("  symbol:        %s\n", aht_token.token_symbol);
    printf("  total_supply:  %.8f\n", aht_token.total_supply);
    printf("==================\n");
}