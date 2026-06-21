#include <stdio.h>
#include <string.h>

#include "../../include/security/sha256.h"

int main(void)
{
    char hash[SHA256_HEX_SIZE];

    sha256("hello", hash);

    printf("Input : hello\n");
    printf("Hash  : %s\n", hash);

    const char *expected =
        "2cf24dba5fb0a30e26e83b2ac5b9e29e"
        "1b161e5c1fa7425e73043362938b9824";

    if (strcmp(hash, expected) == 0)
    {
        printf("SHA256 TEST PASSED\n");
    }
    else
    {
        printf("TEST FAILED\n");
        printf("Expected: %s\n", expected);
    }

    return 0;
}