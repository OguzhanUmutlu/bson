#include <stdio.h>
#include "bson.h"
#include "utils.h"

int main() {
    FILE *file = fopen("data.bson", "w+b");
    if (!file) {
        perror("Failed to open file");
        return 1;
    }

    const char bytes[] = {0, 1};
    bson_t byt = bson_bytes(bytes);

    bson_print(&byt);

    const object_pair_t pairs[] = {
        {string("name"), bson_string("Alice")},
        {string("age"), bson_i32(20)},
        {string("is_student"), bson_true}
    };

    bson_t document = bson_object(pairs);

    bson_optimize(&document);

    if (bson_write(file, &document) != 0) {
        perror("Failed to write BSON data to file");
        fclose(file);
        return 1;
    }

    bson_print(&document);

    bson_free(&document);

    fseek_safe(file, 0, SEEK_SET, { return 1; });

    bson_t loaded = bson_read(file);

    if (loaded.type == BSON_INVALID) {
        perror("Failed to load BSON data from file");
        fclose(file);
        return 1;
    }

    bson_print(&loaded);

    bson_free(&loaded);

    fclose(file);

    return 0;
}
