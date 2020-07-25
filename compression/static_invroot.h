#include <cstdlib>
#include <iostream>

typedef ushort Compf;

#define BLOCK_SIZE 64
#define MIN_VALUE 1e-17
#define RANGE 161
#define MAGIC 0x580000
#define OFFSET 1

class CompressedBlock {
    private:
        typedef union {     
            float f;
            uint32_t i;
        } float_int;    // for converting float into integer format
        CompressedBlock() { };

    public:
        static void set(float* __restrict__ data, Compf* __restrict__ p, int size);
        static void get(float* __restrict__ data, Compf* __restrict__ p, int size);

        static int countSizes(float* data, uint32_t* sizes, uint32_t* indexes, int n_blocks);
        static int countSizes(Compf* p, uint32_t* sizes, uint32_t* indexes, int n_blocks);
};

// Compresses given data block to p.
inline void CompressedBlock::set(float* __restrict__ data, Compf* __restrict__ p, int size) {
    
    // if block contains only zeroes, data pointer is NULL.
    if (size == 0) {
        *p = 0;
        return;
    }

    // if block contains very few zeroes, the whole block is stored.
    // otherwise the locations of zeroes are marked into 64-bit int.
    if (size >= BLOCK_SIZE - 4)
    {   

        Compf* temp = p + OFFSET;
        float_int value; 
        for (int i = 0; i < BLOCK_SIZE; i++) 
        {
            if (data[i] > MIN_VALUE) {
                value.f = data[i];
                value.i = MAGIC - ( value.i / RANGE );
                temp[i] = value.i >> 5;        
            } 
            else temp[i] = 0;
        }
    }
    else
    {
        // pointer to start of compressed data
        Compf* temp = p + OFFSET + sizeof(ulong) / sizeof(Compf);

        // bitmask for marking where values in the array are less than MIN_VALUE
        ulong &mask = *(ulong*) (p + OFFSET);
        mask = 0;

        float_int value; 
        for (int i = 0; i < BLOCK_SIZE; i++)
        {
            if (data[i] > MIN_VALUE) {
                mask |= (1UL << i);

                // compression of value with the fast inverse root method
                value.f = data[i];
                value.i = MAGIC - ( value.i / RANGE );
                *temp++ = value.i >> 5;
            }
        }
    }
    // saving number of values, RANGE and MAGIC number at start of the array
    *p = size;
}

inline void CompressedBlock::get(float* __restrict__ data, Compf* __restrict__ p, int size) {
    if (size == 0) {
        for (int i = 0; i < BLOCK_SIZE; i++) 
            data[i] = 0.f;
        return;
    }
    
    if (size >= BLOCK_SIZE - 4)
    {
        float_int value;
        Compf *temp = p + OFFSET;
        for (int i = 0; i < BLOCK_SIZE; i++) 
        {
            if (temp[i]) {
                value.i = 0xF + (temp[i] << 5);
                value.i = RANGE * (MAGIC - value.i);
                data[i] = value.f;
            }
            else data[i] = 0.f;
        }
    }
    else
    {
        Compf *temp = p + OFFSET + sizeof(ulong) / sizeof(Compf);
        const ulong mask = *(ulong*) (p + OFFSET);

        float_int value;
        for (int i = 0; i < BLOCK_SIZE; i++)
        {
            if ((mask >> i) & 1UL) {
                value.i = 0xF + (*temp++ << 5);
                value.i = RANGE * (MAGIC - value.i);
                data[i] = value.f;
            }
            else data[i] = 0.f;
        }
    }
}


inline int CompressedBlock::countSizes(float* data, uint32_t* sizes, uint32_t* indexes, int n_blocks) {
    int sum = 0;
    for (int i = 0; i < n_blocks; i++)
    {
        indexes[i] = sum;

        uint32_t n_values = 0;
        for (int j = 0; j < BLOCK_SIZE; j++)
            n_values += (data[j] > MIN_VALUE);
            
        sizes[i] = n_values;

        if (n_values == 0) 
            sum +=  1;
        else if (n_values >= BLOCK_SIZE - 4) 
            sum += BLOCK_SIZE + OFFSET;
        else 
            sum +=  n_values + OFFSET + sizeof(ulong) / sizeof(Compf);

        data += BLOCK_SIZE;
    }
    return sum;
}

inline int CompressedBlock::countSizes(Compf* p, uint32_t* sizes, uint32_t* indexes, int n_blocks) {
    int sum = 0;
    for (int i = 0; i < n_blocks; i++)
    {
        indexes[i] = sum;
        sizes[i] = p[sum];

        if (sizes[i] == 0) 
            sum +=  1;
        else if (sizes[i] >= BLOCK_SIZE - 4) 
            sum += BLOCK_SIZE + OFFSET;
        else 
            sum +=  sizes[i] + OFFSET + sizeof(ulong) / sizeof(Compf);
    }
    return sum;
}