#ifndef TOKEN_H
#define TOKEN_H

typedef struct {
    char token_name[64];
    char token_symbol[16];
    double total_supply;
} Token;

extern Token aht_token;

void init_token();
void token_mint(double amount);
void view_token();

#endif