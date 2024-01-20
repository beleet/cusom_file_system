#include "userfs.h"
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>


enum {
    BLOCK_SIZE = 512,
    MAX_FILE_SIZE = 1024 * 1024 * 100,
};

/** Global error code. Set from any function on any error. */
static enum ufs_error_code ufs_error_code = UFS_ERR_NO_ERR;

struct block {
    /** Block memory. */
    char *memory;
    /** How many bytes are occupied. */
    int occupied;
    /** Next block in the file. */
    struct block *next;
    /** Previous block in the file. */
    struct block *prev;

    /* PUT HERE OTHER MEMBERS */
};

struct file {
    /** Double-linked list of file blocks. */
    struct block *block_list;
    /**
     * Last block in the list above for fast access to the end
     * of file.
     */
    struct block *last_block;
    /** How many file descriptors are opened on the file. */
    int refs;
    /** File name. */
    char *name;
    /** Files are stored in a double-linked list. */
    struct file *next;
    struct file *prev;

    /* PUT HERE OTHER MEMBERS */
};

/** List of all files. */
static struct file *file_list = NULL;

struct filedesc {
    struct file *file;
    size_t current_position;

    /* PUT HERE OTHER MEMBERS */
};

/**
 * An array of file descriptors. When a file descriptor is
 * created, its pointer drops here. When a file descriptor is
 * closed, its place in this array is set to NULL and can be
 * taken by next ufs_open() call.
 */
static struct filedesc **file_descriptors = NULL;
static int file_descriptor_count = 0;
static int file_descriptor_capacity = 0;

enum ufs_error_code
ufs_errno()
{
    return ufs_error_code;
}


struct file* search_file(const char* file_name) {
    // Searching file by the file name

    struct file* current_file = file_list;

    while (current_file != NULL) {
        if (current_file->name == file_name)
            return current_file;
        current_file = current_file->next;
    }

    return NULL;
}

int create_file_descriptor(struct file *file) {

    // Function for file descriptor creation
    file->refs++;

    struct filedesc *new_file_descriptor = (struct filedesc*) calloc(1, sizeof(struct filedesc));

    new_file_descriptor->file = file;
    new_file_descriptor->current_position = 0;

    //  Initialize the file_descriptors array if its empty
    if (file_descriptor_capacity == 0) {
        file_descriptor_capacity = 1;
        file_descriptors = (struct filedesc **) calloc(1, sizeof(struct filedesc*));
    }

    //  Looking for free cell for new created descriptor
    for (int i = 0; i < file_descriptor_capacity; i++) {
        if (file_descriptors[i] == NULL) {
            file_descriptors[i] = new_file_descriptor;
            return i;
        }
    }

    //  Logic that handles the case when array of descriptors is full
    file_descriptor_capacity *= 2;
    struct filedesc **extended_file_descriptors = (struct filedesc **) calloc(file_descriptor_capacity, sizeof(struct filedesc*));

    for (int i = 0; i < file_descriptor_capacity / 2; i++)
        extended_file_descriptors[i] = file_descriptors[i];

    free(file_descriptors);

    extended_file_descriptors[file_descriptor_capacity / 2] = new_file_descriptor;
    file_descriptors = extended_file_descriptors;

    return file_descriptor_capacity / 2;
}

struct file *create_file(const char *filename) {

    //  Function for file creation
    struct file *new_file = (struct file*) calloc(1, sizeof(struct file));

    //  Initialize fields
    new_file->name = (char *) filename;
    new_file->block_list = NULL;
    new_file->last_block = NULL;
    new_file->refs = 0;

    //  Append file into linked list of files
    if (file_list == NULL)
        file_list = new_file;

    else {
        new_file->next = file_list;
        file_list = new_file;
    }

    return new_file;
}

int
ufs_open(const char *filename, int flags)
{
    ufs_error_code = UFS_ERR_NO_ERR;

    struct file *desired_file = search_file(filename);

    if (flags == 0) {

        if (desired_file == NULL) {
            ufs_error_code = UFS_ERR_NO_FILE;
            return -1;
        }
    }

    if (flags == UFS_CREATE) {

        if (desired_file == NULL) {
            desired_file = create_file(filename);
        }
    }

    return create_file_descriptor(desired_file);;
}

ssize_t
ufs_write(int fd, const char *buf, size_t size)
{
    if (fd < 0 || fd >= file_descriptor_capacity) {
        ufs_error_code = UFS_ERR_NO_FILE;
        return -1;
    }

    if (file_descriptors[fd] == NULL) {
        ufs_error_code = UFS_ERR_NO_FILE;
        return -1;
    }

    struct filedesc *current_file_descriptor = file_descriptors[fd];
    struct file *current_file = current_file_descriptor->file;

    size_t remaining_size = size;
    size_t offset = current_file_descriptor->current_position;

//    current_file_descriptor->current_position = (current_file_descriptor->current_position + size) % BLOCK_SIZE;

    while (remaining_size > 0) {

        //  Check if file is empty
        if (current_file->block_list == NULL) {
            // TODO: проверить на максимальный размер файла

            struct block *new_block = (struct block*) calloc(1, sizeof(struct block));
            new_block->memory = (char *) calloc(BLOCK_SIZE, sizeof(char));

            current_file->block_list = new_block;
            current_file->block_list->next = NULL;
            current_file->last_block = current_file->block_list;
        }

        size_t write_size = (remaining_size > BLOCK_SIZE - offset) ? (BLOCK_SIZE - offset) : remaining_size;
//        printf("write size: %d, current position: %d\n", write_size, current_file_descriptor->current_position);
        memcpy(current_file->last_block->memory + offset, buf + size - remaining_size, write_size);

        remaining_size -= write_size;
        offset += write_size;

        if (offset == BLOCK_SIZE) {
            offset = 0;
            if (current_file->last_block->next == NULL) {
                current_file->last_block->next = (struct block*) calloc(1, sizeof(struct block));
                current_file->last_block->next->memory = (char *) calloc(BLOCK_SIZE, sizeof(char));
                current_file->last_block->next->next = NULL;
            }
            current_file->last_block = current_file->last_block->next;
        }
    }


//    current_file->last_block->occupied = (() + current_file->last_block->occupied) % BLOCK_SIZE;

    if (current_file->last_block->occupied < current_file_descriptor->current_position + size)
        current_file->last_block->occupied = (current_file_descriptor->current_position + size) % BLOCK_SIZE;


//    printf("\nbuffer: %s\n", buf);
//    printf("current position before: %d\n", current_file_descriptor->current_position);
    current_file_descriptor->current_position = (current_file_descriptor->current_position + size) % BLOCK_SIZE;
//    printf("current position after: %d\n", current_file_descriptor->current_position);
//    printf("occupied after: %d\n\n", current_file->last_block->occupied);


    return (ssize_t) size;
}

ssize_t
ufs_read(int fd, char *buf, size_t size)
{
//    TODO: прописать правильную логику, учитывая, что информация может лежать в разных блоках
    if (fd < 0 || fd >= file_descriptor_capacity) {
        ufs_error_code = UFS_ERR_NO_FILE;
        return -1;
    }

    if (file_descriptors[fd] == NULL) {
        ufs_error_code = UFS_ERR_NO_FILE;
        return -1;
    }

    struct filedesc *current_file_descriptor = file_descriptors[fd];
    struct file *current_file = current_file_descriptor->file;
    struct block *iterator = current_file->block_list;
    int full_blocks = 0;

    size_t remaining_size = size;
    size_t offset = current_file_descriptor->current_position;

//    printf("\noccupied: %d\n", current_file->last_block->occupied);
//    printf("before read: %d, ", current_file_descriptor->current_position);

    while (remaining_size > 0 && iterator != NULL) {
        // Сколько байт мы можем прочитать из текущего блока
        size_t read_size = (remaining_size > BLOCK_SIZE - offset) ? (BLOCK_SIZE - offset) : remaining_size;
        // Копируем данные из блока файла в buf
        memcpy(buf + size - remaining_size, iterator->memory + offset, read_size);

        // Обновляем оставшийся размер данных и смещение
        remaining_size -= read_size;
        offset += read_size;

        // Если текущий блок полностью прочитан, переходим к следующему блоку
        if (offset == BLOCK_SIZE) {
            offset = 0;
            iterator = iterator->next;
            full_blocks++;
        }
    }

    size_t prev_position = current_file_descriptor->current_position;

//    printf("\ncurrent position before: %d\n", current_file_descriptor->current_position);

    if (size < current_file->last_block->occupied - prev_position) {
        current_file_descriptor->current_position += size;
//        printf("\ncurrent position before: %d\n\n", current_file_descriptor->current_position);
        return (ssize_t) size;
    }

    current_file_descriptor->current_position = current_file->last_block->occupied;
//    printf("current position after: %d\n\n", current_file_descriptor->current_position);
    printf("full blocks: %d\n", full_blocks);
//    printf("\nbuffer: %s\n", buf);
//    printf("returns: %s\n\n", current_file->last_block->occupied - prev_position);

    return current_file->last_block->occupied - prev_position;

//    return (current_file_descriptor->current_position - prev_position == 0) ?
//        current_file->last_block->occupied - prev_position :
//        current_file_descriptor->current_position - prev_position;
}

int
ufs_close(int fd)
{
    if (fd < 0 || fd >= file_descriptor_capacity) {
        ufs_error_code = UFS_ERR_NO_FILE;
        return -1;
    }

    if (file_descriptors[fd] == NULL) {
        ufs_error_code = UFS_ERR_NO_FILE;
        return -1;
    }

    file_descriptors[fd]->file->refs--;
    file_descriptors[fd] = NULL;

    return 0;
}

int
ufs_delete(const char *filename)
{
    struct file *desired_file = search_file(filename);

    if (desired_file == NULL)
        return -1;

//    if (desired_file->refs != 0)
//        return -1;

    struct file *temp = file_list, *prev = NULL;

    while (temp != NULL && temp->name != filename) {
        prev = temp;
        temp = temp->next;
    }

    if (temp == NULL)
        return -1;

    if (prev == NULL)
        file_list = temp->next;
    else
        prev->next = temp->next;

    free(temp);

    return 0;
}

void
ufs_destroy(void)
{
}