#ifndef MEMBER_H
#define MEMBER_H

#define MEMBER_OK              0
#define MEMBER_DUPLICATE       1
#define MEMBER_NOT_FOUND       2
 

typedef struct {
    char member_id[64];
    char full_name[128];

    char wallet_address[64];

    char role[32];
    char status[16];

    long registration_date;
} Member;

int register_member(const char *member_address);
Member* find_member(const char member_id);
int view_member(const char *member_id);

#endif