#ifndef MEMBER_H
#define MEMBER_H

typedef struct {
    char member_id[64];
    char full_name[128];

    char wallet_address[64];

    char role[32];
    char status[16];

    long registration_date;
} Member;

#endif