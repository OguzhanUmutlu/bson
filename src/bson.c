#include "bson.h"

#include <string.h>

#include "utils.h"
#include "errno.h"

/**
 *
 * @param bson BSON object to cache the size of
 * @return Size of the BSON object in bytes
 */
size_t bson_optimize(bson_t *bson) { // NOLINT(*-no-recursion)
    size_t size = 8;
    switch (bson->type) {
        case BSON_I8:
        case BSON_U8:
            return 1;
        case BSON_I16:
        case BSON_U16:
            return 2;
        case BSON_I32:
        case BSON_U32:
        case BSON_F32:
            return 4;
        case BSON_I64:
        case BSON_U64:
        case BSON_F64:
        case BSON_DATE:
            return 8;
        case BSON_STRING:
        case BSON_BYTES:
            return 4 + bson->string.length;
        case BSON_ARRAY:
            if (bson->size != (1 << 25)) return bson->size;
            for (size_t i = 0; i < bson->array.length; i++) {
                size += 1 + bson_optimize(&bson->array.elements[i]);
            }
            bson->size = size;
            return size;
        case BSON_OBJECT:
            if (bson->size != (1 << 25)) return bson->size;
            for (size_t i = 0; i < bson->object.length; i++) {
                object_pair_t *pair = &bson->object.elements[i];
                size += 4 + pair->key.length;
                size += 1 + bson_optimize(&pair->value);
            }
            bson->size = size;
            return size;
        case BSON_NULL:
        case BSON_TRUE:
        case BSON_FALSE:
        case BSON_INVALID:
        case BSON_MAX:
            break;
    }
    return 0;
}

#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
#define buf_write_8o(index, val) buffer[index] = (val) & 0xFF
#define buf_write_16o(index, val) buf_write_8o(index, val); buf_write_8o(index + 1, (val) >> 8)
#define buf_write_32o(index, val) buf_write_16o(index, val); buf_write_16o(index + 2, (val) >> 16)
#define buf_write_64o(index, val) buf_write_32o(index, val); buf_write_32o(index + 4, (val) >> 32)
#define buf_read_u32o(buf, index) buf[index] | (buf[index + 1] << 8) | (buf[index + 2] << 16) | (buf[index + 3] << 24)
#elif defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
#define buf_write_8o(index, val) buffer[index] = (val) & 0xFF
#define buf_write_16o(index, val) buf_write_8o(index, (val) >> 8); buf_write_8o(index, val)
#define buf_write_32o(index, val) buf_write_16o(index, (val) >> 16); buf_write_16o(index, val)
#define buf_write_64o(index, val) buf_write_32o(index, (val) >> 32); buf_write_32o(index, val)
#define buf_read_u32o(buf, index) (buf[index] << 24) | (buf[index + 1] << 16) | (buf[index + 2] << 8) | buf[index + 3]
#else
#error "Cannot determine endianness"
#endif

#define buf_write_8(val) buf_write_8o(index, val); index++
#define buf_write_16(val) buf_write_16o(index, val); index += 2
#define buf_write_32(val) buf_write_32o(index, val); index += 4
#define buf_write_64(val) buf_write_64o(index, val); index += 8

/**
 *
 * @param buffer Buffer to write BSON data into
 * @param index Current index in the buffer
 * @param bson BSON value to write
 * @return Updated index in the buffer after writing
 */
size_t bson_write_iter(uint8_t *buffer, size_t index, const bson_t *bson) { // NOLINT(*-no-recursion)
    buf_write_8(bson->type);
    switch (bson->type) {
        case BSON_I8:
        case BSON_U8:
            buf_write_8(bson->u8);
            break;
        case BSON_I16:
        case BSON_U16:
            const uint16_t u16 = bson->u16;
            buf_write_16(u16);
            break;
        case BSON_I32:
        case BSON_U32:
            const uint32_t u32 = bson->u32;
            buf_write_32(u32);
            break;
        case BSON_I64:
        case BSON_U64:
        case BSON_DATE:
            const uint64_t u64 = bson->u64;
            buf_write_64(u64);
            break;
        case BSON_F32:
            union {
                float f;
                uint32_t u;
            } f32_union;
            f32_union.f = bson->f32;
            buf_write_32(f32_union.u);
            break;
        case BSON_F64:
            union {
                double d;
                uint64_t u;
            } f64_union;
            f64_union.d = bson->f64;
            buf_write_64(f64_union.u);
            break;
        case BSON_STRING:
        case BSON_BYTES:
            buf_write_32(bson->string.length);
            memcpy(&buffer[index], bson->string.data, bson->string.length);
            index += bson->string.length;
            break;
        case BSON_ARRAY:
            const array_t arr = bson->array;
            buf_write_32(arr.length);
            index += 4;
            const size_t array_start = index;
            for (int i = 0; i < arr.length; i++) {
                index = bson_write_iter(buffer, index, &arr.elements[i]);
            }
            const size_t array_size = index - array_start;
            buf_write_32o(array_start - 4, array_size);
            break;
        case BSON_OBJECT:
            const object_t obj = bson->object;
            buf_write_32(obj.length);
            index += 4;
            const size_t object_start = index;
            for (int i = 0; i < obj.length; i++) {
                const object_pair_t *pair = &obj.elements[i];
                const string_t *key = &pair->key;
                const size_t key_length = key->length;
                buf_write_32(key_length);
                memcpy(&buffer[index], key->data, key->length);
                index += key->length;
                index = bson_write_iter(buffer, index, &pair->value);
            }
            const size_t object_size = index - object_start;
            buf_write_32o(object_start - 4, object_size);
            break;
        case BSON_INVALID:
        case BSON_TRUE:
        case BSON_FALSE:
        case BSON_NULL:
        case BSON_MAX:
            break;
    }
    return index;
}

/**
 *
 * @param buffer Pointer to a buffer that will hold the serialized BSON data
 * @param bson BSON object to serialize
 * @return 0 on success, non-zero on failure
 */
int bson_serialize(uint8_t **buffer, bson_t *bson) {
    const size_t size = 1 + bson_optimize(bson);
    *buffer = malloc_safe(size, { return 1; });

    bson_write_iter(*buffer, 0, bson);

    return 0;
}

/**
 * @param file FILE pointer to write BSON data to
 * @param bson Optimized BSON object to write
 * @return 0 on success, non-zero on failure
 */
int bson_write(FILE *file, bson_t *bson) {
    uint8_t *buffer;
    bson_serialize(&buffer, bson);

    fwrite_safe(file, buffer, 1, 1 + bson->size, { free(buffer); return 1; });

    free(buffer);
    return 0;
}

/**
 * @param bson BSON object to free
 */
void bson_free(bson_t *bson) { // NOLINT(*-no-recursion)
    if (!bson) return;

    switch (bson->type) {
        case BSON_STRING:
        case BSON_BYTES:
            if (bson->string.alloc) free(bson->string.data);
            break;
        case BSON_ARRAY:
            if (!bson->object.elements) break;
            for (size_t i = 0; i < bson->array.length; i++) {
                bson_free(&bson->array.elements[i]);
            }
            if (bson->array.alloc) free(bson->array.elements);
            break;
        case BSON_OBJECT:
            if (!bson->object.elements) break;
            for (size_t i = 0; i < bson->object.length; i++) {
                bson_free(&bson->object.elements[i].value);
            }
            if (bson->object.alloc) free(bson->object.elements);
            break;
        case BSON_INVALID:
        case BSON_I8:
        case BSON_I16:
        case BSON_I32:
        case BSON_I64:
        case BSON_U8:
        case BSON_U16:
        case BSON_U32:
        case BSON_U64:
        case BSON_F32:
        case BSON_F64:
        case BSON_TRUE:
        case BSON_FALSE:
        case BSON_DATE:
        case BSON_NULL:
        case BSON_MAX:
            break;
    }
}

bson_t bson_read(FILE *file) { // NOLINT(*-no-recursion)
    uint8_t type;
    fread_safe(file, &type, sizeof(uint8_t), 1, { return bson_invalid; });
    return bson_read_typed(file, type);
}

#define obj_free_rest_temp() \
while (1) { \
    if (i == 0) break; \
    bson_free(&bson.object.elements[--i].value); \
} \
free(bson.object.elements); \
return bson_invalid

bson_t bson_read_typed(FILE *file, const uint8_t type) { // NOLINT(*-no-recursion)
    if (type >= BSON_MAX || type <= 0) {
        errno = EINVAL;
        return bson_invalid;
    }

    bson_t bson = {.type = type};

    uint32_t lens[2];
    switch ((bson_type) type) {
        case BSON_NULL:
        case BSON_INVALID:
        case BSON_TRUE:
        case BSON_FALSE:
        case BSON_MAX:
            break;
        case BSON_I8:
        case BSON_U8:
            fread_safe(file, &bson.u8, sizeof(uint8_t), 1, { return bson_invalid; });
            bson.size = 1;
            break;
        case BSON_I16:
        case BSON_U16:
            fread_safe(file, &bson.u16, sizeof(uint16_t), 1, { return bson_invalid; });
            bson.size = 2;
            break;
        case BSON_I32:
        case BSON_U32:
        case BSON_F32:
            fread_safe(file, &bson.u32, sizeof(uint32_t), 1, { return bson_invalid; });
            bson.size = 4;
            break;
        case BSON_I64:
        case BSON_U64:
        case BSON_F64:
        case BSON_DATE:
            fread_safe(file, &bson.u64, sizeof(uint64_t), 1, { return bson_invalid; });
            bson.size = 8;
            break;
        case BSON_STRING:
        case BSON_BYTES:
            uint32_t len;
            fread_safe(file, &len, sizeof(uint32_t), 1, { return bson_invalid; });
            if (len > (1 << 24)) {
                errno = EOVERFLOW;
                return bson_invalid;
            }
            bson.string.alloc = len != 0;
            bson.string.length = len;
            bson.size = 4 + len;
            bson.string.data = len ? malloc_safe(bson.string.length, { return bson_invalid; }) : NULL;
            if (len) {
                fread_safe(file, bson.string.data, 1, bson.string.length, { return bson_invalid; });
            }
            break;
        case BSON_ARRAY:
            fread_safe(file, &lens, sizeof(uint32_t), 2, { return bson_invalid; });
            if (lens[0] > (1 << 24) || lens[1] > (1 << 24)) {
                errno = EOVERFLOW;
                return bson_invalid;
            }
            bson.array.alloc = lens[0];
            bson.array.length = lens[0];
            bson.size = lens[1];
            bson.array.elements = lens[0] ? malloc_safe(lens[0] * sizeof(bson_t), { return bson_invalid; }) : NULL;
            for (size_t i = 0; i < lens[0]; i++) {
                const bson_t loaded = bson_read(file);
                if (loaded.type == BSON_INVALID) {
                    while (1) {
                        if (i == 0) break;
                        bson_free(&bson.array.elements[--i]);
                    }
                    free(bson.array.elements);
                    return bson_invalid;
                }
                // ReSharper thinks it might be a null pointer, but it's not because the loop wouldn't run if it was.
                // ReSharper disable once CppDFANullDereference
                bson.array.elements[i] = loaded;
            }
            break;
        case BSON_OBJECT:
            fread_safe(file, &lens, sizeof(uint32_t), 2, { return bson_invalid; });
            if (lens[0] > (1 << 24) || lens[1] > (1 << 24)) {
                errno = EOVERFLOW;
                return bson_invalid;
            }
            bson.object.alloc = lens[0];
            bson.object.length = lens[0];
            bson.size = lens[1];
            bson.object.elements = lens[0]
                                       ? malloc_safe(lens[0] * sizeof(object_pair_t), { return bson_invalid; })
                                       : NULL;

            for (size_t i = 0; i < lens[0]; i++) {
                object_pair_t pair;
                fread_safe(file, &pair.key.length, sizeof(uint32_t), 1, { return bson_invalid; });

                if (pair.key.length > (1 << 24)) {
                    errno = EOVERFLOW;
                    obj_free_rest_temp();
                }
                char *str = malloc_safe(pair.key.length, { obj_free_rest_temp(); });

                pair.key.data = str;
                fread_safe(file, str, 1, pair.key.length, { free(str); obj_free_rest_temp(); });
                const bson_t loaded = bson_read(file);
                if (loaded.type == BSON_INVALID) {
                    free(str);
                    obj_free_rest_temp();
                }
                pair.value = loaded;
                // ReSharper thinks it might be a null pointer, but it's not because the loop wouldn't run if it was.
                // ReSharper disable once CppDFANullDereference
                bson.object.elements[i] = pair;
            }
            break;
    }
    return bson;
}

/**
 *
 * @param buffer Pointer to a buffer that holds the serialized BSON data
 * @param index_ref Pointer to an index in the buffer that will be updated
 * @return Deserialized BSON object, or bson_invalid on error
 */
bson_t bson_deserialize(const uint8_t *buffer, uint32_t *index_ref) { // NOLINT(*-no-recursion)
    const uint8_t type = buffer[*index_ref++];
    if (type >= BSON_MAX || type <= 0) {
        errno = EINVAL;
        return bson_invalid;
    }

    return bson_deserialize_typed(buffer, index_ref, type);
}

/**
 *
 * @param buffer Pointer to a buffer that holds the serialized BSON data
 * @param index_ref Pointer to an index in the buffer that will be updated
 * @param type Type of BSON data to deserialize
 * @return Deserialized BSON object of the specified type, or bson_invalid on error
 */
bson_t bson_deserialize_typed(const uint8_t *buffer, uint32_t *index_ref, const uint8_t type) { // NOLINT(*-no-recursion)
    if (type >= BSON_MAX || type <= 0) {
        errno = EINVAL;
        return bson_invalid;
    }

    bson_t bson = {.type = type};

    uint32_t index = *index_ref;

    uint32_t len0, len1;
    switch ((bson_type) type) {
        case BSON_NULL:
        case BSON_INVALID:
        case BSON_TRUE:
        case BSON_FALSE:
        case BSON_MAX:
            break;
        case BSON_I8:
        case BSON_U8:
            bson.u8 = buffer[index];
            *index_ref++;
            bson.size = 1;
            break;
        case BSON_I16:
        case BSON_U16:
#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
            bson.u16 = buffer[index] | (buffer[index + 1] << 8);
#else
            bson.u16 = (buffer[index] << 8) | buffer[index + 1];
#endif
            *index_ref += 2;
            bson.size = 2;
            break;
        case BSON_I32:
        case BSON_U32:
        case BSON_F32:
            bson.u32 = buf_read_u32o(buffer, index);
            *index_ref += 4;
            bson.size = 4;
            break;
        case BSON_I64:
        case BSON_U64:
        case BSON_F64:
        case BSON_DATE:
#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
            bson.u64 = ((uint64_t) buffer[index] | ((uint64_t) buffer[index + 1] << 8) |
                        ((uint64_t) buffer[index + 2] << 16) | ((uint64_t) buffer[index + 3] << 24)) |
                       (((uint64_t) buffer[index + 4] | ((uint64_t) buffer[index + 5] << 8) |
                         ((uint64_t) buffer[index + 6] << 16) | ((uint64_t) buffer[index + 7] << 24)) << 32);
#else
            bson.u64 = (((uint64_t) buffer[index] << 56) | ((uint64_t) buffer[index + 1] << 48) |
                                 ((uint64_t) buffer[index + 2] << 40) | ((uint64_t) buffer[index + 3] << 32)) |
                                (((uint64_t) buffer[index + 4] << 24) | ((uint64_t) buffer[index + 5] << 16) |
                                 ((uint64_t) buffer[index + 6] << 8) | (uint64_t) buffer[index + 7]);
#endif
            *index_ref += 8;
            bson.size = 8;
            break;
        case BSON_STRING:
        case BSON_BYTES:
            uint32_t len = buf_read_u32o(buffer, index);
            if (len > (1 << 24)) {
                errno = EOVERFLOW;
                return bson_invalid;
            }
            bson.string.alloc = len != 0;
            bson.string.length = len;
            bson.size = 4 + len;
            bson.string.data = len ? malloc_safe(bson.string.length, { return bson_invalid; }) : NULL;
            if (len) {
                memcpy(bson.string.data, &buffer[index + 4], bson.string.length);
            }
            *index_ref += len + 4;
            break;
        case BSON_ARRAY:
            len0 = buf_read_u32o(buffer, index);
            len1 = buf_read_u32o(buffer, index + 4);
            if (len0 > (1 << 24) || len1 > (1 << 24)) {
                errno = EOVERFLOW;
                return bson_invalid;
            }
            bson.array.alloc = len0;
            bson.array.length = len0;
            bson.size = len1;
            bson.array.elements = len0 ? malloc_safe(len0 * sizeof(bson_t), { return bson_invalid; }) : NULL;
            *index_ref += 8;
            for (size_t i = 0; i < len0; i++) {
                const bson_t loaded = bson_deserialize(buffer, index_ref);
                if (loaded.type == BSON_INVALID) {
                    while (1) {
                        if (i == 0) break;
                        bson_free(&bson.array.elements[--i]);
                    }
                    free(bson.array.elements);
                    return bson_invalid;
                }
                // ReSharper thinks it might be a null pointer, but it's not because the loop wouldn't run if it was.
                // ReSharper disable once CppDFANullDereference
                bson.array.elements[i] = loaded;
            }
            break;
        case BSON_OBJECT:
            len0 = buf_read_u32o(buffer, index);
            len1 = buf_read_u32o(buffer, index + 4);
            if (len0 > (1 << 24) || len1 > (1 << 24)) {
                errno = EOVERFLOW;
                return bson_invalid;
            }
            bson.object.alloc = len0;
            bson.object.length = len0;
            bson.size = len1;
            bson.object.elements = len0 ? malloc_safe(len0 * sizeof(object_pair_t), { return bson_invalid; }) : NULL;
            *index_ref += 8;

            for (size_t i = 0; i < len0; i++) {
                object_pair_t pair;
                pair.key.length = buf_read_u32o(buffer, index);

                if (pair.key.length > (1 << 24)) {
                    errno = EOVERFLOW;
                    obj_free_rest_temp();
                }
                char *str = malloc_safe(pair.key.length, { obj_free_rest_temp(); });

                pair.key.data = str;
                memcpy(str, &buffer[index + 4], pair.key.length);
                *index_ref += 4 + pair.key.length;
                const bson_t loaded = bson_deserialize(buffer, index_ref);
                if (loaded.type == BSON_INVALID) {
                    free(str);
                    obj_free_rest_temp();
                }
                pair.value = loaded;
                // ReSharper thinks it might be a null pointer, but it's not because the loop wouldn't run if it was.
                // ReSharper disable once CppDFANullDereference
                bson.object.elements[i] = pair;
            }
            break;
    }
    return bson;
}

void bson_print_indent(const bson_t *bson, const int indent) { // NOLINT(*-no-recursion)
    if (bson == NULL) {
        printf("NULL");
        return;
    }

    switch (bson->type) {
        case BSON_I8:
            printf("%d", bson->i8);
            break;
        case BSON_I16:
            printf("%d", bson->i16);
            break;
        case BSON_I32:
            printf("%d", bson->i32);
            break;
        case BSON_I64:
            printf("%lld", bson->i64);
            break;
        case BSON_U8:
            printf("%u", bson->u8);
            break;
        case BSON_U16:
            printf("%u", bson->u16);
            break;
        case BSON_U32:
            printf("%u", bson->u32);
            break;
        case BSON_U64:
            printf("%llu", bson->u64);
            break;
        case BSON_F32:
            printf("%f", bson->f32);
            break;
        case BSON_F64:
            printf("%lf", bson->f64);
            break;
        case BSON_TRUE:
            printf("true");
            break;
        case BSON_FALSE:
            printf("false");
            break;
        case BSON_STRING:
            printf("\"%.*s\"", bson->string.length, bson->string.data);
            break;
        case BSON_BYTES:
            printf("<Buffer ", bson->string.length);
            for (size_t i = 0; i < bson->string.length; i++) {
                if (i > 0) printf(" ");
                printf("%02x", (unsigned char) bson->string.data[i]);
            }
            printf(">");
            break;
        case BSON_DATE:
            printf("date(%llu)", bson->u64);
            break;
        case BSON_ARRAY:
            printf("[\n");
            for (size_t i = 0; i < bson->array.length; i++) {
                for (int j = 0; j <= indent; j++) {
                    printf("  ");
                }
                bson_print_indent(&bson->array.elements[i], indent >= 0 ? indent + 1 : -1);
                if (i < bson->array.length - 1) {
                    printf(",");
                }
                if (indent != -1) printf("\n");
            }
            for (int j = 0; j < indent; j++) {
                printf("  ");
            }
            printf("]");
            break;
        case BSON_OBJECT:
            printf("{\n");
            for (size_t i = 0; i < bson->object.length; i++) {
                for (int j = 0; j <= indent; j++) {
                    printf("  ");
                }
                const object_pair_t *pair = &bson->object.elements[i];
                printf("\"%.*s\": ", pair->key.length, pair->key.data);
                bson_print_indent(&pair->value, indent >= 0 ? indent + 1 : -1);
                if (i < bson->object.length - 1) {
                    printf(",");
                }
                if (indent != -1) printf("\n");
            }
            for (int j = 0; j < indent; j++) {
                printf("  ");
            }
            printf("}");
            break;
        case BSON_NULL:
        case BSON_MAX:
        case BSON_INVALID:
            printf("null");
            break;
    }
}

void bson_print(const bson_t *bson) {
    bson_print_indent(bson, 0);
    printf("\n");
}
