#ifndef ACCOUNT_H
#define ACCOUNT_H

#define ACCOUNT_ADDRESS_LEN 64
#define ACCOUNT_INITIAL_CAPACITY 16

typedef enum {
    ACCOUNT_MEMBER = 0,
    ACCOUNT_PROVIDER,
    ACCOUNT_INSURANCE_POOL,
    ACCOUNT_MINER,
    ACCOUNT_REINSURANCE_POOL,
} AccountType;

typedef struct {
    char address[ACCOUNT_ADDRESS_LEN];
    AccountType type;
    double balance;
    long nonce;
    double claim_average;
    long claim_settlement_count;
} Account;

typedef struct {
    Account *accounts;   /* malloc'd, grows via realloc */
    int count;
    int capacity;
} AccountSet;

/* Global account set, defined once in account.c */
extern AccountSet account_set;

#define INSURANCE_POOL_ADDRESS "INSURANCE_POOL"
#define REINSURANCE_POOL_ADDRESS "REINSURANCE_POOL"

void init_accounts();

int account_create(const char *address, AccountType type);

/* Returns a pointer to the account at `address`, or NULL if it doesn't
 * exist. */
Account* account_find(const char *address);

int account_validate_transaction(const char *sender_address, long sender_nonce, double amount);

int account_apply_transaction(const char *sender_address, const char *receiver_address, double amount);
void account_record_claim_settlement(const char *provider_address, double amount);
 

void free_accounts();
void view_accounts();

#endif