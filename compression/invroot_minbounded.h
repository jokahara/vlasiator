#include <cstdlib>
#include <iostream>

typedef ushort Compf;

#define BLOCK_SIZE 64
#define MIN_VALUE 1e-16f
#define OFFSET 2

class CompressedBlock {
    private:
        typedef union {     
            float f;
            uint32_t i;
        } float_int;    // for converting float into integer format

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

    Compf n_values = 0;
    for (int i = 0; i < BLOCK_SIZE; i++)
    {
        if (array[i] > MIN_VALUE) n_values++;
    }
    // if block contains only mask, data pointer is NULL.
    if (!n_values) return;
    
    // find the largest values in the array.
    float_int max { .f = 0.f };
    for (int i = 0; i < BLOCK_SIZE; i++){
        if (array[i] > max.f) {
            max.f = array[i]; 
        }
    }

    float_int min = { .f = MIN_VALUE };
    min.i &= 0x3F800000;

    uint range = (max.i - min.i + 0x3000000) >> 25;
    uint magic = 0x3FF00000 & ( min.i / range + 0x1FFFFFF);
    
    //std::cerr << range << " " << (void*) magic << std::endl;
    
    // if block contains very few zeroes, the whole block is stored.
    // otherwise the locations of zeroes are marked into 64-bit int.
    if (n_values >= BLOCK_SIZE - 4)
    {
        data = (Compf*) malloc((BLOCK_SIZE + 1) * sizeof(Compf));

        Compf* temp = data + OFFSET;
        float_int value; 
        for (int i = 0; i < BLOCK_SIZE; i++) 
        {
            if (array[i] < MIN_VALUE) temp[i] = 0;
            else {
                value.f = array[i];
                value.i  = magic - ( value.i / range );
                temp[i] = value.i >> 9;        
            }   
        }
    }
    else
    {
        data = (Compf*) malloc((n_values + OFFSET) * sizeof(Compf) + sizeof(long));
        Compf* temp = data + OFFSET + sizeof(ulong) / sizeof(Compf);

        // bitmask for marking where values in the array are less than MIN_VALUE
        ulong &mask = *(ulong*) (data + OFFSET);
        mask = 0;

        float_int value; 
        for (int i = 0; i < BLOCK_SIZE; i++)
        {
            if (array[i] > MIN_VALUE) {
                mask |= (1UL << i);

                value.f = array[i];
                value.i  = magic - ( value.i / range );
                *temp++ = value.i >> 9;
            }
        }
    }
    // save number of values, range and magic number
    *data = n_values + (range << 8);
    *(data + 1) = (magic >> 16);
}

inline void CompressedBlock::get(float *array) const {
    if (!data)
    {
        for (int i = 0; i < BLOCK_SIZE; i++) 
            array[i] = 0.f;
        return;
    }

    int range = *data >> 8;
    int magic = *(data+1) << 16;

    if ((*data & 0xFF) >= BLOCK_SIZE - 4)
    {
        float_int value;
        Compf *temp = data + OFFSET;
        for (int i = 0; i < BLOCK_SIZE; i++) 
        {
            if (temp[i] == 0) array[i] = 0.f;
            else {
                value.i = 0xFF |(temp[i] << 9);
                value.i = range * (magic - value.i);
                array[i] = value.f;
            }
        }
    }
    else
    {
        Compf *temp = data + OFFSET + sizeof(ulong) / sizeof(Compf);
        const ulong mask = *(ulong*) (data + OFFSET);

        float_int value;
        for (int i = 0; i < BLOCK_SIZE; i++)
        {
            if ((mask >> i) & 1UL) {
                value.i = 0xFF |(*temp++ << 9);
                value.i = range * (magic - value.i);
                array[i] = value.f;
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

    Compf n_values = *data & 0xFF;
    return (n_values >= BLOCK_SIZE - 4) 
            ? (BLOCK_SIZE + OFFSET) * sizeof(Compf)
            : (n_values + OFFSET) * sizeof(Compf) + sizeof(long); 
}