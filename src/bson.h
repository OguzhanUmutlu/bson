#ifndef BSON_H
#define BSON_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
    BSON_INVALID = 0, // reserved for null byte
    BSON_I8,
    BSON_I16,
    BSON_I32,
    BSON_I64,
    BSON_U8,
    BSON_U16,
    BSON_U32,
    BSON_U64,
    BSON_F32,
    BSON_F64,
    BSON_TRUE,
    BSON_FALSE,
    BSON_STRING,
    BSON_BYTES,
    BSON_DATE,
    BSON_ARRAY,
    BSON_OBJECT,
    BSON_NULL,

    BSON_MAX
} bson_type;

typedef struct bson_t bson_t;
typedef struct object_pair_t object_pair_t;

typedef struct {
    char *data;
    uint32_t length;
    uint8_t alloc; // 0 for stack, otherwise heap allocated
} string_t;

typedef struct {
    bson_t *elements;
    uint32_t length;
    uint8_t alloc; // 0 for stack, otherwise heap allocated
} array_t;

typedef struct {
    object_pair_t *elements;
    uint32_t length;
    uint8_t alloc; // 0 for stack, otherwise heap allocated
} object_t;

// todo: handle padding manually just in case for old systems? (with static_assert() and offsetof())
struct bson_t {
    bson_type type;
    size_t size;

    union {
        uint8_t u8;
        uint16_t u16;
        uint32_t u32;
        uint64_t u64;
        int8_t i8;
        int16_t i16;
        int32_t i32;
        int64_t i64;
        float f32;
        double f64;

        string_t string;
        array_t array;
        object_t object;
    };
};

struct object_pair_t {
    string_t key;
    bson_t value;
};

static const string_t empty_string_t = {.data = NULL, .alloc = 0, .length = 0};
static const array_t empty_array_t = {.elements = NULL, .alloc = 0, .length = 0};
static const object_t empty_object_t = {.elements = NULL, .alloc = 0, .length = 0};

#define string(str) ((string_t){.data = (char *)(str), .length = sizeof(str) - 1, .alloc = 0})
#define bytes(str) ((string_t){.data = (char *)(str), .length = sizeof(str), .alloc = 0})
#define string_heap(str, len) ((string_t){.data = (char *)(str), .length = (len), .alloc = 1})
#define array(data) ((array_t){.elements = (bson_t *)(data), .length = sizeof(data) / sizeof(bson_t), .alloc = 0})
#define array_heap(data, len) ((array_t){.elements = (bson_t *) (data), .length = (len), .alloc = 1})
#define object(data) ((object_t){.elements = (object_pair_t *) (data), .length = sizeof(data) / sizeof(object_pair_t), .alloc = 0})
#define object_heap(data, len) ((object_t){.elements = (object_pair_t *)(k), .length = (len), .alloc = 1})

#define bson_u8(value) ((bson_t){.type = BSON_U8, .size = 1, .u8 = (value)})
#define bson_u16(value) ((bson_t){.type = BSON_U16, .size = 2, .u16 = (value)})
#define bson_u32(value) ((bson_t){.type = BSON_U32, .size = 4, .u32 = (value)})
#define bson_u64(value) ((bson_t){.type = BSON_U64, .size = 8, .u64 = (value)})
#define bson_i8(value) ((bson_t){.type = BSON_I8, .size = 1, .i8 = (value)})
#define bson_i16(value) ((bson_t){.type = BSON_I16, .size = 2, .i16 = (value)})
#define bson_i32(value) ((bson_t){.type = BSON_I32, .size = 4, .i32 = (value)})
#define bson_i64(value) ((bson_t){.type = BSON_I64, .size = 8, .i64 = (value)})
#define bson_f32(value) ((bson_t){.type = BSON_F32, .size = 4, .f32 = (value)})
#define bson_f64(value) ((bson_t){.type = BSON_F64, .size = 8, .f64 = (value)})
#define bson_date(value) ((bson_t){.type = BSON_DATE, .size = 8, .u64 = (value)})
#define bson_string(str) ((bson_t){.type = BSON_STRING, .size = 4 + sizeof(str) - 1, .string = string(str)})
#define bson_string_heap(str, len) ((bson_t){.type = BSON_STRING, .size = 4 + len, .string = string_heap(str, len)})
#define bson_bytes(str) ((bson_t){.type = BSON_BYTES, .size = 4 + sizeof(str), .string = bytes(str)})
#define bson_bytes_heap(str, len) ((bson_t){.type = BSON_BYTES, .size = 4 + len, .string = string_heap(str, len)})
#define bson_array(data) ((bson_t){.type = BSON_ARRAY, .size = (1 << 25), .array = array(data)})
#define bson_array_heap(data, len) ((bson_t){.type = BSON_ARRAY, .size = (1 << 25), .array = array_heap(data, len)})
#define bson_object(data) ((bson_t){.type = BSON_OBJECT, .size = (1 << 25), .object = object(data)})
#define bson_object_heap(data, len) ((bson_t){.type = BSON_OBJECT, .size = (1 << 25), .object = object_heap(data, len)})
#define bson_bool(value) ((bson_t){.type = (value) ? BSON_TRUE : BSON_FALSE, .size = 1})

static const bson_t empty_bson_string = {.type = BSON_STRING, .size = 4, .string = empty_string_t};
static const bson_t empty_bson_array = {.type = BSON_ARRAY, .size = 8, .array = empty_array_t};
static const bson_t empty_bson_object = {.type = BSON_OBJECT, .size = 8, .object = empty_object_t};
static const bson_t bson_null = {.type = BSON_NULL, .size = 1};
static const bson_t bson_true = {.type = BSON_TRUE, .size = 1};
static const bson_t bson_false = {.type = BSON_FALSE, .size = 1};

static const bson_t bson_invalid = {.type = BSON_INVALID};

void bson_free(bson_t *bson);

size_t bson_optimize(bson_t *bson);

int bson_serialize(uint8_t **buffer, bson_t *bson);

bson_t bson_deserialize(const uint8_t *buffer, uint32_t *index_ref);

bson_t bson_deserialize_typed(const uint8_t *buffer, uint32_t *index_ref, const uint8_t type);

int bson_write(FILE *file, bson_t *bson);

size_t bson_write_iter(uint8_t *buffer, const size_t index, const bson_t *bson);

size_t bson_write_iter_typed(uint8_t *buffer, size_t index, const bson_t *bson);

bson_t bson_read(FILE *file);

bson_t bson_read_typed(FILE *file, const uint8_t type);

void bson_print_indent(const bson_t *bson, const int indent);

void bson_print(const bson_t *bson);

#endif
