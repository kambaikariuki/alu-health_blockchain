#include "../../include/models/account.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ACCT_OK                0
#define ACCT_OUT_OF_MEMORY     1
#define ACCT_DUPLICATE_ADDRESS 2
#define ACCT_NOT_FOUND         3

AccountSet account_set;

void init_accounts()
{
    account_set.accounts = malloc(sizeof(Account) * ACCOUNT_INITIAL_CAPACITY);
    if (account_set.accounts == NULL) {
        fprintf(stderr, "FATAL: failed to allocate initial account set\n");
        exit(EXIT_FAILURE);
    }
    account_set.count = 0;
    account_set.capacity = ACCOUNT_INITIAL_CAPACITY;

    /* singleton pool wallets, see ACCOUNT_H's note on this assumption */
    account_create(INSURANCE_POOL_ADDRESS, ACCOUNT_INSURANCE_POOL);
    account_create(REINSURANCE_POOL_ADDRESS, ACCOUNT_REINSURANCE_POOL);
}

Account* account_find(const char *address)
{
    for (int i = 0; i < account_set.count; i++) {
        if (strcmp(account_set.accounts[i].address, address) == 0) {
            return &account_set.accounts[i];
        }
    }
    return NULL;
}

int account_create(const char *address, AccountType type)
{
    if (account_find(address) != NULL) {
        fprintf(stderr, "account_create: rejected, duplicate address %s\n", address);
        return ACCT_DUPLICATE_ADDRESS;
    }

    if (account_set.count >= account_set.capacity) {
        int new_capacity = account_set.capacity * 2;
        Account *resized = realloc(account_set.accounts, sizeof(Account) * new_capacity);
        if (resized == NULL) {
            fprintf(stderr, "account_create: out of memory growing account set\n");
            return ACCT_OUT_OF_MEMORY;
        }
        account_set.accounts = resized;
        account_set.capacity = new_capacity;
    }

    Account *acct = &account_set.accounts[account_set.count];
    strncpy(acct->address, address, ACCOUNT_ADDRESS_LEN - 1);
    acct->address[ACCOUNT_ADDRESS_LEN - 1] = '\0';
    acct->type = type;
    acct->balance = 0.0;
    acct->nonce = 0;
    acct->claim_average = 0.0;
    acct->claim_settlement_count = 0;

    account_set.count++;
    return ACCT_OK;
}

int account_validate_transaction(const char *sender_address, long sender_nonce, double amount)
{
    Account *sender = account_find(sender_address);
    if (sender == NULL) {
        fprintf(stderr, "account_validate_transaction: rejected, sender %s does not exist\n",
                sender_address);
        return 0;
    }

    if (sender_nonce != sender->nonce + 1) {
        fprintf(stderr,
                "account_validate_transaction: rejected, nonce mismatch for %s (got %ld, expected %ld)\n",
                sender_address, sender_nonce, sender->nonce + 1);
        return 0;
    }

    if (sender->balance < amount) {
        fprintf(stderr,
                "account_validate_transaction: rejected, insufficient balance for %s (has %.8f, needs %.8f)\n",
                sender_address, sender->balance, amount);
        return 0;
    }

    return 1;
}

int account_apply_transaction(const char *sender_address, const char *receiver_address, double amount)
{
    Account *sender = account_find(sender_address);
    Account *receiver = account_find(receiver_address);

    if (sender == NULL) {
        fprintf(stderr, "account_apply_transaction: sender %s does not exist\n", sender_address);
        return ACCT_NOT_FOUND;
    }
    if (receiver == NULL) {
        fprintf(stderr, "account_apply_transaction: receiver %s does not exist\n", receiver_address);
        return ACCT_NOT_FOUND;
    }

    sender->balance -= amount;
    receiver->balance += amount;
    sender->nonce += 1;

    return ACCT_OK;
}

void account_record_claim_settlement(const char *provider_address, double amount)
{
    Account *provider = account_find(provider_address);
    if (provider == NULL) {
        fprintf(stderr, "account_record_claim_settlement: provider %s does not exist\n",
                provider_address);
        return;
    }

    provider->claim_settlement_count++;
    provider->claim_average += (amount - provider->claim_average) / provider->claim_settlement_count;
}

void free_accounts()
{
    free(account_set.accounts);
    account_set.accounts = NULL;
    account_set.count = 0;
    account_set.capacity = 0;
}

static const char* type_to_string(AccountType type)
{
    switch (type) {
        case ACCOUNT_MEMBER:           return "MEMBER";
        case ACCOUNT_PROVIDER:         return "PROVIDER";
        case ACCOUNT_INSURANCE_POOL:   return "INSURANCE_POOL";
        case ACCOUNT_MINER:            return "MINER";
        case ACCOUNT_REINSURANCE_POOL: return "REINSURANCE_POOL";
        default:                       return "UNKNOWN";
    }
}

void view_accounts()
{
    if (account_set.count <= 0) {
        printf("No accounts exist.\n");
        return;
    }

    printf("===== Accounts (%d) =====\n\n", account_set.count);

    for (int i = 0; i < account_set.count; i++) {
        Account *a = &account_set.accounts[i];
        printf("[%d] address: %s\n", i, a->address);
        printf("    type:    %s\n", type_to_string(a->type));
        printf("    balance: %.8f\n", a->balance);
        printf("    nonce:   %ld\n", a->nonce);
        printf("    claim_average: %.8f (n=%ld)\n\n", a->claim_average, a->claim_settlement_count);
    }

    printf("===== End of accounts =====\n");
}