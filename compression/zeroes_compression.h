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
    data = NULL;
    *this = block;
}

// Compresses given data block of size 64
inline void CompressedBlock::set(float* array) {
    clear();
    
    int n_zeroes = 0;
    for (int i = 0; i < BLOCK_SIZE; i++)
    {
        if (array[i] < MIN_VALUE) n_zeroes++;
    }

    // if block contains only zeroes, data pointer is NULL;
    if (n_zeroes == BLOCK_SIZE) return;

    else
    {
        compressed_size = BLOCK_SIZE - n_zeroes;
        data = (Compf*) malloc(compressedSize());

        Compf* temp = data;
        for (int i = 0; i < BLOCK_SIZE; i++)
        {
            if (array[i] < MIN_VALUE) zeroes |= (1UL << i);
            else *temp++ = array[i];
        }
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
