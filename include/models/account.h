#ifndef ACCOUNT_H
#define ACCOUNT_H

typedef struct {
    char address[64];
    double balance;
    long nonce;
} Account;

void create_account(char* address);
Account* get_account(char* address);
int transfer_funds(char* from, char* to, double amount);

#endif