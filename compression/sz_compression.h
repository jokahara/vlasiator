#include "sz.h"
#include "rw.h"
#include <cstdlib>

#define BLOCK_SIZE 64

class CompressedBlock {

    private:
        unsigned char* buffer;
        unsigned short size;
        
    public:
        CompressedBlock();
        ~CompressedBlock();
        CompressedBlock(const CompressedBlock&);

        uint32_t set(float* data);
        uint32_t get(float* array);
        const uint32_t get(float* array) const;
        void clear();

        size_t compressedSize() const;

        CompressedBlock& operator=(const CompressedBlock& block) {
            clear();
            size = block.size;
            if (block.buffer)
            {
                buffer = (unsigned char*) malloc(size);
                for (uint i = 0; i < size; i++)
                {
                    buffer[i] = block.buffer[i];
                }
            }
            return *this;
        }
};

inline CompressedBlock::CompressedBlock() {
    buffer = NULL;
    size = 0;
}

// use of this not recommended because it is slow
inline CompressedBlock::CompressedBlock(const CompressedBlock& block) {
    buffer = NULL;
    size = 0;
    *this = block;
}

// Compresses given data block of size 64
// returns the compressed size (or zero if unsuccesful)
inline uint32_t CompressedBlock::set(float* data) {
    clear();

    size_t size_t;
    buffer = SZ_compress(SZ_FLOAT, data, &size_t, 0,0,0,0,64);

    size = size_t;
    return size;
}

inline uint32_t CompressedBlock::get(float *array) {
    if (buffer) {
        float* data = (float*) SZ_decompress(SZ_FLOAT, buffer, size, 0,0,4,4,4);
        for (int i = 0; i < BLOCK_SIZE; i++) array[i] = data[i];
        free(data);
    }
    else
    {
        for (int i = 0; i < BLOCK_SIZE; i++) array[i] = 0.f;
    }
    return size;
}

inline const uint32_t CompressedBlock::get(float *array) const {
    if (buffer) {
        float* data = (float*) SZ_decompress(SZ_FLOAT, buffer, size, 0,0,4,4,4);
        for (int i = 0; i < BLOCK_SIZE; i++) array[i] = data[i];
        free(data);
    }
    else
    {
        for (int i = 0; i < BLOCK_SIZE; i++) array[i] = 0.f;
    }
    return size;
}

inline void CompressedBlock::clear() {
    if(buffer) {
        free(buffer);
        size = 0;
        buffer = NULL;
    }
}

inline size_t CompressedBlock::compressedSize() const {
    return size;
}

inline CompressedBlock::~CompressedBlock() {
    if(buffer) {
        free(buffer);
    }
}
