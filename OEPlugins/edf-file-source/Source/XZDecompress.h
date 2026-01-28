/*
 * XZ Decompressor - Dynamic liblzma loading for reading .xz/.csv.xz files
 * 
 * This implementation dynamically loads liblzma.dll at runtime,
 * so it works even if the library is not available (graceful fallback).
 */

#ifndef XZ_DECOMPRESS_H
#define XZ_DECOMPRESS_H

#include <JuceHeader.h>
#include <vector>
#include <cstdint>

#ifdef _WIN32
#include <Windows.h>
#else
#include <dlfcn.h>
#endif

namespace XZDecompress {

// LZMA types and constants (from lzma.h)
typedef uint32_t lzma_ret;
typedef uint64_t lzma_vli;

#define LZMA_OK                 0
#define LZMA_STREAM_END         1
#define LZMA_NO_CHECK           2
#define LZMA_UNSUPPORTED_CHECK  3
#define LZMA_GET_CHECK          4
#define LZMA_MEM_ERROR          5
#define LZMA_MEMLIMIT_ERROR     6
#define LZMA_FORMAT_ERROR       7
#define LZMA_OPTIONS_ERROR      8
#define LZMA_DATA_ERROR         9
#define LZMA_BUF_ERROR          10
#define LZMA_PROG_ERROR         11

#define LZMA_CONCATENATED       0x08
#define LZMA_RUN                0
#define LZMA_FINISH             3

typedef struct {
    const uint8_t* next_in;
    size_t avail_in;
    uint64_t total_in;
    
    uint8_t* next_out;
    size_t avail_out;
    uint64_t total_out;
    
    void* allocator;
    void* internal;
    
    void* reserved_ptr1;
    void* reserved_ptr2;
    void* reserved_ptr3;
    void* reserved_ptr4;
    uint64_t reserved_int1;
    uint64_t reserved_int2;
    size_t reserved_int3;
    size_t reserved_int4;
    uint32_t reserved_enum1;
    uint32_t reserved_enum2;
} lzma_stream;

#define LZMA_STREAM_INIT { nullptr, 0, 0, nullptr, 0, 0, nullptr, nullptr, \
    nullptr, nullptr, nullptr, nullptr, 0, 0, 0, 0, 0, 0 }

// Function pointer types
typedef lzma_ret (*lzma_stream_decoder_fn)(lzma_stream*, uint64_t, uint32_t);
typedef lzma_ret (*lzma_code_fn)(lzma_stream*, uint32_t);
typedef void (*lzma_end_fn)(lzma_stream*);

/**
 * LZMA Library wrapper with dynamic loading
 */
class LZMALibrary {
public:
    static LZMALibrary& getInstance() {
        static LZMALibrary instance;
        return instance;
    }
    
    bool isAvailable() const { return available; }
    
    lzma_ret stream_decoder(lzma_stream* strm, uint64_t memlimit, uint32_t flags) {
        if (fn_stream_decoder)
            return fn_stream_decoder(strm, memlimit, flags);
        return LZMA_PROG_ERROR;
    }
    
    lzma_ret code(lzma_stream* strm, uint32_t action) {
        if (fn_code)
            return fn_code(strm, action);
        return LZMA_PROG_ERROR;
    }
    
    void end(lzma_stream* strm) {
        if (fn_end)
            fn_end(strm);
    }
    
private:
    LZMALibrary() : available(false), handle(nullptr) {
        loadLibrary();
    }
    
    ~LZMALibrary() {
        if (handle) {
#ifdef _WIN32
            FreeLibrary((HMODULE)handle);
#else
            dlclose(handle);
#endif
        }
    }
    
    void loadLibrary() {
        // Try to find liblzma.dll in various locations
        std::vector<String> searchPaths = {
            // Next to plugin DLL (recommended)
            File::getSpecialLocation(File::currentExecutableFile).getParentDirectory().getChildFile("liblzma.dll").getFullPathName(),
            // In plugins folder
            File::getSpecialLocation(File::currentExecutableFile).getParentDirectory().getChildFile("plugins/liblzma.dll").getFullPathName(),
            // System PATH
            "liblzma.dll",
            // Common installation locations
            "C:/Program Files/xz/bin/liblzma.dll",
            "C:/xz/bin/liblzma.dll"
        };
        
        for (const auto& path : searchPaths) {
#ifdef _WIN32
            handle = LoadLibraryA(path.toRawUTF8());
#else
            handle = dlopen(path.toRawUTF8(), RTLD_NOW);
#endif
            if (handle) {
                LOGC("XZ: Loaded liblzma from: ", path);
                break;
            }
        }
        
        if (!handle) {
            LOGC("XZ: liblzma.dll not found. XZ decompression disabled.");
            LOGC("XZ: Copy liblzma.dll to the plugins folder to enable .xz file support.");
            return;
        }
        
        // Load function pointers
#ifdef _WIN32
        fn_stream_decoder = (lzma_stream_decoder_fn)GetProcAddress((HMODULE)handle, "lzma_stream_decoder");
        fn_code = (lzma_code_fn)GetProcAddress((HMODULE)handle, "lzma_code");
        fn_end = (lzma_end_fn)GetProcAddress((HMODULE)handle, "lzma_end");
#else
        fn_stream_decoder = (lzma_stream_decoder_fn)dlsym(handle, "lzma_stream_decoder");
        fn_code = (lzma_code_fn)dlsym(handle, "lzma_code");
        fn_end = (lzma_end_fn)dlsym(handle, "lzma_end");
#endif
        
        available = (fn_stream_decoder && fn_code && fn_end);
        
        if (available) {
            LOGC("XZ: liblzma loaded successfully - XZ decompression enabled");
        } else {
            LOGE("XZ: Failed to load liblzma functions");
        }
    }
    
    bool available;
    void* handle;
    lzma_stream_decoder_fn fn_stream_decoder = nullptr;
    lzma_code_fn fn_code = nullptr;
    lzma_end_fn fn_end = nullptr;
};

/**
 * Check if a file is XZ compressed based on magic bytes
 */
inline bool isXZFile(const File& file)
{
    FileInputStream stream(file);
    if (!stream.openedOk())
        return false;
    
    // XZ magic bytes: FD 37 7A 58 5A 00
    uint8_t magic[6];
    if (stream.read(magic, 6) != 6)
        return false;
    
    return magic[0] == 0xFD && 
           magic[1] == 0x37 && 
           magic[2] == 0x7A && 
           magic[3] == 0x58 && 
           magic[4] == 0x5A && 
           magic[5] == 0x00;
}

/**
 * Check if file extension suggests XZ compression
 */
inline bool hasXZExtension(const File& file)
{
    String ext = file.getFileExtension().toLowerCase();
    return ext == ".xz" || file.getFullPathName().toLowerCase().endsWith(".csv.xz");
}

/**
 * Decompress XZ file to memory
 */
inline bool decompressXZ(const File& inputFile, MemoryBlock& outputData)
{
    LZMALibrary& lzma = LZMALibrary::getInstance();
    
    if (!lzma.isAvailable()) {
        LOGE("XZ: Cannot decompress - liblzma not available");
        return false;
    }
    
    FileInputStream stream(inputFile);
    if (!stream.openedOk()) {
        LOGE("XZ: Failed to open file: ", inputFile.getFullPathName());
        return false;
    }
    
    // Read entire compressed file into memory
    MemoryBlock compressedData;
    stream.readIntoMemoryBlock(compressedData);
    
    LOGC("XZ: Decompressing ", inputFile.getFileName(), " (", compressedData.getSize() / 1024, " KB compressed)");
    
    // Initialize decoder
    lzma_stream strm = LZMA_STREAM_INIT;
    lzma_ret ret = lzma.stream_decoder(&strm, UINT64_MAX, LZMA_CONCATENATED);
    
    if (ret != LZMA_OK) {
        LOGE("XZ: Failed to initialize decoder, error: ", (int)ret);
        return false;
    }
    
    // Setup input
    strm.next_in = (const uint8_t*)compressedData.getData();
    strm.avail_in = compressedData.getSize();
    
    // Decompress in chunks
    const size_t CHUNK_SIZE = 1024 * 1024; // 1MB chunks
    std::vector<uint8_t> outBuffer(CHUNK_SIZE);
    
    do {
        strm.next_out = outBuffer.data();
        strm.avail_out = outBuffer.size();
        
        ret = lzma.code(&strm, LZMA_FINISH);
        
        if (ret != LZMA_OK && ret != LZMA_STREAM_END) {
            LOGE("XZ: Decompression error: ", (int)ret);
            lzma.end(&strm);
            return false;
        }
        
        size_t have = outBuffer.size() - strm.avail_out;
        outputData.append(outBuffer.data(), have);
        
    } while (ret == LZMA_OK);
    
    lzma.end(&strm);
    
    LOGC("XZ: Decompressed to ", outputData.getSize() / 1024, " KB");
    return true;
}

/**
 * Read file contents, automatically decompressing if XZ compressed
 * Returns StringArray of lines
 */
inline bool readFileLines(const File& file, StringArray& lines)
{
    // Check if file is XZ compressed
    if (hasXZExtension(file) || isXZFile(file))
    {
        if (!LZMALibrary::getInstance().isAvailable()) {
            LOGE("XZ: Cannot read compressed file - liblzma.dll not found");
            LOGE("XZ: Please copy liblzma.dll to: ", 
                 File::getSpecialLocation(File::currentExecutableFile).getParentDirectory().getFullPathName());
            return false;
        }
        
        MemoryBlock decompressed;
        if (!decompressXZ(file, decompressed)) {
            return false;
        }
        
        // Parse decompressed data as text
        String content = String::fromUTF8((const char*)decompressed.getData(), (int)decompressed.getSize());
        lines.addLines(content);
        return lines.size() > 0;
    }
    
    // Regular file - read directly
    file.readLines(lines);
    return lines.size() > 0;
}

/**
 * Get decompressed file size (useful for progress indication)
 */
inline int64 getDecompressedSize(const File& file)
{
    if (!hasXZExtension(file) && !isXZFile(file))
        return file.getSize();
    
    // XZ files store uncompressed size in footer (last 12 bytes)
    // This is a simplified estimate - actual size may vary
    return file.getSize() * 5; // Typical compression ratio estimate
}

} // namespace XZDecompress

#endif // XZ_DECOMPRESS_H
