#ifndef PERSISTENCE_H
#define PERSISTENCE_H

#define PERSISTENCE_MAGIC 0x41484331u   
#define PERSISTENCE_VERSION 1

#define PERSIST_OK              0
#define PERSIST_FILE_ERROR      1
#define PERSIST_BAD_MAGIC       2
#define PERSIST_BAD_VERSION     3
#define PERSIST_OUT_OF_MEMORY   4
#define PERSIST_TRUNCATED       5

int chain_save(const char *path);

int chain_load(const char *path);

#endif