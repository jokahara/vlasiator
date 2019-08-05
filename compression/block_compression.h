#include <cstdlib>
#include <cmath>

#define BLOCK_SIZE 64
#define MIN_VALUE 1E-16f

typedef union {     
    float f;
    struct parts
    {
        unsigned int man: 23, exp: 8, sign: 1;
    } parts;
} float_parts;

class CompressedBlock {
    private:
        unsigned char exp;
        ushort data[BLOCK_SIZE];

    public:
        CompressedBlock();
        CompressedBlock(const CompressedBlock&);

        void set(float* data);
        void get(float* array);
        const void get(float* array) const;
        void clear();

        //size_t compressedSize() const;

        CompressedBlock& operator=(const CompressedBlock& block) {
            exp = block.exp;
            if (exp != 0)
            {
                for (int i = 0; i < BLOCK_SIZE; i++)
                {
                    data[i] = block.data[i];
                }
            }
            return *this;
        }
};

inline CompressedBlock::CompressedBlock() {
    exp = 0;
}

inline CompressedBlock::CompressedBlock(const CompressedBlock& block) {
    exp = 0;
    *this = block;
}

// Compresses given data block of size 64
inline void CompressedBlock::set(float* array) {
    float max = array[0];

    for (int i = 1; i < BLOCK_SIZE; i++)
    {
        if (array[i] > max) max = array[i];
    }

    if (max < MIN_VALUE)
    {
        exp = 0;
        return;
    }

    float_parts fp = { .f = max };
    exp = fp.parts.exp;     // save exponent of the largest value
    
    for (int i = 0; i < BLOCK_SIZE; i++)
    {
        if (array[i] < MIN_VALUE) data[i] = 0;
        else
        {
            fp.f = array[i];
            fp.parts.exp = 142 + (fp.parts.exp - exp);
            data[i] = (ushort) (fp.f + .5f);
        }
    }
}

inline void CompressedBlock::get(float *array) {
    float_parts fp;
    
    if (exp == 0)
    {
        for (int i = 0; i < BLOCK_SIZE; i++) 
            array[i] = 0.f;
    }
    else for (int i = 0; i < BLOCK_SIZE; i++)
    {
        if (data[i] == 0) array[i] = 0.f;
        else
        {
            fp.f = (float) data[i];
            fp.parts.exp = exp - (142 - fp.parts.exp);
            array[i] = fp.f;
        }
    }
}

inline const void CompressedBlock::get(float *array) const {
    float_parts fp;
    
    if (exp == 0)
    {
        for (int i = 0; i < BLOCK_SIZE; i++) 
            array[i] = 0.f;
    }
    else for (int i = 0; i < BLOCK_SIZE; i++)
    {
        if (data[i] == 0) array[i] = 0.f;
        else
        {
            fp.f = (float) data[i];
            fp.parts.exp = exp - (142 - fp.parts.exp);
            array[i] = fp.f;
        }
    }
}

inline void CompressedBlock::clear() {
    exp = 0;
} 
