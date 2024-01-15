#include "userfs.h"
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>

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

// Searching file by the file name
struct file* search_file(const char* file_name) {
    struct file* current_file = file_list;

    while (current_file != NULL) {
        if (current_file->name == file_name)
            return current_file;
        current_file = current_file->next;
    }

    return NULL;
}

int create_file_descriptor(struct file *file) {

    struct filedesc *new_file_descriptor = (struct filedesc*) calloc(1, sizeof(struct filedesc));

    new_file_descriptor->file = file;

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

    struct file *new_file = (struct file*) calloc(1, sizeof(struct file));
    new_file->name = (char *) filename;

    if (file_list == NULL)
        file_list = new_file;

    else {
        struct file *index_file = file_list;

        while (index_file->next != NULL)
            index_file = index_file->next;

        index_file = new_file;
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
    /* IMPLEMENT THIS FUNCTION */
    (void)fd;
    (void)buf;
    (void)size;
    ufs_error_code = UFS_ERR_NOT_IMPLEMENTED;
    return -1;
}

ssize_t
ufs_read(int fd, char *buf, size_t size)
{
    /* IMPLEMENT THIS FUNCTION */
    (void)fd;
    (void)buf;
    (void)size;
    ufs_error_code = UFS_ERR_NOT_IMPLEMENTED;
    return -1;
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

    file_descriptors[fd] = NULL;

    return 0;
}

int
ufs_delete(const char *filename)
{
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