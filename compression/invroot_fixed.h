#include <cstdlib>

typedef ushort Compf;

#define BLOCK_SIZE 64
#define MIN_VALUE 1E-15f
#define RANGE 7
#define MAGIC 0x7700000
#define OFFSET 1

class CompressedBlock {
    private:
        typedef union {     
            float f;
            uint32_t i;
        } float_int;    // for converting float into integer format

        Compf *data;
        float average;
        
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
            
            average = block.average;
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
    average = 0.f;
}

inline CompressedBlock::CompressedBlock(const CompressedBlock& block) {
    data = NULL;
    
    average = block.average;
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
    float sum = 0.f;
    for (int i = 0; i < BLOCK_SIZE; i++)
    {
        if (array[i] > MIN_VALUE) n_values++;
        else sum += array[i];
    }
    average = (n_values != BLOCK_SIZE) ? sum / (BLOCK_SIZE - n_values) : 0.f;

    // if block contains only zeroes, data pointer is NULL;
    if (!n_values) return;
    
    // if block contains very few zeroes, the whole block is stored.
    // otherwise the locations of zeroes are marked into 64-bit int.
    if (n_values >= BLOCK_SIZE - 4)
    {
        data = (Compf*) malloc((BLOCK_SIZE + 1) * sizeof(Compf));
        *data = n_values;

        float_int value; 
        for (int i = 0; i < BLOCK_SIZE; i++) 
        {
            if (array[i] < MIN_VALUE) data[i+1] = 0;
            else
            {
                value.f = array[i];
                value.i  = MAGIC - ( value.i / RANGE );
                data[i+1] = value.i >> 9;        
            }   
        }
    }
    else
    {
        data = (Compf*) malloc((n_values + 1) * sizeof(Compf) + sizeof(long));
        *data = n_values;

        ulong &zeroes = *(ulong*) (data + 1);
        zeroes = 0;
        
        Compf* temp = data + 1 + sizeof(ulong) / sizeof(Compf);

        float_int value; 
        for (int i = 0; i < BLOCK_SIZE; i++)
        {
            if (array[i] > MIN_VALUE) {
                zeroes |= (1UL << i);   // bit 1 marks points were value is more than MIN_VALUE.

                value.f = array[i];
                value.i  = MAGIC - ( value.i / RANGE );
                *temp++ = value.i >> 9;
            }
        }
    }
}

inline void CompressedBlock::get(float *array) const {
    if (!data)
    {
        for (int i = 0; i < BLOCK_SIZE; i++) 
            array[i] = average;
    }
    else if (*data >= BLOCK_SIZE - 4)
    {
        float_int value;
        for (int i = 0; i < BLOCK_SIZE; i++) 
        {
            if (data[i+1] == 0) array[i] = average;
            else
            {
                value.i = 0xFF |(data[i+1] << 9);
                value.i = RANGE * (MAGIC - value.i);
                array[i] = value.f;
            }
        }
    }
    else
    {
        Compf *temp = data + 1 + sizeof(ulong) / sizeof(Compf);
        const ulong zeroes = *(ulong*) (data + 1);

        float_int value;
        for (int i = 0; i < BLOCK_SIZE; i++)
        {
            if ((zeroes >> i) & 1UL) {
                value.i = 0xFF |(*temp++ << 9);
                value.i = RANGE * (MAGIC - value.i);
                array[i] = value.f;
            }
            else array[i] = average;
        }
    }
}

inline void CompressedBlock::clear() {
    if (data) free(data);
    average = 0.f;
    data = NULL;
}

inline size_t CompressedBlock::compressedSize() const { 
    if(!data) return 0;

    Compf n_values = *data;
    return (n_values >= BLOCK_SIZE - 4) 
            ? (BLOCK_SIZE + OFFSET) * sizeof(Compf)
            : (n_values + OFFSET) * sizeof(Compf) + sizeof(long); 
}