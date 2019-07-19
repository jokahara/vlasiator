#include <cstdlib>

#define BLOCK_SIZE 64
#define MIN_VALUE 9E-16f

typedef union {     
    float f;
    struct parts
    {
        unsigned int man: 23, exp: 8, sign: 1;
    } parts;
} float_parts;

class CompressedBlock {
    private:
        ulong zeroes;                   // bit 1 marks points were value is less than MIN_VALUE aka zero.
        unsigned char compressed_size;  // number of compressed values in data array
        unsigned char exp;
        ushort *data;

    public:
        CompressedBlock();
        CompressedBlock(const CompressedBlock&);
        ~CompressedBlock() { if (data) free(data); };

        void set(float* data);
        void get(float* array) const;
        void clear();
        
        #define COMP_SIZE
        inline int compressedSize() const { return compressed_size * sizeof(ushort); }

        CompressedBlock& operator=(const CompressedBlock& block) {
            clear();
            
            if (!block.data) return *this;

            exp = block.exp;
            zeroes = block.zeroes;
            compressed_size = block.compressed_size;
            data = (ushort*) malloc(compressedSize());
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
    exp = 0;
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
    float max = 0;

    for (int i = 0; i < BLOCK_SIZE; i++)
    {
        if (array[i] < MIN_VALUE) n_zeroes++;
        else if (array[i] > max) max = array[i];
    }

    // if block contains only zeroes, data pointer is NULL;
    if (n_zeroes == BLOCK_SIZE) return;

    float_parts fp = { .f = max };
    exp = fp.parts.exp;     // save exponent of the largest value

    compressed_size = BLOCK_SIZE - n_zeroes;
    data = (ushort*) malloc(compressedSize());

    ushort* temp = data;
    for (int i = 0; i < BLOCK_SIZE; i++)
    {
        if (array[i] < MIN_VALUE) zeroes |= (1UL << i);
        else
        {
            fp.f = array[i];
            fp.parts.exp = 142 + (fp.parts.exp - exp);
            *temp++ = (ushort) (fp.f + .5f);
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
        ushort *temp = data;
        float_parts fp;

        for (int i = 0; i < BLOCK_SIZE; i++)
        {
            if ((zeroes >> i) & 1) {
                array[i] = 0.f;
            }
            else if(*temp)
            {
                fp.f = (float) *temp++;
                fp.parts.exp = exp - (142 - fp.parts.exp);
                array[i] = fp.f;
            }
            else {
                array[i] = 0.f;
                temp++;
            }
        }
    }
}

inline void CompressedBlock::clear() {
    if (data) free(data);
    data = NULL;
    zeroes = 0;
    compressed_size = 0;
    exp = 0;
}
