#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#define BLOCK_SIZE 1024
#define MAX_NAME_SIZE 32

struct DiscInfo
{
    char name[MAX_NAME_SIZE];
    int size;
    int fatSize;
    int rootSize;
    int blockSize;
};

struct VFS
{
    FILE *file;
    struct Block *root;
    int nBlocks;
};

struct Block
{
    char name[MAX_NAME_SIZE];
    int size;
    int firstBlock;
    int isUsed;
};

void create(char *name, int size);
struct VFS *open(char *name);
void closeDisc(struct VFS *vfs);
void copyInto(char *name, char *source, char *destination);
void copyFrom(char *name, char *source, char *destination);
void removeFile(char *name, char *fileName);
void viewFiles(char *name);
void viewMap(char *name);

void create(char *name, int size)
{
    FILE *file;
    int nBlocks;
    struct DiscInfo discInfo;
    struct VFS *vfs;
    struct Block *blocks;
    char *nulls;
    int *fatTable;
    int i;

    nBlocks = (size - sizeof(discInfo)) / (sizeof(struct Block) + BLOCK_SIZE + sizeof(int));
    nulls = malloc(size * sizeof(char));
    memset(nulls, 0, sizeof(nulls));
    file = fopen(name, "w+");
    fwrite(nulls, 1, size, file);
    blocks = malloc(sizeof(struct Block) * nBlocks);
    fatTable = malloc(sizeof(int) * nBlocks);
    discInfo.size = size;
    discInfo.fatSize = sizeof(int) * nBlocks;
    discInfo.rootSize = sizeof(struct Block) * nBlocks;
    discInfo.blockSize = BLOCK_SIZE;
    strncpy(discInfo.name, name, MAX_NAME_SIZE);
    for (i = 0; i < nBlocks; i++)
        fatTable[i] = -1;
    fseek(file, 0, 0);
    fwrite(&discInfo, sizeof(discInfo), 1, file);
    fseek(file, sizeof(discInfo), 0);
    fwrite(fatTable, sizeof(int), nBlocks, file);
    for (i = 0; i < nBlocks; i++)
    {
        strcpy(blocks[i].name, "");
        blocks[i].size = 0;
        blocks[i].firstBlock = -1;
        blocks[i].isUsed = 0;
    }
    vfs = malloc(sizeof(struct VFS));
    vfs->file = file;
    vfs->nBlocks = nBlocks;
    vfs->root = blocks;
    free(nulls);
    free(fatTable);
    closeDisc(vfs);
}

struct VFS *open(char *name)
{
    struct DiscInfo discInfo;
    struct VFS *vfs;
    struct Block *blocks;
    FILE *file;
    int nBlocks;
    int size;

    file = fopen(name, "r+");
    fseek(file, 0, 2);
    size = ftell(file);
    fseek(file, 0, 0);
    fread(&discInfo, sizeof(discInfo), 1, file);
    nBlocks = (discInfo.size - sizeof(discInfo)) / (sizeof(struct Block) + BLOCK_SIZE + sizeof(int));
    blocks = malloc(nBlocks * sizeof(struct Block));
    fseek(file, sizeof(discInfo) + sizeof(int) * nBlocks, 0);
    fread(blocks, sizeof(struct Block), nBlocks, file);
    vfs = malloc(sizeof(struct VFS));
    vfs->file = file;
    vfs->nBlocks = nBlocks;
    vfs->root = blocks;
    return vfs;
}

void closeDisc(struct VFS *vfs)
{
    fseek(vfs->file, sizeof(struct DiscInfo) + sizeof(int) * vfs->nBlocks, 0);
    fwrite(vfs->root, sizeof(struct Block), vfs->nBlocks, vfs->file);
    fclose(vfs->file);
    free(vfs->root);
    free(vfs);
    vfs = NULL;
}

void copyInto(char *name, char *source, char *destination)
{
    FILE *file;
    struct VFS *vfs;
    int sourceFileSize;
    int requiredBlocks;
    char data[BLOCK_SIZE];
    int idxToModify;
    int dataLeft;
    int blockIndex;
    int *blocks;
    int *fatTable;
    int size;
    int pos;
    int i;

    vfs = open(name);
    idxToModify = -1;
    for (i = 0; i < vfs->nBlocks; i++)
    {
        if (vfs->root[i].isUsed == 1 && strcmp(vfs->root[i].name, destination) == 0)
        {
            idxToModify = i;
            break;
        }
    }
    if (idxToModify != -1)
    {
        closeDisc(vfs);
        removeFile(name, destination);
        vfs = open(name);
    }
    file = fopen(source, "r+");
    fatTable = malloc(sizeof(int) * vfs->nBlocks);
    fseek(file, 0, 2);
    sourceFileSize = ftell(file);
    fseek(vfs->file, sizeof(struct DiscInfo), 0);
    fread(fatTable, sizeof(int), vfs->nBlocks, vfs->file);
    fseek(file, 0, 0);
    requiredBlocks = 1;
    dataLeft = sourceFileSize;
    while (dataLeft > BLOCK_SIZE)
    {
        requiredBlocks++;
        dataLeft -= BLOCK_SIZE;
    }
    blocks = malloc(requiredBlocks * sizeof(int));
    blockIndex = 0;
    for (i = 0; i < vfs->nBlocks && blockIndex != requiredBlocks; i++)
    {
        if (!vfs->root[i].isUsed)
        {
            blocks[blockIndex] = i;
            blockIndex++;
        }
    }
    if (blockIndex < requiredBlocks)
    {
        free(blocks);
        fclose(file);
        printf("Not enough free blocks");
        return;
    }
    for (i = 0; i < requiredBlocks; i++)
    {
        size = fread(data, 1, sizeof(data), file);
        vfs->root[blocks[0]].size += size;
        vfs->root[blocks[i]].isUsed = 1;
        vfs->root[blocks[i]].size = size;
        if (!i)
        {
            strncpy(vfs->root[blocks[i]].name, destination, MAX_NAME_SIZE);
            vfs->root[blocks[i]].firstBlock = blocks[i];
        }
        if (i < requiredBlocks - 1)
            fatTable[blocks[i]] = blocks[i + 1];
        if (i == requiredBlocks - 1)
            fatTable[blocks[i]] = -1;
        pos = sizeof(struct DiscInfo) + (sizeof(int) + sizeof(struct Block)) * vfs->nBlocks + BLOCK_SIZE * blocks[i];

        fseek(vfs->file, pos, 0);
        fwrite(data, 1, size, vfs->file);
    }
    fseek(vfs->file, sizeof(struct DiscInfo), 0);
    fwrite(fatTable, sizeof(int), vfs->nBlocks, vfs->file);
    free(fatTable);
    free(blocks);
    fclose(file);
    closeDisc(vfs);
}

void copyFrom(char *name, char *source, char *destination)
{
    FILE *file;
    struct VFS *vfs;
    char data[BLOCK_SIZE];
    int blockIdx;
    int *fatTable;
    int tempSize;
    int size;
    int pos;
    int i;

    vfs = open(name);
    file = fopen(destination, "w+");
    fatTable = malloc(sizeof(int) * vfs->nBlocks);
    fseek(vfs->file, sizeof(struct DiscInfo), 0);
    fread(fatTable, sizeof(int), vfs->nBlocks, vfs->file);
    blockIdx = -1;
    for (i = 0; i < vfs->nBlocks; i++)
    {
        if (vfs->root[i].isUsed && vfs->root[i].firstBlock >= 0 && strcmp(vfs->root[i].name, source) == 0)
        {
            blockIdx = i;
            break;
        }
    }
    size = vfs->root[blockIdx].size;
    while (blockIdx >= 0)
    {
        tempSize = size;
        if (size > BLOCK_SIZE)
        {
            size -= BLOCK_SIZE;
            tempSize = BLOCK_SIZE;
        }
        pos = sizeof(struct DiscInfo) + (sizeof(int) + sizeof(struct Block)) * vfs->nBlocks + BLOCK_SIZE * blockIdx;
        fseek(vfs->file, pos, 0);
        fread(data, 1, tempSize, vfs->file);
        fwrite(data, 1, tempSize, file);
        blockIdx = fatTable[blockIdx];
    }
    free(fatTable);
    fclose(file);
    closeDisc(vfs);
}

void removeFile(char *name, char *fileName)
{
    struct VFS *vfs;
    int i;
    int blockIdx;
    int fatIdx;
    char data[BLOCK_SIZE];
    int *fatTable;

    vfs = open(name);
    fatTable = malloc(sizeof(int) * vfs->nBlocks);
    fseek(vfs->file, sizeof(struct DiscInfo), 0);
    fread(fatTable, sizeof(int), vfs->nBlocks, vfs->file);
    blockIdx = -1;
    for (i = 0; i < vfs->nBlocks; i++)
    {
        if (vfs->root[i].isUsed && strcmp(vfs->root[i].name, fileName) == 0)
        {
            blockIdx = i;
            break;
        }
    }
    while (blockIdx >= 0)
    {
        fatIdx = fatTable[blockIdx];
        vfs->root[blockIdx].isUsed = 0;
        vfs->root[blockIdx].firstBlock = 0;
        vfs->root[blockIdx].size = 0;
        strcpy(vfs->root[blockIdx].name, "");
        fatTable[blockIdx] = -1;
        blockIdx = fatIdx;
    }
    free(fatTable);
    closeDisc(vfs);
}

void viewFiles(char *name)
{
    struct VFS *vfs;
    int i;
    vfs = open(name);
    for (i = 0; i < vfs->nBlocks; i++)
        if (vfs->root[i].isUsed && strcmp(vfs->root[i].name, "") != 0)
            printf("File: %s\tSize: %d\tBeginning: %d\tEnding: %d\n", vfs->root[i].name, vfs->root[i].size, vfs->root[i].firstBlock, vfs->root[i].firstBlock + vfs->root[i].size / BLOCK_SIZE);
    closeDisc(vfs);
}

void viewMap(char *name)
{
    struct DiscInfo discInfo;
    struct VFS *vfs;
    int i;

    vfs = open(name);
    fseek(vfs->file, 0, 0);
    fread(&discInfo, sizeof(discInfo), 1, vfs->file);
    for (i = 0; i < vfs->nBlocks; i++)
        printf("%c%s", discInfo.blockSize == (vfs->root[i].size > discInfo.blockSize ? 0 : discInfo.blockSize - vfs->root[i].size) ? 'F' : 'U', (i + 1) % 64 == 0 ? "\n" : "");
    printf("\n");
    closeDisc(vfs);
}

int main(int argc, char *argv[])
{
    if (argc < 3 || argc > 5)
    {
        printf("Instructions:\n");
        printf("Create VFS: NAME -cr SIZE\n");
        printf("Copy into VFS: NAME -ci SOURCE DESTINATION\n");
        printf("Copy from VFS: NAME -cf SOURCE DESTINATION\n");
        printf("Remove file: NAME -rf FILE\n");
        printf("View files: NAME -vf\n");
        printf("Remove VFS: NAME -rv\n");
        printf("View map: NAME -bm\n");
        return 1;
    }

    if (strcmp(argv[2], "-cr") == 0)
    {
        create(argv[1], atoi(argv[3]));
    }
    else if (strcmp(argv[2], "-ci") == 0)
    {
        copyInto(argv[1], argv[3], argv[4]);
    }
    else if (strcmp(argv[2], "-cf") == 0)
    {
        copyFrom(argv[1], argv[3], argv[4]);
    }
    else if (strcmp(argv[2], "-rf") == 0)
    {
        removeFile(argv[1], argv[3]);
    }
    else if (strcmp(argv[2], "-vf") == 0)
    {
        viewFiles(argv[1]);
    }
    else if (strcmp(argv[2], "-rv") == 0)
    {
        remove(argv[1]);
    }
    else if (strcmp(argv[2], "-vm") == 0)
    {
        viewMap(argv[1]);
    }

    return 0;
}