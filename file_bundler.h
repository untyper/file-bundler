/* Github: https://github.com/untyper/file_bundler */

/*
  This is free and unencumbered software released into the public domain.

  Anyone is free to copy, modify, publish, use, compile, sell, or
  distribute this software, either in source code form or as a compiled
  binary, for any purpose, commercial or non-commercial, and by any
  means.

  In jurisdictions that recognize copyright laws, the author or authors
  of this software dedicate any and all copyright interest in the
  software to the public domain. We make this dedication for the benefit
  of the public at large and to the detriment of our heirs and
  successors. We intend this dedication to be an overt act of
  relinquishment in perpetuity of all present and future rights to this
  software under copyright law.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
  IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
  OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
  ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
  OTHER DEALINGS IN THE SOFTWARE.

  For more information, please refer to <http://unlicense.org/>
*/

#ifndef FILE_BUNDLER_H
#define FILE_BUNDLER_H

#include <vector>
#include <string>
#include <cstdint>
#include <fstream>
#include <filesystem>

namespace fs = std::filesystem;

namespace file_bundler
{

/* Implementation details. */
namespace /* file_bundler:: */ _
{

namespace STREAM_OBJECT_TYPE
{
  enum
  {
    MEMORY,
    VECTOR,
    FILE
  };
}

/* Basic stream helper for memory <-> file operations. */
class Base_Stream
{
  protected:
  int stream_object_type = 0;
  std::uint64_t current_offset = 0;

  /* Possible stream objects: */

  /* 1. File object */
  std::fstream file;
  std::string file_name;

  /* 2. Vector */
  std::vector<std::uint8_t>* vector = nullptr;

  /* 3. Raw memory buffer */
  std::uint8_t* memory = nullptr;
  std::uint64_t memory_size = 0;

  public:
  /* Returns empty string if not file stream object */
  std::string get_file_name()
  {
    return this->file_name;
  }

  void open(std::uint8_t* p_address, std::uint64_t p_size)
  {
    this->memory = p_address;
    this->memory_size = p_size;
    this->stream_object_type = STREAM_OBJECT_TYPE::MEMORY;
  }

  void open(std::vector<std::uint8_t>* p_vector, std::uint64_t p_size)
  {
    this->vector = p_vector;
    this->vector->reserve(p_size + sizeof(std::uint8_t)); // Initialize internal pointer even if p_size is zero.
    this->vector->resize(p_size);

    this->memory = this->vector->data();
    this->memory_size = this->vector->size();
    this->stream_object_type = STREAM_OBJECT_TYPE::VECTOR;
  }

  void open(const std::string& p_file_name, std::ios_base::openmode p_openmode)
  {
    this->file.open(p_file_name, p_openmode);
    this->file_name = p_file_name;
    this->stream_object_type = STREAM_OBJECT_TYPE::FILE;
  }

  Base_Stream(std::uint8_t* p_address, std::uint64_t p_size)
  {
    open(p_address, p_size);
  }

  Base_Stream(std::vector<std::uint8_t>* p_vector, std::uint64_t p_size)
  {
    open(p_vector, p_size);
  }

  Base_Stream(const std::string& p_file_name, std::ios_base::openmode p_openmode)
  {
    open(p_file_name, p_openmode);
  }

  Base_Stream() {}
};

class Input_Stream : public Base_Stream
{
  public:
  void read(std::uint8_t* p_address, std::uint64_t p_size)
  {
    if (this->memory != nullptr)
    {
      if ( (this->current_offset + p_size) >= this->memory_size )
      {
        /* Read chunk of the specified size (p_size) from the very end of the buffer. */
        //std::memcpy(p_address, this->memory + this->memory_size - p_size, p_size);
        return;
      }

      std::memcpy(p_address, this->memory + this->current_offset, p_size);
      this->current_offset += p_size;
      return;
    }

    this->file.read(reinterpret_cast<char*>(p_address), p_size);
  }

  void seekg(std::uint64_t p_offset)
  {
    if (this->memory != nullptr)
    {
      if (p_offset <= this->memory_size)
      {
        this->current_offset = p_offset;
      }

      return;
    }

    this->file.seekg(p_offset);
  }

  /* Inherit parent constructors */
  using Base_Stream::Base_Stream;
};

class Output_Stream : public Base_Stream
{
  private:
  /* total_bytes_written is distinct from current_offset.
   * The latter can be manipulated (with seekg) while the former is merely an incremental counter.
   */
  std::uint64_t total_bytes_written = 0;

  public:
  std::uint64_t get_total_bytes_written()
  {
    return this->total_bytes_written;
  }

  void write(std::uint8_t* p_address, std::uint64_t p_size)
  {
    if (this->memory != nullptr)
    {
      if (this->stream_object_type == STREAM_OBJECT_TYPE::VECTOR && ( (this->current_offset + p_size) > this->memory_size ) )
      {
        this->vector->resize(this->memory_size + p_size);
        this->memory_size = this->vector->size();
        this->memory = this->vector->data(); // Reallocation occurred, cache internal pointer.
      }

      std::memcpy(this->memory + this->current_offset, p_address, p_size);
      this->current_offset += p_size;
      this->total_bytes_written += p_size;
      return;
    }

    this->file.write(reinterpret_cast<char*>(p_address), p_size);
    this->total_bytes_written += p_size;
  }

  /* Inherit parent constructors */
  using Base_Stream::Base_Stream;
};

/* We use this metadata to parse and debundle our bundled files.
 * This way there is no need to use magic numbers to separate each section.
 * Adding offsets would make parsing easier but the trade off is a slight increase in size of the final bundle.
 */
struct Header_Metadata
{
  std::uint64_t names_section_size = 0;
  std::uint64_t sizes_section_size = 0;
  std::uint64_t files_section_size = 0;
};

} // namespace file_bundler::_

class File
{
  private:
  std::string name;
  std::uint64_t size = 0;
  std::vector<std::uint8_t> bytes;

  public:
  std::uint64_t& get_size()
  {
    return this->size;
  }

  void set_size(std::uint64_t p_file_size)
  {
    this->size = p_file_size;
  }

  std::string& get_name()
  {
    return this->name;
  }

  void set_name(const std::string& p_file_name)
  {
    this->name = p_file_name;
  }

  std::vector<std::uint8_t>& get_bytes()
  {
    return this->bytes;
  }

  void set_bytes(std::vector<std::uint8_t> p_bytes)
  {
    this->bytes = p_bytes;
  }

  /* Helper (overload) for easier transfer of bytes from memory block to member vector.
  * Just use this instead of bothering with memcpy and std::vector's methods.
  * NOTE: Only deallocate if memory block is on the heap.
  */
  void set_bytes(std::uint8_t* p_address, std::uint64_t p_size, bool p_deallocate = false)
  {
    this->size = p_size;
    this->bytes.resize(this->size);
    std::memcpy(this->bytes.data(), p_address, p_size);

    if (p_deallocate)
    {
      delete p_address;
    }
  }

  File(const std::string& p_file_name, std::uint8_t* p_address, std::uint64_t p_size, bool p_deallocate = false)
  {
    this->name = p_file_name;
    this->set_bytes(p_address, p_size, p_deallocate);
  }

  File(std::string p_file_name, std::vector<std::uint8_t> p_bytes)
  {
    this->name = p_file_name;
    this->size = p_bytes.size();
    this->bytes = p_bytes;
  }

  File(std::string p_file_name, std::uint64_t p_size)
  {
    this->name = p_file_name;
    this->size = p_size;
  }

  File() {}
};

/* Main bundler function. */
File bundle(_::Output_Stream& p_output_stream, const std::vector<File>& p_files, bool p_from_memory)
{
  _::Header_Metadata metadata;

  for (auto file : p_files)
  {
    metadata.names_section_size += file.get_name().size() + 1; /* +1 for null-terminator */
    metadata.sizes_section_size += sizeof(std::uint64_t);
    metadata.files_section_size += file.get_size();
  }

  /* Write metadata to bundle before anything else */
  p_output_stream.write(reinterpret_cast<std::uint8_t*>(&metadata), sizeof(_::Header_Metadata));

  for (auto file : p_files)
  {
    std::uint64_t file_name_size = file.get_name().size() + 1; /* +1 for null-terminator */
    p_output_stream.write(reinterpret_cast<std::uint8_t*>(const_cast<char*>(file.get_name().c_str())), file_name_size);
  }

  for (auto file : p_files)
  {
    p_output_stream.write(reinterpret_cast<std::uint8_t*>(&file.get_size()), sizeof(std::uint64_t));
  }

  /* Read in the individual files byte by byte */
  for (auto file : p_files)
  {
    if (p_from_memory)
    {
      p_output_stream.write(reinterpret_cast<std::uint8_t*>(file.get_bytes().data()), file.get_bytes().size());
    }
    else
    {
      _::Input_Stream input_stream(file.get_name(), std::ios::in | std::ios::binary);

      /* Could be more efficient but whatever... works fine with small to medium sized files. */
      for (int offset = 0; offset < file.get_size(); offset++)
      {
        char byte = CHAR_MAX;
        input_stream.read(reinterpret_cast<std::uint8_t*>(&byte), sizeof(char));
        p_output_stream.write(reinterpret_cast<std::uint8_t*>(&byte), sizeof(char));
      }
    }
  }

  return {p_output_stream.get_file_name(), p_output_stream.get_total_bytes_written()};
}

/* Bundle files from memory to disk. */
File bundle(const std::string& p_bundle_output_path, const std::vector<File>& p_files)
{
  _::Output_Stream output_stream(p_bundle_output_path, std::ios::out | std::ios::binary | std::ios::app);
  return bundle(output_stream, p_files, true);
}

/* Bundle files from disk to disk. */
File bundle(const std::string& p_bundle_output_path, const std::vector<std::string>& p_file_names)
{
  _::Output_Stream output_stream(p_bundle_output_path, std::ios::out | std::ios::binary | std::ios::app);
  std::vector<File> files;

  for (const auto& file_name : p_file_names)
  {
    files.push_back({file_name, fs::file_size(file_name)});
  }

  return bundle(output_stream, files, false);
}

/* Bundle files from memory to memory. */
File bundle(const std::vector<File>& p_files)
{
  std::vector<std::uint8_t> buffer;
  _::Output_Stream output_stream(&buffer, buffer.size());

  auto package = bundle(output_stream, p_files, true);
  package.get_bytes() = std::move(buffer);
  return package;
}

/* Bundle files from disk to memory. */
File bundle(const std::vector<std::string>& p_file_names)
{
  std::vector<std::uint8_t> buffer;
  _::Output_Stream output_stream(&buffer, buffer.size());
  std::vector<File> files;

  for (const auto& file_name : p_file_names)
  {
    files.push_back({file_name, fs::file_size(file_name)});
  }

  auto package = bundle(output_stream, files, false);
  package.get_bytes() = std::move(buffer);
  return package;
}

/* Main de-bundler function. */
std::vector<File> debundle(_::Input_Stream& p_input_stream, const std::string& p_output_directory, bool p_to_memory)
{
  std::vector<File> debundled_files;

  _::Header_Metadata metadata;
  auto metadata_size = sizeof(_::Header_Metadata);

  /* Read bundle header metadata first for info necessary for parsing */
  p_input_stream.read(reinterpret_cast<std::uint8_t*>(&metadata), metadata_size);

  /* Starting offset in bytes of each section from the beginning of the file. */
  std::uint64_t names_section_offset = metadata_size;
  std::uint64_t sizes_section_offset = names_section_offset + metadata.names_section_size;
  std::uint64_t files_section_offset = sizes_section_offset + metadata.names_section_size + metadata.sizes_section_size;

  std::uint64_t number_of_bundled_files = metadata.sizes_section_size / sizeof(std::uint64_t);

  /* These will be filled with the names sizes of each bundled file. */
  std::vector<std::string> names_of_bundled_files;
  std::vector<std::uint64_t> sizes_of_bundled_files;

  /* Reserve space for the array for efficiency (probably senseless),
   * since we already know the size of the array because the size of an integer is static unlike a string (file names).
   */
  sizes_of_bundled_files.reserve(number_of_bundled_files);

  p_input_stream.seekg(names_section_offset);

  /* Grab the file names from the header. */
  for (int i = 0; i < number_of_bundled_files; i++)
  {
    std::string file_name;
    char current_char = CHAR_MAX;

    while (current_char != '\0')
    {
      p_input_stream.read(reinterpret_cast<std::uint8_t*>(&current_char), sizeof(std::uint8_t));
      file_name += current_char;
    }

    names_of_bundled_files.push_back(file_name);
  }

  //bundle_file.seekg(sizes_section_offset);

  /* Grab the file sizes from the header. */
  for (int i = 0; i < number_of_bundled_files; i++)
  {
    std::uint64_t file_size = 0;
    p_input_stream.read(reinterpret_cast<std::uint8_t*>(&file_size), sizeof(std::uint64_t));

    sizes_of_bundled_files.push_back(file_size);
  }

  /* Now that we have the file names and sizes, we can prepare for extraction.
   * First check if file is supposed to be in a directory tree.
   */
  for (auto name : names_of_bundled_files)
  {
    if (!p_to_memory)
    {
      break;
    }

    std::string original_path;

    for (int i = name.size() - 1; i > 0; i--)
    {
      if (name[i] == '/' || name[i] == '\\')
      {
        original_path = name.substr(0, i);
        break;
      }
    }

    /* Create directory tree. */
    if (!original_path.empty())
    {
      std::string output_path = p_output_directory + '/' + original_path;
      fs::create_directories(output_path);
    }
  }

  //bundle_file.seekg(files_section_offset);

  /* Finally debundle files.
   * Extract files byte by byte.
   */
  for (int i = 0; i < names_of_bundled_files.size(); i++)
  {
    _::Output_Stream output_stream;

    auto file_name = names_of_bundled_files[i];
    auto file_size = sizes_of_bundled_files[i];
    debundled_files.push_back({file_name, file_size});

    if (p_to_memory)
    {
      auto& file_bytes = debundled_files[i].get_bytes();
      file_bytes.resize(file_bytes.size() + file_size);
      output_stream.open(file_bytes.data(), file_size);
    }
    else
    {
      output_stream.open(file_name, std::ios::out | std::ios::binary | std::ios::app);
    }

    /* Could be more efficient but whatever... works fine with small to medium sized files. */
    for (int offset = 0; offset < file_size; offset++)
    {
      char byte = CHAR_MAX;
      p_input_stream.read(reinterpret_cast<std::uint8_t*>(&byte), sizeof(char));
      output_stream.write(reinterpret_cast<std::uint8_t*>(&byte), sizeof(char));
    }
  }

  return debundled_files;
}

/* Debundle files from memory to disk.
 * Returns list of de-bundled files. 'bytes' property will be empty when de-bundled to disk.
 */
std::vector<File> debundle(std::uint8_t* p_bundle_address, std::uint64_t p_bundle_size, const std::string& p_output_directory)
{
  _::Input_Stream input_stream(p_bundle_address, p_bundle_size);
  return debundle(input_stream, p_output_directory, false);
}

/* Debundle files from disk to disk. */
std::vector<File> debundle(const std::string& p_bundle_path, const std::string& p_output_directory)
{
  _::Input_Stream input_stream(p_bundle_path, std::ios::in | std::ios::binary);
  return debundle(input_stream, p_output_directory, false);
}

/* Debundle files from memory to memory. */
std::vector<File> debundle(std::uint8_t* p_bundle_address, std::uint64_t p_bundle_size)
{
  _::Input_Stream input_stream(p_bundle_address, p_bundle_size);
  return debundle(input_stream, "", true);
}

/* Debundle files from disk to memory. */
std::vector<File> debundle(const std::string& p_bundle_path)
{
  _::Input_Stream input_stream(p_bundle_path, std::ios::in | std::ios::binary);
  return debundle(input_stream, "", true);
}

/* Debundle files to memory. */
std::vector<File> debundle(File& p_package)
{
  auto buffer_size = p_package.get_bytes().size();
  auto file_name = p_package.get_name();

  if (buffer_size > 0)
  {
    /* Debundle files from memory to memory. */
    return debundle(p_package.get_bytes().data(), buffer_size);
  }

  if (!file_name.empty())
  {
    /* Debundle files from disk to memory. */
    return debundle(file_name);
  }

  return {};
}

/* Debundle files to disk. */
std::vector<File> debundle(File& p_package, const std::string& p_output_directory)
{
  auto buffer_size = p_package.get_bytes().size();
  auto file_name = p_package.get_name();

  if (buffer_size > 0)
  {
    /* Debundle files from memory to disk. */
    return debundle(p_package.get_bytes().data(), buffer_size, p_output_directory);
  }

  if (!file_name.empty())
  {
    /* Debundle files from disk to disk. */
    return debundle(file_name, p_output_directory);
  }

  return {};
}

} // namespace file_bundler

#endif // FILE_BUNDLER_H