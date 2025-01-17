#ifndef BMENCODING_H__INCLUDED__
#define BMENCODING_H__INCLUDED__
/*
Copyright(c) 2002-2017 Anatoliy Kuznetsov(anatoliy_kuznetsov at yahoo.com)

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

For more information please visit:  http://bitmagic.io
*/

/*! \file encoding.h
    \brief Encoding utilities for serialization (internal)
*/


#include <memory.h>
#include "bmutil.h"


#ifdef _MSVC_VER
#pragma warning (push)
#pragma warning (disable : 4702)
#endif 

namespace bm
{


// ----------------------------------------------------------------

/*!
   \brief Memory encoding.
   
   Class for encoding data into memory. 
   Class handles aligment issues with the integer data types.
   
   \ingroup gammacode
*/
class encoder
{
public:
    typedef unsigned char* position_type;
public:
    encoder(unsigned char* buf, size_t size);
    void put_8(unsigned char c);
    void put_16(bm::short_t  s);
    void put_16(const bm::short_t* s, unsigned count);
    void put_32(bm::word_t  w);
    void put_32(const bm::word_t* w, unsigned count);
    void put_64(bm::id64_t w);
    void put_prefixed_array_32(unsigned char c, 
                               const bm::word_t* w, unsigned count);
    void put_prefixed_array_16(unsigned char c, 
                               const bm::short_t* s, unsigned count,
                               bool encode_count);
    void memcpy(const unsigned char* src, size_t count);
    size_t size() const;
    unsigned char* get_pos() const;
    void set_pos(unsigned char* buf_pos);
private:
    unsigned char*  buf_;
    unsigned char*  start_;
    size_t          size_;
};

// ----------------------------------------------------------------
/**
    Base class for all decoding functionality
    \ingroup gammacode
*/
class decoder_base
{
public:
    decoder_base(const unsigned char* buf) { buf_ = start_ = buf; }
    
    /// Reads character from the decoding buffer. 
    unsigned char get_8() { return *buf_++; }
    
    /// Returns size of the current decoding stream.
    size_t size() const { return size_t(buf_ - start_); }
    
    /// change current position
    void seek(int delta) { buf_ += delta; }
    
    /// read bytes from the decode buffer
    void memcpy(unsigned char* dst, size_t count);
    
    /// Return current buffer pointer
    const unsigned char* get_pos() const { return buf_; }
protected:
   const unsigned char*   buf_;
   const unsigned char*   start_;
};


// ----------------------------------------------------------------
/**
   Class for decoding data from memory buffer.
   Properly handles aligment issues with integer data types.
   \ingroup gammacode
*/
class decoder : public decoder_base
{
public:
    decoder(const unsigned char* buf);
    bm::short_t get_16();
    bm::word_t get_32();
    bm::id64_t get_64();
    void get_32(bm::word_t* w, unsigned count);
    bool get_32_OR(bm::word_t* w, unsigned count);
    void get_32_AND(bm::word_t* w, unsigned count);
    void get_16(bm::short_t* s, unsigned count);
};

// ----------------------------------------------------------------
/**
   Class for decoding data from memory buffer.
   Properly handles aligment issues with integer data types.
   Converts data to big endian architecture 
   (presumed it was encoded as little endian)
   \ingroup gammacode
*/
typedef decoder decoder_big_endian;


// ----------------------------------------------------------------
/**
   Class for decoding data from memory buffer.
   Properly handles aligment issues with integer data types.
   Converts data to little endian architecture 
   (presumed it was encoded as big endian)
   \ingroup gammacode
*/
class decoder_little_endian : public decoder_base
{
public:
    decoder_little_endian(const unsigned char* buf);
    bm::short_t get_16();
    bm::word_t get_32();
    bm::id64_t get_64();
    void get_32(bm::word_t* w, unsigned count);
    bool get_32_OR(bm::word_t* w, unsigned count);
    void get_32_AND(bm::word_t* w, unsigned count);
    void get_16(bm::short_t* s, unsigned count);
};


/** 
    Byte based writer for un-aligned bit streaming 

    @ingroup gammacode
    @sa encoder
*/
template<class TEncoder>
class bit_out
{
public:
    bit_out(TEncoder& dest)
        : dest_(dest), used_bits_(0), accum_(0)
    {}

    ~bit_out() { flush(); }
    
    /// issue single bit into encode bit-stream
    void put_bit(unsigned value);

    /// issue count bits out of value
    void put_bits(unsigned value, unsigned count);

    /// issue 0 into output stream
    void put_zero_bit();

    /// issue specified number of 0s
    void put_zero_bits(unsigned count);

    /// Elias Gamma encode the specified value
    void gamma(unsigned value);
    
    /// Binary Interpolative array decode
    void bic_encode_u16(const bm::gap_word_t* arr, unsigned sz,
                        bm::gap_word_t lo, bm::gap_word_t hi)
    {
        bic_encode_u16_cm(arr, sz, lo, hi);
    }

    /// Binary Interpolative encoding (array of 16-bit ints)
    void bic_encode_u16_rg(const bm::gap_word_t* arr, unsigned sz,
                           bm::gap_word_t lo,
                           bm::gap_word_t hi);
    
    /// Binary Interpolative encoding (array of 16-bit ints)
    /// cm - "center-minimal"
    void bic_encode_u16_cm(const bm::gap_word_t* arr, unsigned sz,
                           bm::gap_word_t lo,
                           bm::gap_word_t hi);

    /// Binary Interpolative encoding (array of 32-bit ints)
    /// cm - "center-minimal"
    void bic_encode_u32_cm(const bm::word_t* arr, unsigned sz,
                           bm::word_t lo, bm::word_t hi);

    /// Flush the incomplete 32-bit accumulator word
    void flush() { if (used_bits_) flush_accum(); }

private:
    void flush_accum()
    {
        dest_.put_32(accum_);
        used_bits_ = accum_ = 0;
    }
private:
    bit_out(const bit_out&);
    bit_out& operator=(const bit_out&);

private:
    TEncoder&      dest_;      ///< Bit stream target
    unsigned       used_bits_; ///< Bits used in the accumulator
    unsigned       accum_;     ///< write bit accumulator 
};


/** 
    Byte based reader for un-aligned bit streaming 

    @ingroup gammacode
    @sa encoder
*/
template<class TDecoder>
class bit_in
{
public:
    bit_in(TDecoder& decoder)
        : src_(decoder),
          used_bits_(unsigned(sizeof(accum_) * 8)),
          accum_(0)
    {}

    /// decode unsigned value using Elias Gamma coding
    unsigned gamma();
    
    /// read number of bits out of the stream
    unsigned get_bits(unsigned count);

    /// Binary Interpolative array decode
    void bic_decode_u16(bm::gap_word_t* arr, unsigned sz,
                        bm::gap_word_t lo, bm::gap_word_t hi)
    {
        bic_decode_u16_cm(arr, sz, lo, hi);
    }
    
    void bic_decode_u16_bitset(bm::word_t* block, unsigned sz,
                               bm::gap_word_t lo, bm::gap_word_t hi)
    {
        bic_decode_u16_cm_bitset(block, sz, lo, hi);
    }
    void bic_decode_u16_dry(unsigned sz, bm::gap_word_t lo, bm::gap_word_t hi)
    {
        bic_decode_u16_cm_dry(sz, lo, hi);
    }


    /// Binary Interpolative array decode
    void bic_decode_u16_rg(bm::gap_word_t* arr, unsigned sz,
                           bm::gap_word_t lo, bm::gap_word_t hi);
    /// Binary Interpolative array decode
    void bic_decode_u16_cm(bm::gap_word_t* arr, unsigned sz,
                           bm::gap_word_t lo, bm::gap_word_t hi);

    /// Binary Interpolative array decode (32-bit)
    void bic_decode_u32_cm(bm::word_t* arr, unsigned sz,
                           bm::word_t lo, bm::word_t hi);


    /// Binary Interpolative array decode into bitset (32-bit based)
    void bic_decode_u16_rg_bitset(bm::word_t* block, unsigned sz,
                                  bm::gap_word_t lo, bm::gap_word_t hi);

    /// Binary Interpolative array decode into /dev/null
    void bic_decode_u16_rg_dry(unsigned sz, bm::gap_word_t lo, bm::gap_word_t hi);

    /// Binary Interpolative array decode into bitset (32-bit based)
    void bic_decode_u16_cm_bitset(bm::word_t* block, unsigned sz,
                                  bm::gap_word_t lo, bm::gap_word_t hi);

    /// Binary Interpolative array decode into /dev/null
    void bic_decode_u16_cm_dry(unsigned sz, bm::gap_word_t lo, bm::gap_word_t hi);

private:
    bit_in(const bit_in&);
    bit_in& operator=(const bit_in&);
private:
    TDecoder&           src_;        ///< Source of bytes
    unsigned            used_bits_;  ///< Bits used in the accumulator
    unsigned            accum_;      ///< read bit accumulator
};


/**
    Functor for Elias Gamma encoding
    @ingroup gammacode
*/
template<typename T, typename TBitIO>
class gamma_encoder
{
public:
    gamma_encoder(TBitIO& bout) : bout_(bout) 
    {}
        
    /** Encode word */
    void operator()(T value) { bout_.gamma(value); }
private:
    gamma_encoder(const gamma_encoder&);
    gamma_encoder& operator=(const gamma_encoder&);
private:
    TBitIO&  bout_;
};


/**
    Elias Gamma decoder
    @ingroup gammacode
*/
template<typename T, typename TBitIO>
class gamma_decoder
{
public:
    gamma_decoder(TBitIO& bin) : bin_(bin) {}
    
    /**
        Start encoding sequence
    */
    void start() {}
    
    /**
        Stop decoding sequence
    */
    void stop() {}
    
    /**
        Decode word
    */
    T operator()(void) { return (T)bin_.gamma(); }
private:
    gamma_decoder(const gamma_decoder&);
    gamma_decoder& operator=(const gamma_decoder&);
private:
    TBitIO&  bin_;
};


// ----------------------------------------------------------------
// Implementation details. 
// ----------------------------------------------------------------

/*! 
    \fn encoder::encoder(unsigned char* buf, unsigned size) 
    \brief Construction.
    \param buf - memory buffer pointer.
    \param size - size of the buffer
*/
inline encoder::encoder(unsigned char* buf, size_t a_size)
: buf_(buf), start_(buf)
{
    size_ = a_size;
}
/*!
    \brief Encode 8-bit prefix + an array
*/
inline void encoder::put_prefixed_array_32(unsigned char c, 
                                           const bm::word_t* w, 
                                           unsigned count)
{
    put_8(c);
    put_32(w, count);
}

/*!
    \brief Encode 8-bit prefix + an array 
*/
inline void encoder::put_prefixed_array_16(unsigned char c, 
                                           const bm::short_t* s, 
                                           unsigned count,
                                           bool encode_count)
{
    put_8(c);
    if (encode_count)
        put_16((bm::short_t) count);
    put_16(s, count);
}


/*!
   \fn void encoder::put_8(unsigned char c) 
   \brief Puts one character into the encoding buffer.
   \param c - character to encode
*/
BMFORCEINLINE void encoder::put_8(unsigned char c)
{
    *buf_++ = c;
}

/*!
   \fn encoder::put_16(bm::short_t s)
   \brief Puts short word (16 bits) into the encoding buffer.
   \param s - short word to encode
*/
BMFORCEINLINE void encoder::put_16(bm::short_t s)
{
#if (BM_UNALIGNED_ACCESS_OK == 1)
    ::memcpy(buf_, &s, sizeof(bm::short_t)); // optimizer takes care of it
	buf_ += sizeof(bm::short_t);
#else
    *buf_++ = (unsigned char) s;
    s >>= 8;
    *buf_++ = (unsigned char) s;
#endif
}

/*!
   \brief Method puts array of short words (16 bits) into the encoding buffer.
*/
inline void encoder::put_16(const bm::short_t* s, unsigned count)
{
#if (BM_UNALIGNED_ACCESS_OK == 1)
    ::memcpy(buf_, s, sizeof(bm::short_t)*count);
    buf_ += sizeof(bm::short_t) * count;
#else
    unsigned char* buf = buf_;
    const bm::short_t* s_end = s + count;
    do 
    {
        bm::short_t w16 = *s++;
        unsigned char a = (unsigned char)  w16;
        unsigned char b = (unsigned char) (w16 >> 8);
        
        *buf++ = a;
        *buf++ = b;
                
    } while (s < s_end);
    
    buf_ = (unsigned char*)buf;
#endif
}

/*!
    \brief copy bytes into target buffer or just rewind if src is NULL
*/
inline
void encoder::memcpy(const unsigned char* src, size_t count)
{
    BM_ASSERT((buf_ + count) < (start_ + size_));
    if (src)
        ::memcpy(buf_, src, count);
    buf_ += count;
}


/*!
   \fn unsigned encoder::size() const
   \brief Returns size of the current encoding stream.
*/
inline size_t encoder::size() const
{
    return size_t(buf_ - start_);
}

/**
    \brief Get current memory stream position
*/
inline encoder::position_type encoder::get_pos() const
{
    return buf_;
}

/**
    \brief Set current memory stream position
*/
inline void encoder::set_pos(encoder::position_type buf_pos)
{
    buf_ = buf_pos;
}


/*!
   \fn void encoder::put_32(bm::word_t w)
   \brief Puts 32 bits word into encoding buffer.
   \param w - word to encode.
*/
inline void encoder::put_32(bm::word_t w)
{
#if (BM_UNALIGNED_ACCESS_OK == 1)
    ::memcpy(buf_, &w, sizeof(bm::word_t));
    buf_ += sizeof(bm::word_t);
#else
    *buf_++ = (unsigned char) w;
    *buf_++ = (unsigned char) (w >> 8);
    *buf_++ = (unsigned char) (w >> 16);
    *buf_++ = (unsigned char) (w >> 24);
#endif
}

/*!
   \fn void encoder::put_64(bm::id64_t w)
   \brief Puts 64 bits word into encoding buffer.
   \param w - word to encode.
*/
inline void encoder::put_64(bm::id64_t w)
{
#if (BM_UNALIGNED_ACCESS_OK == 1)
    ::memcpy(buf_, &w, sizeof(bm::id64_t));
    buf_ += sizeof(bm::id64_t);
#else
    *buf_++ = (unsigned char) w;
    *buf_++ = (unsigned char) (w >> 8);
    *buf_++ = (unsigned char) (w >> 16);
    *buf_++ = (unsigned char) (w >> 24);
    *buf_++ = (unsigned char) (w >> 32);
    *buf_++ = (unsigned char) (w >> 40);
    *buf_++ = (unsigned char) (w >> 48);
    *buf_++ = (unsigned char) (w >> 56);
#endif
}


/*!
    \brief Encodes array of 32-bit words
*/
inline 
void encoder::put_32(const bm::word_t* w, unsigned count)
{
#if (BM_UNALIGNED_ACCESS_OK == 1)
    ::memcpy(buf_, w, sizeof(bm::word_t) * count);
    buf_ += sizeof(bm::word_t) * count;
#else
    unsigned char* buf = buf_;
    const bm::word_t* w_end = w + count;
    do 
    {
        bm::word_t w32 = *w++;
        unsigned char a = (unsigned char) w32;
        unsigned char b = (unsigned char) (w32 >> 8);
        unsigned char c = (unsigned char) (w32 >> 16);
        unsigned char d = (unsigned char) (w32 >> 24);

        *buf++ = a;
        *buf++ = b;
        *buf++ = c;
        *buf++ = d;
    } while (w < w_end);
    
    buf_ = (unsigned char*)buf;
#endif
}


// ---------------------------------------------------------------------


/*!
    Load bytes from the decode buffer
*/
inline
void decoder_base::memcpy(unsigned char* dst, size_t count)
{
    if (dst)
        ::memcpy(dst, buf_, count);
    buf_ += count;
}

/*!
   \fn decoder::decoder(const unsigned char* buf) 
   \brief Construction
   \param buf - pointer to the decoding memory. 
*/
inline decoder::decoder(const unsigned char* buf) 
: decoder_base(buf)
{
}

/*!
   \fn bm::short_t decoder::get_16()
   \brief Reads 16-bit word from the decoding buffer.
*/
BMFORCEINLINE bm::short_t decoder::get_16() 
{
#if (BM_UNALIGNED_ACCESS_OK == 1)
    bm::short_t a;
    ::memcpy(&a, buf_, sizeof(bm::short_t));
#else
    bm::short_t a = (bm::short_t)(buf_[0] + ((bm::short_t)buf_[1] << 8));
#endif
	buf_ += sizeof(a);
    return a;
}

/*!
   \fn bm::word_t decoder::get_32()
   \brief Reads 32-bit word from the decoding buffer.
*/
BMFORCEINLINE bm::word_t decoder::get_32() 
{
#if (BM_UNALIGNED_ACCESS_OK == 1)
    bm::word_t a;
    ::memcpy(&a, buf_, sizeof(bm::word_t));
#else
	bm::word_t a = buf_[0]+ ((unsigned)buf_[1] << 8) +
                   ((unsigned)buf_[2] << 16) + ((unsigned)buf_[3] << 24);
#endif
    buf_+=sizeof(a);
    return a;
}

/*!
   \fn bm::word_t decoder::get_64()
   \brief Reads 64-bit word from the decoding buffer.
*/
inline
bm::id64_t decoder::get_64()
{
#if (BM_UNALIGNED_ACCESS_OK == 1)
    bm::id64_t a;
    ::memcpy(&a, buf_, sizeof(bm::id64_t));
#else
	bm::id64_t a = buf_[0]+
                   ((bm::id64_t)buf_[1] << 8)  +
                   ((bm::id64_t)buf_[2] << 16) +
                   ((bm::id64_t)buf_[3] << 24) +
                   ((bm::id64_t)buf_[4] << 32) +
                   ((bm::id64_t)buf_[5] << 40) +
                   ((bm::id64_t)buf_[6] << 48) +
                   ((bm::id64_t)buf_[7] << 56);
#endif
    buf_ += sizeof(a);
    return a;
}


/*!
   \fn void decoder::get_32(bm::word_t* w, unsigned count)
   \brief Reads block of 32-bit words from the decoding buffer.
   \param w - pointer on memory block to read into.
   \param count - size of memory block in words.
*/
inline void decoder::get_32(bm::word_t* w, unsigned count)
{
    if (!w) 
    {
        seek(int(count * sizeof(bm::word_t)));
        return;
    }
#if (BM_UNALIGNED_ACCESS_OK == 1)
	::memcpy(w, buf_, count * sizeof(bm::word_t));
	seek(int(count * sizeof(bm::word_t)));
	return;
#else
    const unsigned char* buf = buf_;
    const bm::word_t* w_end = w + count;
    do 
    {
        bm::word_t a = buf[0]+ ((unsigned)buf[1] << 8) +
                   ((unsigned)buf[2] << 16) + ((unsigned)buf[3] << 24);
        *w++ = a;
        buf += sizeof(a);
    } while (w < w_end);
    buf_ = (unsigned char*)buf;
#endif
}
 
/*!
   \brief Reads block of 32-bit words from the decoding buffer and ORs
   to the destination
   \param w - pointer on memory block to read into
   \param count - should match bm::set_block_size
*/
inline
bool decoder::get_32_OR(bm::word_t* w, unsigned count)
{
    if (!w)
    {
        seek(int(count * sizeof(bm::word_t)));
        return false;
    }
#if defined(BMAVX2OPT)
        __m256i* buf_start = (__m256i*)buf_;
        seek(int(count * sizeof(bm::word_t)));
        __m256i* buf_end = (__m256i*)buf_;

        return bm::avx2_or_arr_unal((__m256i*)w, buf_start, buf_end);
#elif defined(BMSSE42OPT) || defined(BMSSE2OPT)
        __m128i* buf_start = (__m128i*)buf_;
        seek(int(count * sizeof(bm::word_t)));
        __m128i* buf_end = (__m128i*)buf_;

        return bm::sse2_or_arr_unal((__m128i*)w, buf_start, buf_end);
#else
        bm::word_t acc = 0;
        const bm::word_t not_acc = acc = ~acc;

        for (unsigned i = 0; i < count; i+=4)
        {
            acc &= (w[i+0] |= get_32());
            acc &= (w[i+1] |= get_32());
            acc &= (w[i+2] |= get_32());
            acc &= (w[i+3] |= get_32());
        }
        return acc == not_acc;
#endif
}

/*!
   \brief Reads block of 32-bit words from the decoding buffer and ANDs
   to the destination
   \param w - pointer on memory block to read into
   \param count - should match bm::set_block_size
*/
inline
void decoder::get_32_AND(bm::word_t* w, unsigned count)
{
    if (!w)
    {
        seek(int(count * sizeof(bm::word_t)));
        return;
    }
#if defined(BMAVX2OPT)
        __m256i* buf_start = (__m256i*)buf_;
        seek(int(count * sizeof(bm::word_t)));
        __m256i* buf_end = (__m256i*)buf_;

        bm::avx2_and_arr_unal((__m256i*)w, buf_start, buf_end);
#elif defined(BMSSE42OPT) || defined(BMSSE2OPT)
        __m128i* buf_start = (__m128i*)buf_;
        seek(int(count * sizeof(bm::word_t)));
        __m128i* buf_end = (__m128i*)buf_;

        bm::sse2_and_arr_unal((__m128i*)w, buf_start, buf_end);
#else
        for (unsigned i = 0; i < count; i+=4)
        {
            w[i+0] &= get_32();
            w[i+1] &= get_32();
            w[i+2] &= get_32();
            w[i+3] &= get_32();
        }
#endif
}



/*!
   \fn void decoder::get_16(bm::short_t* s, unsigned count)
   \brief Reads block of 32-bit words from the decoding buffer.
   \param s - pointer on memory block to read into.
   \param count - size of memory block in words.
*/
inline void decoder::get_16(bm::short_t* s, unsigned count)
{
    if (!s) 
    {
        seek(int(count * sizeof(bm::short_t)));
        return;
    }
#if (BM_UNALIGNED_ACCESS_OK == 1)
    ::memcpy(s, buf_, sizeof(bm::short_t) * count);
    buf_ += sizeof(bm::short_t) * count;
#else
    const unsigned char* buf = buf_;
    const bm::short_t* s_end = s + count;
    do 
    {
        bm::short_t a = (bm::short_t)(buf[0] + ((bm::short_t)buf[1] << 8));
        *s++ = a;
        buf += sizeof(a);
    } while (s < s_end);
    buf_ = (unsigned char*)buf;
#endif
    
}



// ---------------------------------------------------------------------

inline decoder_little_endian::decoder_little_endian(const unsigned char* buf)
: decoder_base(buf)
{
}

inline
bm::short_t decoder_little_endian::get_16()
{
    bm::short_t v1 = bm::short_t(buf_[0]);
    bm::short_t v2 = bm::short_t(buf_[1]);
    bm::short_t a = bm::short_t((v1 << 8) + v2);
    buf_ += sizeof(a);
    return a;
}

inline
bm::word_t decoder_little_endian::get_32()
{
    bm::word_t a = ((unsigned)buf_[0] << 24)+ ((unsigned)buf_[1] << 16) +
                   ((unsigned)buf_[2] << 8) + ((unsigned)buf_[3]);
    buf_+=sizeof(a);
    return a;
}

inline
bm::id64_t decoder_little_endian::get_64()
{
    bm::id64_t a = buf_[0]+
                   ((bm::id64_t)buf_[1] << 56) +
                   ((bm::id64_t)buf_[2] << 48) +
                   ((bm::id64_t)buf_[3] << 40) +
                   ((bm::id64_t)buf_[4] << 32) +
                   ((bm::id64_t)buf_[5] << 24) +
                   ((bm::id64_t)buf_[6] << 16) +
                   ((bm::id64_t)buf_[7] << 8);
    buf_+=sizeof(a);
    return a;
}

inline
void decoder_little_endian::get_32(bm::word_t* w, unsigned count)
{
    if (!w) 
    {
        seek(int(count * sizeof(bm::word_t)));
        return;
    }

    const unsigned char* buf = buf_;
    const bm::word_t* w_end = w + count;
    do 
    {
        bm::word_t a = ((unsigned)buf[0] << 24)+ ((unsigned)buf[1] << 16) +
                       ((unsigned)buf[2] << 8) + ((unsigned)buf[3]);
        *w++ = a;
        buf += sizeof(a);
    } while (w < w_end);
    buf_ = (unsigned char*)buf;
}

inline
bool decoder_little_endian::get_32_OR(bm::word_t* w, unsigned count)
{
    if (!w)
    {
        seek(int(count * sizeof(bm::word_t)));
        return false;
    }

    bm::word_t acc = 0;
    const bm::word_t not_acc = acc = ~acc;

    for (unsigned i = 0; i < count; i+=4)
    {
        acc &= (w[i+0] |= get_32());
        acc &= (w[i+1] |= get_32());
        acc &= (w[i+2] |= get_32());
        acc &= (w[i+3] |= get_32());
    }
    return acc == not_acc;
}

inline
void decoder_little_endian::get_32_AND(bm::word_t* w, unsigned count)
{
    for (unsigned i = 0; i < count; i+=4)
    {
        w[i+0] &= get_32();
        w[i+1] &= get_32();
        w[i+2] &= get_32();
        w[i+3] &= get_32();
    }
}


inline
void decoder_little_endian::get_16(bm::short_t* s, unsigned count)
{
    if (!s) 
    {
        seek(int(count * sizeof(bm::short_t)));
        return;
    }

    const unsigned char* buf = buf_;
    const bm::short_t* s_end = s + count;
    do 
    {
        bm::short_t v1 = bm::short_t(buf_[0]);
        bm::short_t v2 = bm::short_t(buf_[1]);
        bm::short_t a = bm::short_t((v1 << 8) + v2);
        *s++ = a;
        buf += sizeof(a);
    } while (s < s_end);
    buf_ = (unsigned char*)buf;
}

// ----------------------------------------------------------------------
//

template<typename TEncoder>
void bit_out<TEncoder>::put_bit(unsigned value)
{
    BM_ASSERT(value <= 1);
    accum_ |= (value << used_bits_);
    if (++used_bits_ == (sizeof(accum_) * 8))
        flush_accum();
}

// ----------------------------------------------------------------------

template<typename TEncoder>
void bit_out<TEncoder>::put_bits(unsigned value, unsigned count)
{
    unsigned used = used_bits_;
    unsigned acc = accum_;

    {
        unsigned mask = ~0u;
        mask >>= (sizeof(accum_) * 8) - count;
        value &= mask;
    }
    for (;count;)
    {
        unsigned free_bits = unsigned(sizeof(accum_) * 8) - used;
        BM_ASSERT(free_bits);
        acc |= value << used;

        if (count <= free_bits)
        {
            used += count;
            break;
        }
        else
        {
            value >>= free_bits;
            count -= free_bits;
            dest_.put_32(acc);
            acc = used = 0;
            continue;
        }
    }
    if (used == (sizeof(accum_) * 8))
    {
        dest_.put_32(acc);
        acc = used = 0;
    }
    used_bits_ = used;
    accum_ = acc;
}

// ----------------------------------------------------------------------

template<typename TEncoder>
void bit_out<TEncoder>::put_zero_bit()
{
    if (++used_bits_ == (sizeof(accum_) * 8))
        flush_accum();
}

// ----------------------------------------------------------------------

template<typename TEncoder>
void bit_out<TEncoder>::put_zero_bits(unsigned count)
{
    unsigned used = used_bits_;
    unsigned free_bits = (sizeof(accum_) * 8) - used;
    if (count >= free_bits)
    {
        flush_accum();
        count -= free_bits;
        used = 0;

        for ( ;count >= sizeof(accum_) * 8; count -= sizeof(accum_) * 8)
        {
            dest_.put_32(0);
        }
        used += count;
    }
    else
    {
        used += count;
    }
    accum_ |= (1u << used);
    if (++used == (sizeof(accum_) * 8))
        flush_accum();
    else
        used_bits_ = used;
}

// ----------------------------------------------------------------------

template<typename TEncoder>
void bit_out<TEncoder>::gamma(unsigned value)
{
    BM_ASSERT(value);

    unsigned logv = bm::bit_scan_reverse32(value);

    // Put zeroes + 1 bit

    unsigned used = used_bits_;
    unsigned acc = accum_;
    const unsigned acc_bits = (sizeof(acc) * 8);
    unsigned free_bits = acc_bits - used;

    {
        unsigned count = logv;
        if (count >= free_bits)
        {
            dest_.put_32(acc);
            acc = used ^= used;
            count -= free_bits;

            for ( ;count >= acc_bits; count -= acc_bits)
            {
                dest_.put_32(0);
            }
            used += count;
        }
        else
        {
            used += count;
        }
        acc |= (1 << used);
        if (++used == acc_bits)
        {
            dest_.put_32(acc);
            acc = used ^= used;
        }
    }

    // Put the value bits
    //
    {
        unsigned mask = (~0u);
        mask >>= acc_bits - logv;
        value &= mask;
    }
    for (;logv;)
    {
        acc |= value << used;
        free_bits = acc_bits - used;
        if (logv <= free_bits)
        {
            used += logv;
            break;
        }
        else
        {
            value >>= free_bits;
            logv -= free_bits;
            dest_.put_32(acc);
            acc = used ^= used;
            continue;
        }
    } // for

    used_bits_ = used;
    accum_ = acc;
}

// ----------------------------------------------------------------------

template<typename TEncoder>
void bit_out<TEncoder>::bic_encode_u16_rg(const bm::gap_word_t* arr,
                                          unsigned sz,
                                          bm::gap_word_t lo, bm::gap_word_t hi)
{
    for (;sz;)
    {
        BM_ASSERT(lo <= hi);
        unsigned mid_idx = sz >> 1;
        bm::gap_word_t val = arr[mid_idx];
        
        // write the interpolated value
        // write(x, r) where x=(arr[mid] - lo - mid) r=(hi - lo - sz + 1);
        {
            unsigned r = hi - lo - sz + 1;
            if (r)
            {
                unsigned value = val - lo - mid_idx;
                unsigned logv = bm::bit_scan_reverse32(r);
                put_bits(value, logv+1);
            }
        }
        
        bic_encode_u16_rg(arr, mid_idx, lo, gap_word_t(val-1));
        // tail recursion
        //   bic_encode_u16(arr + mid_idx + 1, sz - mid_idx - 1, gap_word_t(val+1), hi);
        arr += mid_idx + 1;
        sz  -= mid_idx + 1;
        lo = gap_word_t(val + 1);
    } // for sz
}

// ----------------------------------------------------------------------

template<typename TEncoder>
void bit_out<TEncoder>::bic_encode_u32_cm(const bm::word_t* arr,
                                          unsigned sz,
                                          bm::word_t lo, bm::word_t hi)
{
    for (;sz;)
    {
        BM_ASSERT(lo <= hi);
        unsigned mid_idx = sz >> 1;
        bm::word_t val = arr[mid_idx];
        
        // write the interpolated value
        // write(x, r) where x=(arr[mid] - lo - mid) r=(hi - lo - sz + 1);
        {
            unsigned r = hi - lo - sz + 1;
            if (r)
            {
                unsigned value = val - lo - mid_idx;
                
                unsigned n = r + 1;
                unsigned logv = bm::bit_scan_reverse32(n);
                unsigned c = (unsigned)(1ull << (logv + 1)) - n;
                int64_t half_c = c >> 1; // c / 2;
                int64_t half_r = r >> 1; // r / 2;
                int64_t lo1 = half_r - half_c;
                int64_t hi1 = half_r + half_c + 1;
                lo1 -= (n & 1);
                logv += (value <= lo1 || value >= hi1);
                
                put_bits(value, logv);
            }
        }
        
        bic_encode_u32_cm(arr, mid_idx, lo, val-1);
        // tail recursive call:
        // bic_encode_u32_cm(arr + mid_idx + 1, sz - mid_idx - 1, val+1, hi);
        arr += mid_idx + 1;
        sz  -= mid_idx + 1;
        lo = val + 1;
    } // for sz
}

// ----------------------------------------------------------------------

#if 0
/**
    Shuffle structure for BIC
    @internal
*/
template<unsigned N>
struct bic_encode_stack_u16
{
    bm::gap_word_t  val_[N];
    bm::gap_word_t  mid_[N];
    bm::gap_word_t  lo_[N];
    bm::gap_word_t  r_[N];

    unsigned stack_size_ = 0;
    
    void bic_shuffle(const bm::gap_word_t* arr,
                     bm::gap_word_t sz, bm::gap_word_t lo, bm::gap_word_t hi)
    {
        for (;sz;)
        {
            BM_ASSERT(lo <= hi);
            bm::gap_word_t mid_idx = sz >> 1;
            bm::gap_word_t val = arr[mid_idx];
            unsigned r = hi - lo - sz + 1;
            if (r)
            {
                unsigned s = stack_size_++;
                r_[s] = (bm::gap_word_t)r;
                val_[s] = val;
                mid_[s] = mid_idx;
                lo_[s] = lo;
            }
            
            bic_shuffle(arr, mid_idx, lo, bm::gap_word_t(val-1));
            // tail recursive call:
            // bic_shuffle(arr + mid_idx + 1, sz - mid_idx - 1, val+1, hi);
            arr += mid_idx + 1;
            sz  -= mid_idx + 1;
            lo = bm::gap_word_t(val + 1);
        } // for sz
    }
};

template<typename TEncoder>
void bit_out<TEncoder>::bic_encode_u16_cm(const bm::gap_word_t* arr,
                                          unsigned sz_i,
                                          bm::gap_word_t lo_i,
                                          bm::gap_word_t hi_i)
{
    BM_ASSERT(sz_i <= 65535);

    bic_encode_stack_u16<bm::bie_cut_off> u16_stack;
    // BIC re-ordering
    u16_stack.bic_shuffle(arr, bm::gap_word_t(sz_i), lo_i, hi_i);
    
    BM_ASSERT(sz_i == u16_stack.stack_size_);
    for (unsigned i = 0; i < sz_i; ++i)
    {
        bm::gap_word_t val = u16_stack.val_[i];
        bm::gap_word_t mid_idx = u16_stack.mid_[i];
        bm::gap_word_t lo = u16_stack.lo_[i];
        unsigned r = u16_stack.r_[i];

        unsigned value = val - lo - mid_idx;
        unsigned n = r + 1;
        unsigned logv = bm::bit_scan_reverse32(n);
        unsigned c = (unsigned)(1ull << (logv + 1)) - n;
        
        int64_t half_c = c >> 1; // c / 2;
        int64_t half_r = r >> 1; // r / 2;
        int64_t lo1 = half_r - half_c;
        int64_t hi1 = half_r + half_c + 1;
        lo1 -= (n & 1);
        logv += (value <= lo1 || value >= hi1);

        put_bits(value, logv);
    } // for i
}
#endif


template<typename TEncoder>
void bit_out<TEncoder>::bic_encode_u16_cm(const bm::gap_word_t* arr,
                                          unsigned sz,
                                          bm::gap_word_t lo, bm::gap_word_t hi)
{
    for (;sz;)
    {
        BM_ASSERT(lo <= hi);
        unsigned mid_idx = sz >> 1;
        bm::gap_word_t val = arr[mid_idx];

        // write the interpolated value
        // write(x, r) where x=(arr[mid] - lo - mid) r=(hi - lo - sz + 1);
        {
            unsigned r = hi - lo - sz + 1;
            if (r)
            {
                unsigned value = val - lo - mid_idx;
                
                unsigned n = r + 1;
                unsigned logv = bm::bit_scan_reverse32(n);
                unsigned c = (unsigned)(1ull << (logv + 1)) - n;
                unsigned half_c = c >> 1; // c / 2;
                unsigned half_r = r >> 1; // r / 2;
                int64_t  lo1 = (int64_t(half_r) - half_c - (n & 1u));
                unsigned hi1 = (half_r + half_c);
                logv += (value <= lo1 || value > hi1);

                put_bits(value, logv);
            }
        }
        
        bic_encode_u16_cm(arr, mid_idx, lo, bm::gap_word_t(val-1));
        // tail recursive call:
        // bic_encode_u32_cm(arr + mid_idx + 1, sz - mid_idx - 1, val+1, hi);
        arr += ++mid_idx;
        sz  -= mid_idx;
        lo = bm::gap_word_t(val + 1);
    } // for sz
}






// ----------------------------------------------------------------------
//
// ----------------------------------------------------------------------


template<class TDecoder>
void bit_in<TDecoder>::bic_decode_u16_rg(bm::gap_word_t* arr, unsigned sz,
                                         bm::gap_word_t lo, bm::gap_word_t hi)
{
    for (;sz;)
    {
        BM_ASSERT(lo <= hi);
        
        unsigned val;
        // read the value
        {
            unsigned r = hi - lo - sz + 1;
            if (r)
            {
                unsigned logv = bm::bit_scan_reverse32(r) + 1;
                val = get_bits(logv);
                BM_ASSERT(val <= r);
            }
            else
            {
                val = 0;
            }
        }
        unsigned mid_idx = sz >> 1;
        val += lo + mid_idx;
        
        BM_ASSERT(val < 65536);
        BM_ASSERT(mid_idx < 65536);
        
        arr[mid_idx] = bm::gap_word_t(val);
        if (sz == 1)
            return;
        bic_decode_u16_rg(arr, mid_idx, lo, bm::gap_word_t(val - 1));
        //bic_decode_u16(arr + mid_idx + 1, sz - mid_idx - 1, bm::gap_word_t(val + 1), hi);
        arr += mid_idx + 1;
        sz  -= mid_idx + 1;
        lo = bm::gap_word_t(val + 1);
    } // for sz
}

// ----------------------------------------------------------------------

template<class TDecoder>
void bit_in<TDecoder>::bic_decode_u32_cm(bm::word_t* arr, unsigned sz,
                                         bm::word_t lo, bm::word_t hi)
{
    for (;sz;)
    {
        BM_ASSERT(lo <= hi);
        
        unsigned val;
        
        // read the interpolated value
        // x = read(r)+ lo + mid,  where r = (hi - lo - sz + 1);
        {
            unsigned r = hi - lo - sz + 1;
            if (r)
            {
                unsigned logv = bm::bit_scan_reverse32(r+1);
                
                unsigned c = unsigned((1ull << (logv + 1)) - r - 1);
                int64_t half_c = c >> 1; // c / 2;
                int64_t half_r = r >> 1; // r / 2;
                int64_t lo1 = half_r - half_c - ((r + 1) & 1);
                int64_t hi1 = half_r + half_c + 1;
                val = get_bits(logv);
                if (val <= lo1 || val >= hi1)
                    val += (get_bits(1) << logv);
                BM_ASSERT(val <= r);
            }
            else
            {
                val = 0;
            }
        }
        
        unsigned mid_idx = sz >> 1;
        val += lo + mid_idx;
        arr[mid_idx] = val;
        if (sz == 1)
            return;
        
        bic_decode_u32_cm(arr, mid_idx, lo, val-1);
        // tail recursive call:
        //  bic_decode_u32_cm(arr + mid_idx + 1, sz - mid_idx - 1, val + 1, hi);
        arr += mid_idx + 1;
        sz  -= mid_idx + 1;
        lo = val + 1;
    } // for sz
}

// ----------------------------------------------------------------------

template<class TDecoder>
void bit_in<TDecoder>::bic_decode_u16_cm(bm::gap_word_t* arr, unsigned sz,
                                         bm::gap_word_t lo, bm::gap_word_t hi)
{
    for (;sz;)
    {
        BM_ASSERT(lo <= hi);
        
        unsigned val;
        
        // read the interpolated value
        // x = read(r)+ lo + mid,  where r = (hi - lo - sz + 1);
        {
            unsigned r = hi - lo - sz + 1;
            if (r)
            {
                unsigned logv = bm::bit_scan_reverse32(r+1);
                
                unsigned c = unsigned((1ull << (logv + 1)) - r - 1);
                int64_t half_c = c >> 1; // c / 2;
                int64_t half_r = r >> 1; // r / 2;
                int64_t lo1 = half_r - half_c - ((r + 1) & 1);
                int64_t hi1 = half_r + half_c + 1;
                val = get_bits(logv);
                if (val <= lo1 || val >= hi1)
                    val += (get_bits(1) << logv);
                BM_ASSERT(val <= r);
            }
            else
            {
                val = 0;
            }
        }
        
        unsigned mid_idx = sz >> 1;
        val += lo + mid_idx;
        arr[mid_idx] = bm::gap_word_t(val);
        if (sz == 1)
            return;
        
        bic_decode_u16_cm(arr, mid_idx, lo, bm::gap_word_t(val-1));
        // tail recursive call:
        //  bic_decode_u32_cm(arr + mid_idx + 1, sz - mid_idx - 1, val + 1, hi);
        arr += mid_idx + 1;
        sz  -= mid_idx + 1;
        lo = bm::gap_word_t(val + 1);
    } // for sz
}

// ----------------------------------------------------------------------

template<class TDecoder>
void bit_in<TDecoder>::bic_decode_u16_cm_bitset(bm::word_t* block, unsigned sz,
                              bm::gap_word_t lo, bm::gap_word_t hi)
{
    for (;sz;)
    {
        BM_ASSERT(lo <= hi);
        
        unsigned val;
        
        // read the interpolated value
        // x = read(r)+ lo + mid,  where r = (hi - lo - sz + 1);
        {
            unsigned r = hi - lo - sz + 1;
            if (r)
            {
                unsigned logv = bm::bit_scan_reverse32(r+1);
                
                unsigned c = unsigned((1ull << (logv + 1)) - r - 1);
                int64_t half_c = c >> 1; // c / 2;
                int64_t half_r = r >> 1; // r / 2;
                int64_t lo1 = half_r - half_c - ((r + 1) & 1);
                int64_t hi1 = half_r + half_c + 1;
                val = get_bits(logv);
                if (val <= lo1 || val >= hi1)
                    val += (get_bits(1) << logv);
                BM_ASSERT(val <= r);
            }
            else
            {
                val = 0;
            }
        }
        
        unsigned mid_idx = sz >> 1;
        val += lo + mid_idx;
        
        // set bit in the target block
        {
            unsigned nword = (val >> bm::set_word_shift);
            block[nword] |= (1u << (val & bm::set_word_mask));
        }
        
        if (sz == 1)
            return;
        
        bic_decode_u16_cm_bitset(block, mid_idx, lo, bm::gap_word_t(val-1));
        // tail recursive call:
        //  bic_decode_u32_cm(block, sz - mid_idx - 1, val + 1, hi);
        sz  -= mid_idx + 1;
        lo = bm::gap_word_t(val + 1);
    } // for sz
}

// ----------------------------------------------------------------------

template<class TDecoder>
void bit_in<TDecoder>::bic_decode_u16_cm_dry(unsigned sz,
                              bm::gap_word_t lo, bm::gap_word_t hi)
{
    for (;sz;)
    {
        BM_ASSERT(lo <= hi);
        
        unsigned val;
        
        // read the interpolated value
        // x = read(r)+ lo + mid,  where r = (hi - lo - sz + 1);
        {
            unsigned r = hi - lo - sz + 1;
            if (r)
            {
                unsigned logv = bm::bit_scan_reverse32(r+1);
                
                unsigned c = unsigned((1ull << (logv + 1)) - r - 1);
                int64_t half_c = c >> 1; // c / 2;
                int64_t half_r = r >> 1; // r / 2;
                int64_t lo1 = half_r - half_c - ((r + 1) & 1);
                int64_t hi1 = half_r + half_c + 1;
                val = get_bits(logv);
                if (val <= lo1 || val >= hi1)
                    val += (get_bits(1) << logv);
                BM_ASSERT(val <= r);
            }
            else
            {
                val = 0;
            }
        }
        
        unsigned mid_idx = sz >> 1;
        val += lo + mid_idx;
        
        if (sz == 1)
            return;
        
        bic_decode_u16_cm_dry(mid_idx, lo, bm::gap_word_t(val-1));
        // tail recursive call:
        //  bic_decode_u32_cm_dry(sz - mid_idx - 1, val + 1, hi);
        sz  -= mid_idx + 1;
        lo = bm::gap_word_t(val + 1);
    } // for sz
}


// ----------------------------------------------------------------------

template<class TDecoder>
void bit_in<TDecoder>::bic_decode_u16_rg_bitset(bm::word_t* block, unsigned sz,
                                                bm::gap_word_t lo, bm::gap_word_t hi)
{
    for (;sz;)
    {
        BM_ASSERT(lo <= hi);
        
        unsigned val;
        // read the value
        {
            unsigned r = hi - lo - sz + 1;
            if (r)
            {
                unsigned logv = bm::bit_scan_reverse32(r) + 1;
                val = get_bits(logv);
                BM_ASSERT(val <= r);
            }
            else
            {
                val = 0;
            }
        }
        unsigned mid_idx = sz >> 1;
        val += lo + mid_idx;
        BM_ASSERT(val < 65536);
        BM_ASSERT(mid_idx < 65536);

        // set bit in the target block
        {
            unsigned nword = (val >> bm::set_word_shift);
            block[nword] |= (1u << (val & bm::set_word_mask));
        }
        
        if (sz == 1)
            return;
        bic_decode_u16_rg_bitset(block, mid_idx, lo, bm::gap_word_t(val - 1));
        // tail recursion:
        //bic_decode_u16_bitset(block, sz - mid_idx - 1, bm::gap_word_t(val + 1), hi);
        sz  -= mid_idx + 1;
        lo = bm::gap_word_t(val + 1);
    } // for sz
}

// ----------------------------------------------------------------------

template<class TDecoder>
void bit_in<TDecoder>::bic_decode_u16_rg_dry(unsigned sz,
                                   bm::gap_word_t lo, bm::gap_word_t hi)
{
    for (;sz;)
    {
        BM_ASSERT(lo <= hi);
        
        unsigned val;
        // read the value
        {
            unsigned r = hi - lo - sz + 1;
            if (r)
            {
                unsigned logv = bm::bit_scan_reverse32(r) + 1;
                val = get_bits(logv);
                BM_ASSERT(val <= r);
            }
            else
            {
                val = 0;
            }
        }
        unsigned mid_idx = sz >> 1;
        val += lo + mid_idx;
        BM_ASSERT(val < 65536);
        BM_ASSERT(mid_idx < 65536);

        if (sz == 1)
            return;
        bic_decode_u16_rg_dry(mid_idx, lo, bm::gap_word_t(val - 1));
        //bic_decode_u16_dry(sz - mid_idx - 1, bm::gap_word_t(val + 1), hi);
        sz  -= mid_idx + 1;
        lo = bm::gap_word_t(val + 1);
    } // for sz
}



// ----------------------------------------------------------------------

template<class TDecoder>
unsigned bit_in<TDecoder>::gamma()
{
    unsigned acc = accum_;
    unsigned used = used_bits_;

    if (used == (sizeof(acc) * 8))
    {
        acc = src_.get_32();
        used ^= used;
    }
    unsigned zero_bits = 0;
    while (true)
    {
        if (acc == 0)
        {
            zero_bits = unsigned(zero_bits +(sizeof(acc) * 8) - used);
            used = 0;
            acc = src_.get_32();
            continue;
        }
        unsigned first_bit_idx =
            #if defined(BM_x86) && (defined(__GNUG__) || defined(_MSC_VER))
                bm::bsf_asm32(acc);
            #else
                bm::bit_scan_fwd(acc);
            #endif
        acc >>= first_bit_idx;
        zero_bits += first_bit_idx;
        used += first_bit_idx;
        break;
    } // while

    // eat the border bit
    //
    if (used == (sizeof(acc) * 8))
    {
        acc = src_.get_32();
        used = 1;
    }
    else
    {
        ++used;
    }
    acc >>= 1;

    // get the value
    unsigned current;
    
    unsigned free_bits = unsigned((sizeof(acc) * 8) - used);
    if (zero_bits <= free_bits)
    {
    take_accum:
        current =
            (acc & block_set_table<true>::_left[zero_bits]) | (1 << zero_bits);
        acc >>= zero_bits;
        used += zero_bits;
        goto ret;
    }

    if (used == (sizeof(acc) * 8))
    {
        acc = src_.get_32();
        used ^= used;
        goto take_accum;
    }

    // take the part
    current = acc;
    // read the next word
    acc = src_.get_32();
    used = zero_bits - free_bits;
    current |=
        ((acc & block_set_table<true>::_left[used]) << free_bits) |
        (1 << zero_bits);

    acc >>= used;
ret:
    accum_ = acc;
    used_bits_ = used;
    return current;
}

// ----------------------------------------------------------------------

template<class TDecoder>
unsigned bit_in<TDecoder>::get_bits(unsigned count)
{
    BM_ASSERT(count);
    const unsigned maskFF = ~0u;
    unsigned acc = accum_;
    unsigned used = used_bits_;

    unsigned value = 0;
    unsigned free_bits = unsigned((sizeof(acc) * 8) - used);
    if (count <= free_bits)
    {
    take_accum:
        value = acc & (maskFF >> (32 - count));
        acc >>= count;
        used += count;
        goto ret;
    }
    if (used == (sizeof(acc) * 8))
    {
        acc = src_.get_32();
        used = 0;
        goto take_accum;
    }
    value = acc;
    acc = src_.get_32();
    used = count - free_bits;
    value |= ((acc & (maskFF >> (32 - used))) << free_bits);
    acc >>= used;
ret:
    accum_ = acc;
    used_bits_ = used;
    return value;
}

// ----------------------------------------------------------------------

} // namespace bm

#ifdef _MSVC_VER
#pragma warning(pop)
#endif 

#endif
