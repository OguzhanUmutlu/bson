# BSON - Binary JavaScript Object Notation

This is a modified version of BSON made in C. Optimized perfectly to work for files, in-memory and networking. Includes
developer-friendly functions for creating, printing, serializing, deserializing, reading and writing BSON objects. The
API is designed to be simple and intuitive, making it easy to work with BSON data structures. There are a few
differences from this and the official BSON. When serializing an array's elements' types are shoved in the beginning
instead of being in front of the elements which makes it a bit faster. Another difference is the fact that since arrays
and objects are dynamically sized they are both prefixed with their length and their size in bytes which makes indexing
faster.

# Data types

- `i8`: 8-bit signed integer
- `i16`: 16-bit signed integer
- `i32`: 32-bit signed integer
- `i64`: 64-bit signed integer
- `u8`: 8-bit unsigned integer
- `u16`: 16-bit unsigned integer
- `u32`: 32-bit unsigned integer
- `u64`: 64-bit unsigned integer
- `f32`: 32-bit floating point number
- `f64`: 64-bit floating point number
- `bool`: Boolean value (true or false)
- `string`: UTF-8 encoded string
- `bytes`: Raw byte array
- `date`: Date represented with unsigned 64-bit integer (milliseconds since epoch)
- `array`: Array of values, can be of any type
- `object`: Key-value pairs, similar to a JSON object
- `null`: Represents a null value

# Creating BSON Types

Here are some built-in constants you can use:

```c++
static const string_t empty_string_t;
static const array_t empty_array_t;
static const object_t empty_object_t;
static const bson_t empty_bson_string;
static const bson_t empty_bson_array;
static const bson_t empty_bson_object;
static const bson_t bson_null;
static const bson_t bson_true;
static const bson_t bson_false;
static const bson_t bson_invalid;
```

Here is how you can create primitive types like numbers, booleans and date:

```c++
bson_t my_u8  = bson_u8(42);
bson_t my_u16 = bson_u16(42424);
bson_t my_u32 = bson_u32(42424242);
bson_t my_u64 = bson_u64(4242424242424242);
bson_t my_i8  = bson_i8(-42);
bson_t my_i16 = bson_i16(-42424);
bson_t my_i32 = bson_i32(-42424242);
bson_t my_i64 = bson_i64(-4242424242424242);
bson_t my_f32 = bson_f32(3.14f);
bson_t my_f64 = bson_f64(3.141592653589793);

bson_t my_bool_true = bson_bool(true);
bson_t my_bool_false = bson_bool(false);

bson_t my_date = bson_date(1672531199000); // Example date in milliseconds since epoch
```

Here is how you can create dynamically sized types like strings, bytes, arrays, and objects:

```c++
bson_t my_string = bson_string("Hello, World!");
// Or if you want to use a heap-allocated string:
char* my_heap_string = strdup("Hello, World!");
bson_t my_string_heap = bson_string_heap(my_heap_string, strlen(my_heap_string));
// This logic of adding the length in the second argument and naming it *_heap is used for all dynamically sized types.

char raw_bytes[] = { 0x01, 0x02, 0x03, 0x04, 0x05 };
bson_t my_bytes = bson_bytes(raw_bytes);

bson_t raw_array[] = { my_u8, my_i32, my_string };
bson_t my_array = bson_array(raw_array);

object_pair_t pairs[] = { // NOTE: The keys must have the type `string_t` and not `bson_t`!
    {string("key1"), bson_string("value1")},
    {string("key2"), bson_i32(123)},
    {string("key3"), bson_bool(true)}
};
bson_t my_object = bson_object(raw_object);
```

# Serializing and Deserializing BSON

```c++
object_pair_t pairs[] = {
    {string("key1"), bson_string("value1")},
    {string("key2"), bson_i32(123)},
    {string("key3"), bson_bool(true)}
};
bson_t my_bson = bson_object(pairs); // Create a BSON object

uint8_t *buffer;
bson_serialize(&buffer, &my_bson); // Serialize the BSON object to a buffer

// Now you can write the buffer to a file or send it over the network

bson_t deserialized_bson;
bson_deserialize(&deserialized_bson, buffer); // Deserialize the buffer back to a BSON object

bson_print(&deserialized_bson); // Print the deserialized BSON object

// Don't forget to free the buffer after use
free(buffer);

// And also free the deserialized BSON object as it was heap-allocated
bson_free(&deserialized_bson);
```

# Reading and Writing BSON

```c++
object_pair_t pairs[] = {
    {string("key1"), bson_string("value1")},
    {string("key2"), bson_i32(123)},
    {string("key3"), bson_bool(true)}
};
bson_t my_bson = bson_object(pairs); // Create a BSON object

// Open the file you want to write to
FILE *file = fopen("data.bson", "wb");
if (!file) {
    perror("Failed to open file");
    return 1;
}

bson_write(file, &my_bson); // Write the BSON object to the file

fclose(file); // Close the file

// Now let's read the BSON object back from the file

file = fopen("data.bson", "rb");

if (!file) {
    perror("Failed to open file");
    return 1;
}

bson_t read_bson;
bson_read(file, &read_bson); // Read the BSON object from the file

bson_print(&read_bson); // Print the read BSON object

fclose(file); // Close the file

// Don't forget to free the heap-allocated BSON objects!
bson_free(&read_bson);
```
