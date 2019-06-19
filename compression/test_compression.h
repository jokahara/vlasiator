

// Simple and fast 25 % compression
class CompressedValue
{
    private:
        typedef union {
            float f;
            long i;
        } float_int;

        unsigned int v;
        
    public:
        CompressedValue() { };
        CompressedValue(float x);
        float get() const;
        void set(float x);

        operator float() { return get(); }
        operator float() const { return get(); }

        CompressedValue& operator=(float value) { set(value); return *this; }
        CompressedValue& operator+=(float value) { set(get() + value); return *this; }
        CompressedValue& operator*=(float value) { set(get() * value); return *this; }
        
        CompressedValue& operator=(const CompressedValue& value) { 
            v = value.v;
            return *this;    
        }
        CompressedValue& operator+=(CompressedValue value) { 
            set(get() + value.get()); 
            return *this;    
        }
};

inline CompressedValue::CompressedValue(float x)
{
    set(x);
}

inline void CompressedValue::set(float x)
{
    float_int fi { .f = x };
    v = fi.i;
}

inline float CompressedValue::get() const
{
    float_int fi { .i = v };

    return fi.f;
}
