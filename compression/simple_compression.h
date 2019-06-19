

// 
class SimplyCompressedValue
{
    private:
        typedef union {
            float f;
            long i;
        } float_int;

        typedef struct
        {
            unsigned char v1: 8;
            unsigned char v2: 8;
            unsigned char v3: 8;
        } simple;
        
        simple val;
        
    public:
        SimplyCompressedValue();
        SimplyCompressedValue(float x);
        float get();
        void set(float x);
};

inline SimplyCompressedValue::SimplyCompressedValue()
{
    val.v1 = val.v2 = 0;
}

inline SimplyCompressedValue::SimplyCompressedValue(float x)
{
    set(x);
}

inline void SimplyCompressedValue::set(float x)
{
    float_int fi { .f = x };
    val.v1 = fi.i >> 6;
    val.v2 = fi.i >> 14;
    val.v3 = fi.i >> 22;
}

inline float SimplyCompressedValue::get()
{
    float_int fi { .i = val.v3 << 22 };
    fi.i |= val.v2 << 14;
    fi.i |= val.v1 << 6;
    fi.i |= 32;                   // average error correction;

    return fi.f;
}
