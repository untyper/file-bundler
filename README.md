# file_bundler
Simple file bundler for C++ 17 (and up)

### Bundling examples
```c++
using fb = file_bundler;

/* Disk to disk */
fb::bundle("test_bundle", {"file1.txt", "file2.exe", "file3.zip"});

/* Disk to memory */
auto test_bundle = fb::bundle({"file1.txt", "file2.exe", "file3.zip"});

/* Memory to disk */
std::uint8_t file_1_bytes[] = {0x00 /* , ... */};
std::uint8_t file_2_bytes[] = {0x00 /* , ... */};
std::uint8_t file_3_bytes[] = {0x00 /* , ... */};

std::vector<fb::File> files =
{
  {"file1.txt", file_1_bytes, sizeof(file_1_bytes)},
  {"file2.exe", file_2_bytes, sizeof(file_2_bytes)},
  {"file3.zip", file_3_bytes, sizeof(file_3_bytes)}
};

fb::bundle("test_bundle", files);

/* Memory to memory */
auto test_bundle = fb::bundle(files);
```

### De-bundling examples
```c++
using fb = file_bundler;

/* Disk to disk */
fb::debundle("test_bundle", "output/debundled/files");

/* Disk to memory */
auto debundled_files = fb::debundle("test_bundle");

/* Memory to disk */
std::uint8_t test_bundle_bytes[]  = {0x00 /* , ... */};
fb::debundle(test_bundle_bytes, sizeof(test_bundle_bytes), "output/debundled/files");

/* Memory to memory */
auto debundled_files = fb::debundle(test_bundle_bytes, sizeof(test_bundle_bytes));
```

### Bundle file format

```
 _______________________
| Fixed size header     |
|_______________________|
| File paths            |
|_______________________|
| File sizes            |
|_______________________|
|                       |
|                       |
| File contents         |
|                       |
|_______________________|
```
