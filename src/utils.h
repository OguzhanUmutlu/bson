#ifndef UTILS_H
#define UTILS_H

#define exit_switch

#define fseek_safe(file, offset, whence, exit) \
    ({ \
        int result = fseek((file), (offset), (whence)); \
        if (result != 0) { \
            perror("fseek failed"); \
            exit_switch; \
            exit; \
        } \
        result; \
    })

#define fread_safe(file, ptr, size, count, exit) \
    ({ \
        size_t result = fread((ptr), (size), (count), (file)); \
        if (result != (count)) { \
            perror("fread failed"); \
            exit_switch; \
            exit; \
        } \
        result; \
    })

#define fwrite_safe(file, ptr, size, count, exit) \
    ({ \
        size_t result = fwrite((ptr), (size), (count), (file)); \
        if (result != (count)) { \
            perror("fwrite failed"); \
            exit_switch; \
            exit; \
        } \
        result; \
    })

#define null_check(ptr, err, exit) \
    ({ \
        if ((ptr) == NULL) { \
            perror(err); \
            exit_switch; \
            exit; \
        } \
        ptr; \
    })

#define malloc_safe(size, exit) \
    ({ \
        void *ptr = malloc((size)); \
        if (!ptr) { \
            perror("Memory allocation failed"); \
            exit_switch; \
            exit; \
        } \
        ptr; \
    })

#endif
