#ifndef TOKEN_H
#define TOKEN_H

typedef struct {
    char token_name[64];
    char token_symbol[16];
    double total_supply;
} Token;

#endif