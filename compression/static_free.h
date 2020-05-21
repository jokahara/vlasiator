#include <cstdlib>
#include <iostream>

typedef ushort Compf;

#define BLOCK_SIZE 64
#define MIN_VALUE 1e-17f    // minimum between 1e-17f and 1e-18f recommended
#define OFFSET 2
#define DEFAULT_COMP_FACTOR 26

class CompressedBlock {
    private:
        typedef union {     
            float f;
            uint32_t i;
        } float_int;    // for converting float into integer format
        CompressedBlock() { };

    public:
        static void set(float* data, Compf* p, int size, float cmin);
        static void get(float* data, Compf* p, int size);

        static int countSizes(float* data, uint32_t* sizes, uint32_t* indexes, int n_blocks, float cmin);
        static int countSizes(float* data, int n_blocks, float cmin);
        static int getSizes(Compf* p, uint32_t* sizes, uint32_t* indexes, int n_blocks);
};

// Compresses given data block to p.
inline void CompressedBlock::set(float* data, Compf* p, int size, float cmin=MIN_VALUE) {
    
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
        min.f = cmin;
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
            if (data[i] > cmin) {
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

        // bitmask for marking where values in the array are less than cmin
        ulong &mask = *(ulong*) (p + OFFSET);
        mask = 0;

        float_int value; 
        for (int i = 0; i < BLOCK_SIZE; i++)
        {
            if (data[i] > cmin) {
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

// calculate the compressed size and index for each data block.
inline int CompressedBlock::countSizes(float* data, uint32_t* sizes, uint32_t* indexes, int n_blocks, float cmin=MIN_VALUE) {
   
    #pragma omp parallel for schedule(static,1)
    for (int b = 0; b < n_blocks; b++)
    {
        uint32_t n_values = 0;
        for (int i = 0; i < BLOCK_SIZE; i++)
            n_values += (data[i + BLOCK_SIZE*b] > cmin);
        sizes[b] = n_values;
    }

    int sum = 0;
    for (int b = 0; b < n_blocks; b++)
    {
        indexes[b] = sum;

        if (sizes[b] == 0) 
            sum +=  1;
        else if (sizes[b] >= BLOCK_SIZE - 4) 
            sum += BLOCK_SIZE + OFFSET;
        else 
            sum +=  sizes[b] + OFFSET + sizeof(ulong) / sizeof(Compf);
    }
    return sum;
}

// return just the total compressed size.
inline int CompressedBlock::countSizes(float* data, int n_blocks, float cmin=MIN_VALUE) {
    uint32_t n_values = 0;
    uint32_t sum = 0;
    for (int b = 0; b < n_blocks; b++)
    {
        uint32_t n_values = 0;
        for (int i = 0; i < BLOCK_SIZE; i++)
            n_values += (data[i + BLOCK_SIZE*b] > cmin);

        if (n_values == 0) 
            sum +=  1;
        else if (n_values >= BLOCK_SIZE - 4) 
            sum += BLOCK_SIZE + OFFSET;
        else 
            sum +=  n_values + OFFSET + sizeof(ulong) / sizeof(Compf);
    }
    return sum;
}

inline int CompressedBlock::getSizes(Compf* p, uint32_t* sizes, uint32_t* indexes, int n_blocks) {
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