#include <cstdlib>
#include <iostream>

typedef ushort Compf;

#define BLOCK_SIZE 64
#define MIN_VALUE 1e-17f    // minimum between 1e-17f and 1e-18f recommended
#define OFFSET 2

class CompressedBlock {
    private:
        typedef union {     
            float f;
            uint32_t i;
        } float_int;    // for converting float into integer format
        CompressedBlock() { };

    public:
        static void set(float* data, Compf* p, int size);
        static void get(float* data, Compf* p, int size);

        static int countSizes(float* data, uint32_t* sizes, uint32_t* indexes, int n_blocks);
        static int countSizes(Compf* p, uint32_t* sizes, uint32_t* indexes, int n_blocks);
};

// Compresses given data block to p.
inline void CompressedBlock::set(float* data, Compf* p, int size) {
    
    // if block contains only zeroes, data pointer is NULL.
    if (size == 0) {
        *p = 0;
        return;
    }

    // find largest and smallest values to compress
    float_int max, min;
    if (size == BLOCK_SIZE)
    {
        max.f = data[0];
        min.f = data[0];
        for (int i = 1; i < BLOCK_SIZE; i++){
            if (data[i] > max.f) max.f = data[i]; 
            else if (data[i] < min.f) min.f = data[i];
        }
    }
    else
    {
        max.f = data[0];
        min.f = MIN_VALUE;
        for (int i = 1; i < BLOCK_SIZE; i++){
            if (data[i] > max.f) max.f = data[i]; 
        }
    }

    uint range = (max.i - min.i + 0x3FFFFF) >> 21;
    uint magic = 0x3FFFC000 & ( min.i / range + 0x1FFFFF);

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
                value.i = magic - ( value.i / range );
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
                value.i = magic - ( value.i / range );
                *temp++ = value.i >> 5;
            }
        }
    }
    // saving number of values, range and magic number at start of the array
    *p = size + (range << 7);
    *(p + 1) = (magic >> 14);
}

inline void CompressedBlock::get(float *data, Compf *p, int size) {
    if (size == 0) {
        for (int i = 0; i < BLOCK_SIZE; i++) 
            data[i] = 0.f;
        return;
    }

    int range = *p >> 7;
    int magic = *(p+1) << 14;
    
    if (size >= BLOCK_SIZE - 4)
    {
        float_int value;
        Compf *temp = p + OFFSET;
        for (int i = 0; i < BLOCK_SIZE; i++) 
        {
            if (temp[i]) {
                value.i = 0xF + (temp[i] << 5);
                value.i = range * (magic - value.i);
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
                value.i = range * (magic - value.i);
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
        sizes[i] = (p[sum] & 0x7F);

        if (sizes[i] == 0) 
            sum +=  1;
        else if (sizes[i] >= BLOCK_SIZE - 4) 
            sum += BLOCK_SIZE + OFFSET;
        else 
            sum +=  sizes[i] + OFFSET + sizeof(ulong) / sizeof(Compf);
    }
    return sum;
}