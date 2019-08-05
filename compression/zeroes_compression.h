#include <cstdlib>

#define BLOCK_SIZE 64
#define MIN_VALUE 9E-16f

typedef float Compf;

class CompressedBlock {
    private:
        ulong zeroes;                   // bit 1 marks points were value is less than MIN_VALUE aka zero.
        unsigned char compressed_size;  // number of compressed values in data array
        Compf *data;

    public:
        CompressedBlock();
        CompressedBlock(const CompressedBlock&);
        ~CompressedBlock() { if (data) free(data); };

        void set(float* data);
        void get(float* array) const;
        void clear();
        
        #define COMP_SIZE
        inline int compressedSize() const { return compressed_size * sizeof(Compf); }

        CompressedBlock& operator=(const CompressedBlock& block) {
            clear();
            
            if (!block.data) return *this;

            zeroes = block.zeroes;
            compressed_size = block.compressed_size;
            data = (Compf*) malloc(compressedSize());
            for (unsigned int i = 0; i < compressed_size; i++)
            {
                data[i] = block.data[i];
            }

            return *this;
        }
};

inline CompressedBlock::CompressedBlock() {
    zeroes = 0;
    compressed_size = 0;
    data = NULL;
}

inline CompressedBlock::CompressedBlock(const CompressedBlock& block) {
    zeroes = block.zeroes;
    compressed_size = block.compressed_size;
    data = NULL;
            
    if (!block.data) return;

    data = (Compf*) malloc(compressedSize());
    for (unsigned int i = 0; i < compressed_size; i++)
    {
        data[i] = block.data[i];
    }
}

// Compresses given data block of size 64
inline void CompressedBlock::set(float* array) {
    clear();
    
    int n_zeroes = 0;
    data = (Compf*) malloc(sizeof(Compf) * BLOCK_SIZE);
    Compf* temp = data;
    for (int i = 0; i < BLOCK_SIZE; i++)
    {
        if (array[i] < MIN_VALUE) {
            n_zeroes++;
            zeroes |= (1UL << i);
        }
        else *temp++ = array[i];
    }

    // if block contains only zeroes, data pointer is NULL;
    if (n_zeroes == BLOCK_SIZE) 
    {
        free(data);
        data = NULL;
    }
    else
    {
        compressed_size = BLOCK_SIZE - n_zeroes;
        data = (Compf*) realloc(data, compressedSize());
    }
}

inline void CompressedBlock::get(float *array) const {
    if (!data)
    {
        for (int i = 0; i < BLOCK_SIZE; i++) 
            array[i] = 0.f;
    }
    else
    {
        Compf *temp = data;

        for (int i = 0; i < BLOCK_SIZE; i++)
        {
            if ((zeroes >> i) & 1) {
                array[i] = 0.f;
            }
            else array[i] = *temp++;
        }
    }
}

inline void CompressedBlock::clear() {
    if (data) free(data);
    data = NULL;
    zeroes = 0;
    compressed_size = 0;
}
/*#include <cstdlib>
#include <cmath>

#include "invroot_compression.h"
typedef CompressedValue Compf;

#define BLOCK_SIZE 64
#define MIN_VALUE 9E-16f

class CompressedBlock {
    private:
        Compf *data;
        
    public:
        CompressedBlock();
        CompressedBlock(const CompressedBlock&);
        ~CompressedBlock() { if (data) free(data); };

        void set(float* data);
        void get(float* array) const;
        void clear();
        
        #define COMP_SIZE
        size_t compressedSize() const;

        inline CompressedBlock& operator=(const CompressedBlock& block) {
            clear();
            
            if (!block.data) return *this;
            
            size_t size = block.compressedSize();
            data = (Compf*) malloc(size);
            for (unsigned int i = 0; i < size / sizeof(Compf); i++)
            {
                data[i] = block.data[i];
            }

            return *this;
        }
};

inline CompressedBlock::CompressedBlock() {
    data = NULL;
}

inline CompressedBlock::CompressedBlock(const CompressedBlock& block) {
    data = NULL;
    
    if (!block.data) return;

    size_t size = block.compressedSize();
    data = (Compf*) malloc(size);
    for (unsigned int i = 0; i < size / sizeof(Compf); i++)
    {
        data[i] = block.data[i];
    }
}

// Compresses given data block of size 64
inline void CompressedBlock::set(float* array) {
    clear();

    ushort n_values = 0;
    for (int i = 0; i < BLOCK_SIZE; i++)
    {
        if (array[i] > MIN_VALUE) n_values++;
    }
    // if block contains only zeroes, data pointer is NULL;
    if (!n_values) return;
    
    // if block contains very few zeroes, the whole block is stored.
    // otherwise the locations of zeroes are marked into 64-bit int.
    if (n_values >= BLOCK_SIZE - 4)
    {
        data = (Compf*) malloc((BLOCK_SIZE + 1) * sizeof(Compf));
        *(ushort*) data = n_values;

        for (int i = 0; i < BLOCK_SIZE; i++) 
            data[i+1] = array[i];        
    }
    else
    {
        data = (Compf*) malloc((n_values + 1) * sizeof(Compf) + sizeof(long));
        *(ushort*) data = n_values;
        ulong &zeroes = *(ulong*) (data + 1);
        Compf* temp = data + 1 + sizeof(ulong) / sizeof(Compf);

        zeroes = 0;
        for (int i = 0; i < BLOCK_SIZE; i++)
        {
            if (array[i] > MIN_VALUE) {
                zeroes |= (1UL << i);   // bit 1 marks points were value is more than MIN_VALUE.
                *temp++ = array[i];
            }
        }
    }
}

inline void CompressedBlock::get(float *array) const {
    if (!data)
    {
        for (int i = 0; i < BLOCK_SIZE; i++) 
            array[i] = 0.f;
    }
    else if (*(ushort*) data >= BLOCK_SIZE - 4)
    {
        for (int i = 0; i < BLOCK_SIZE; i++) 
            array[i] = data[i+1];        
    }
    else
    {
        Compf *temp = data + 1 + sizeof(ulong) / sizeof(Compf);
        const ulong zeroes = *(ulong*) (data + 1);

        for (int i = 0; i < BLOCK_SIZE; i++)
        {
            if ((zeroes >> i) & 1UL) {
                array[i] = *temp++;
            }
            else array[i] = 0.f;
        }
    }
}

inline void CompressedBlock::clear() {
    if (data) free(data);
    data = NULL;
}

inline size_t CompressedBlock::compressedSize() const { 
    if(!data) return 0;

    ushort n_values = *(ushort*) data;
    return (n_values + 1) * sizeof(Compf) + sizeof(long); 
} */