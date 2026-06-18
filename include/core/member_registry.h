#ifndef MEMBER_REGISTRY_H
#define MEMBER_REGISTRY_H

#define MAX_MEMBERS 1000
#include <member.h>

typedef struct {
    Member members[MAX_MEMBERS];
    int count;
} MemberRegistry;

extern MemberRegistry registry;

void init_member_registry(void);

int register_member(const char *member_id,
                    const char *full_name,
                    const char *role,
                    const char *wallet_address);


// retrieve member by id`                    
Member* get_member_by_id(const char *member_id);

/* Update member status (ACTIVE / SUSPENDED) */
int update_member_status(const char *member_id, const char *status);

/* Validate if member exists and is active */
int is_member_active(const char *member_id);

/* Print member details (CLI) */
void view_member(const char *member_id);

/* List all members */
void list_members(void);

/* Save registry to disk */
int save_member_registry(void);

/* Load registry from disk */
int load_member_registry(void);

#endif