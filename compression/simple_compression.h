

// Simple and fast 25 % compression
class CompressedValue
{
    private:
        typedef union {
            float f;
            long i;
        } float_int;

        unsigned char v1: 8;
        unsigned char v2: 8;
        unsigned char v3: 8;
        
    public:
        CompressedValue();
        CompressedValue(float x);
        float get() const;
        void set(float x);

        operator float() { return get(); }
        operator float() const { return get(); }

        CompressedValue& operator=(float value) { set(value); return *this; }
        CompressedValue& operator+=(float value) { set(get() + value); return *this; }
        CompressedValue& operator*=(float value) { set(get() * value); return *this; }
        
        CompressedValue& operator=(const CompressedValue& value) { 
            v1 = value.v1;
            v2 = value.v2;
            v3 = value.v3;
            return *this;    
        }
        CompressedValue& operator+=(CompressedValue value) { 
            set(get() + value.get()); 
            return *this;    
        }
};

inline CompressedValue::CompressedValue()
{
    v1 = v2 = v3 = 0;
}

inline CompressedValue::CompressedValue(float x)
{
    set(x);
}

inline void CompressedValue::set(float x)
{
    float_int fi { .f = x };
    // byte shifts and stores float in to three short values
    v1 = fi.i >> 6;
    v2 = fi.i >> 14;
    v3 = fi.i >> 22;
}

inline float CompressedValue::get() const
{
    if (v3 == 0) return 0.f; 

    float_int fi { .i = v3 << 22 };
    fi.i |= v2 << 14;
    fi.i |= v1 << 6;
    fi.i |= 32;                   // average error correction;

    return fi.f;
}
