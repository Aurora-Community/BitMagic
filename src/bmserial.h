#ifndef BMSERIAL__H__INCLUDED__
#define BMSERIAL__H__INCLUDED__
/*
Copyright(c) 2002-2019 Anatoliy Kuznetsov(anatoliy_kuznetsov at yahoo.com)

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

/*! \file bmserial.h
    \brief Serialization / compression of bvector<>.
    Set theoretical operations on compressed BLOBs.
*/

/*! 
    \defgroup bvserial bvector<> serialization
    Serialization for bvector<> container
 
    \ingroup bvector
*/

#ifndef BM__H__INCLUDED__
// BitMagic utility headers do not include main "bm.h" declaration 
// #include "bm.h" or "bm64.h" explicitly 
# error missing include (bm.h or bm64.h)
#endif


#ifdef _MSC_VER
#pragma warning( push )
#pragma warning( disable : 4311 4312 4127)
#endif



#include "encoding.h"
#include "bmfunc.h"
#include "bmtrans.h"
#include "bmalgo_impl.h"
#include "bmutil.h"
#include "bmbuffer.h"
#include "bmdef.h"


namespace bm
{

const unsigned set_compression_max = 5;     ///< Maximum supported compression level
const unsigned set_compression_default = 5; ///< Default compression level

/**
    Bit-vector serialization class.
    
    Class designed to convert sparse bit-vectors into a single block of memory
    ready for file or database storage or network transfer.
    
    Reuse of this class for multiple serializations (but not across threads).
    Class resue offers some performance advantage (helps with temp memory
    reallocations).
    
    @ingroup bvserial 
*/
template<class BV>
class serializer
{
public:
    typedef BV                                                bvector_type;
    typedef typename bvector_type::allocator_type             allocator_type;
    typedef typename bvector_type::blocks_manager_type        blocks_manager_type;
    typedef typename bvector_type::statistics                 statistics_type;
    typedef typename bvector_type::block_idx_type             block_idx_type;
    typedef typename bvector_type::size_type                  size_type;

    typedef byte_buffer<allocator_type> buffer;
public:
    /**
        Constructor
        
        \param alloc - memory allocator
        \param temp_block - temporary block for various operations
               (if NULL it will be allocated and managed by serializer class)
        Temp block is used as a scratch memory during serialization,
        use of external temp block allows to avoid unnecessary re-allocations.
     
        Temp block attached is not owned by the class and NOT deallocated on
        destruction.
    */
    serializer(const allocator_type&   alloc  = allocator_type(),
              bm::word_t*  temp_block = 0);
    
    serializer(bm::word_t*  temp_block);

    ~serializer();

    /*! @name Compression level settings                               */
    //@{

    // --------------------------------------------------------------------
    /**
        Set compression level. Higher compression takes more time to process.
        @param clevel - compression level (0-4)
    */
    void set_compression_level(unsigned clevel);

    /**
        Get compression level (0-5), Default 5 (recommended)
        0 - take as is
        1, 2 - apply light weight RLE/GAP encodings, limited depth hierarchical
               compression, intervals encoding
        3 - variant of 2 with different cut-offs
        4 - delta transforms plus Elias Gamma encoding where possible legacy)
        5 - binary interpolated encoding (Moffat, et al)
     
        Recommended: use 3 or 5
    */
    unsigned get_compression_level() const { return compression_level_; }
    
    //@}

    
    // --------------------------------------------------------------------
    /*! @name Serialization Methods                                      */
    //@{

    /**
        Bitvector serialization into memory block
        
        @param bv - input bitvector
        @param buf - out buffer (pre-allocated)
           No range checking is done in this method. 
           It is responsibility of caller to allocate sufficient 
           amount of memory using information from calc_stat() function.        
        
        @param buf_size - size of the output buffer
        
       @return Size of serialization block.
       @sa calc_stat     
    */
    size_type serialize(const BV& bv,
                        unsigned char* buf, size_t buf_size);
    
    /**
        Bitvector serialization into buffer object (resized automatically)
     
        @param bv       - input bitvector
        @param buf      - output buffer object
        @param bv_stat  - input (optional) bit-vector statistics object
                          if NULL, serialize will compute the statistics
    */
    void serialize(const BV& bv,
                   typename serializer<BV>::buffer& buf,
                   const statistics_type* bv_stat = 0);
    
    /**
        Bitvector serialization into buffer object (resized automatically)
        Input bit-vector gets optimized and then destroyed, content is
        NOT guaranteed after this operation.
        Effectively it moves data into the buffer.

        The reason this operation exsists is because it is faster to do
        all three operations in one single pass.
        This is a destructive serialization!

        @param bv       - input/output bitvector
        @param buf      - output buffer object
    */
    void optimize_serialize_destroy(BV& bv,
                                    typename serializer<BV>::buffer& buf);

    //@}
    // --------------------------------------------------------------------

    /**
        Return serialization counter vector
        @internal
    */
    const size_type* get_compression_stat() const { return compression_stat_; }
    
    /**
        Set GAP length serialization (serializes GAP levels of the original vector)
                
        @param value - when TRUE serialized vector includes GAP levels parameters
    */
    void gap_length_serialization(bool value);
    
    /**
        Set byte-order serialization (for cross platform compatibility)
        @param value - TRUE serialization format includes byte-order marker
    */
    void byte_order_serialization(bool value);

protected:
    /**
        Encode serialization header information
    */
    void encode_header(const BV& bv, bm::encoder& enc);
    
    /*! Encode GAP block */
    void encode_gap_block(const bm::gap_word_t* gap_block, bm::encoder& enc);

    /*! Encode GAP block with Elias Gamma coder */
    void gamma_gap_block(const bm::gap_word_t* gap_block, bm::encoder& enc);

    /**
        Encode GAP block as delta-array with Elias Gamma coder
    */
    void gamma_gap_array(const bm::gap_word_t* gap_block, 
                         unsigned              arr_len, 
                         bm::encoder&          enc,
                         bool                  inverted = false);
    
    /// Encode bit-block as an array of bits
    void encode_bit_array(const bm::word_t* block,
                          bm::encoder& enc, bool inverted);
    
    void gamma_gap_bit_block(const bm::word_t* block,
                             bm::encoder&      enc);
    
    void gamma_arr_bit_block(const bm::word_t* block,
                          bm::encoder& enc, bool inverted);

    void bienc_arr_bit_block(const bm::word_t* block,
                            bm::encoder& enc, bool inverted);

    /// encode bit-block as interpolated bit block of gaps
    void bienc_gap_bit_block(const bm::word_t* block, bm::encoder& enc);

    void interpolated_arr_bit_block(const bm::word_t* block,
                            bm::encoder& enc, bool inverted);
    /// encode bit-block as interpolated gap block
    void interpolated_gap_bit_block(const bm::word_t* block,
                                    bm::encoder&      enc);

    /**
        Encode GAP block as an array with binary interpolated coder
    */
    void interpolated_gap_array(const bm::gap_word_t* gap_block,
                                unsigned              arr_len,
                                bm::encoder&          enc,
                                bool                  inverted);


    /*! Encode GAP block with using binary interpolated encoder */
    void interpolated_encode_gap_block(
                const bm::gap_word_t* gap_block, bm::encoder& enc);

    /**
        Encode BIT block with repeatable runs of zeroes
    */
    void encode_bit_interval(const bm::word_t* blk, 
                             bm::encoder&      enc,
                             unsigned          size_control);
    /**
        Encode bit-block using digest (hierarchical compression)
    */
    void encode_bit_digest(const bm::word_t*  blk,
                             bm::encoder&     enc,
                             bm::id64_t       d0);

    /**
        Determine best representation for GAP block based
        on current set compression level
     
        @return  set_block_gap, set_block_bit_1bit, set_block_arrgap
                 set_block_arrgap_egamma, set_block_arrgap_bienc
                 set_block_arrgap_inv, set_block_arrgap_egamma_inv
                 set_block_arrgap_bienc_inv, set_block_gap_egamma
                 set_block_gap_bienc
     
        @internal
    */
    unsigned char find_gap_best_encoding(const bm::gap_word_t* gap_block);
    
    /// Determine best representation for a bit-block
    unsigned char find_bit_best_encoding(const bm::word_t* block);

    /// Determine best representation for a bit-block (level 5)
    unsigned char find_bit_best_encoding_l5(const bm::word_t* block);

    /// Reset all accumulated compression statistics
    void reset_compression_stats();
    
    void reset_models() { mod_size_ = 0; }
    void add_model(unsigned char mod, unsigned score);

private:
    serializer(const serializer&);
    serializer& operator=(const serializer&);
    
private:
    typedef bm::bit_out<bm::encoder>                        bit_out_type;
    typedef bm::gamma_encoder<bm::gap_word_t, bit_out_type> gamma_encoder_func;
    typedef bm::heap_vector<bm::gap_word_t, allocator_type> block_arridx_type;
    typedef typename allocator_type::allocator_pool_type    allocator_pool_type;

private:
    bm::id64_t         digest0_;
    unsigned           bit_model_d0_size_; ///< memory (bytes) by d0 method (bytes)
    unsigned           bit_model_0run_size_; ///< memory (bytes) by run-0 method (bytes)
    block_arridx_type  bit_idx_arr_;
    unsigned           scores_[64];
    unsigned char      models_[64];
    unsigned           mod_size_;
    
    allocator_type  alloc_;
    size_type*      compression_stat_;
    bool            gap_serial_;
    bool            byte_order_serial_;
    bm::word_t*     temp_block_;
    unsigned        compression_level_;
    bool            own_temp_block_;
    
    bool            optimize_; ///< flag to optimize the input vector
    bool            free_;     ///< flag to free the input vector
    allocator_pool_type pool_;

};

/**
    Base deserialization class
    \ingroup bvserial
*/
template<class DEC> class deseriaizer_base
{
protected:
    typedef DEC decoder_type;
    
protected:
    deseriaizer_base() : id_array_(0) {}
    
    /// Read GAP block from the stream
    void read_gap_block(decoder_type&   decoder, 
                        unsigned        block_type, 
                        bm::gap_word_t* dst_block,
                        bm::gap_word_t& gap_head);

	/// Read list of bit ids
	///
	/// @return number of ids
	unsigned read_id_list(decoder_type&   decoder, 
                          unsigned        block_type, 
                          bm::gap_word_t* dst_arr);
    
    /// Read binary interpolated list into a bit-set
    void read_bic_arr(decoder_type&   decoder, bm::word_t* blk);

    /// Read binary interpolated gap blocks into a bitset
    void read_bic_gap(decoder_type&   decoder, bm::word_t* blk);

    /// Read inverted binary interpolated list into a bit-set
    void read_bic_arr_inv(decoder_type&   decoder, bm::word_t* blk);
    
    /// Read digest0-type bit-block
    void read_digest0_block(decoder_type& decoder, bm::word_t* blk);
    
    
    /// read bit-block encoded as runs
    static
    void read_0runs_block(decoder_type& decoder, bm::word_t* blk);
    
    static
    const char* err_msg() { return "BM::Invalid serialization format"; }


protected:
    bm::gap_word_t*   id_array_; ///< ptr to idx array for temp decode use
};

/**
    Deserializer for bit-vector
    \ingroup bvserial 
*/
template<class BV, class DEC>
class deserializer : protected deseriaizer_base<DEC>
{
public:
    typedef deseriaizer_base<DEC>                          parent_type;
    typedef BV                                             bvector_type;
    typedef typename bvector_type::allocator_type          allocator_type;
    typedef typename BV::size_type                         size_type;
    typedef typename bvector_type::block_idx_type          block_idx_type;
    typedef typename deseriaizer_base<DEC>::decoder_type   decoder_type;
    
public:
    deserializer();
    ~deserializer();

    size_t deserialize(bvector_type&        bv,
                       const unsigned char* buf,
                       bm::word_t*          temp_block);
protected:
   typedef typename BV::blocks_manager_type blocks_manager_type;

protected:
   void deserialize_gap(unsigned char btype, decoder_type& dec, 
                        bvector_type&  bv, blocks_manager_type& bman,
                        block_idx_type nb,
                        bm::word_t* blk);
   void decode_bit_block(unsigned char btype, decoder_type& dec,
                         blocks_manager_type& bman,
                         block_idx_type nb,
                         bm::word_t* blk);
protected:
    typedef bm::heap_vector<bm::gap_word_t, allocator_type> block_arridx_type;

protected:
    block_arridx_type  bit_idx_arr_;
    block_arridx_type  gap_temp_block_;
    bm::word_t*        temp_block_;
    allocator_type     alloc_;
};


/**
    Iterator to walk forward the serialized stream.

    \internal
    \ingroup bvserial 
*/
template<class BV, class SerialIterator>
class iterator_deserializer
{
public:
    typedef BV                               bvector_type;
    typedef typename bvector_type::size_type size_type;
    typedef SerialIterator                   serial_iterator_type;
public:

    void set_range(size_type from, size_type to);

    size_type deserialize(bvector_type&         bv,
                          serial_iterator_type& sit,
                          bm::word_t*           temp_block,
                          set_operation         op = bm::set_OR,
                          bool                  exit_on_one = false);

private:
    typedef typename BV::blocks_manager_type            blocks_manager_type;
    typedef typename bvector_type::block_idx_type       block_idx_type;

    /// load data from the iterator of type "id list"
    static
    void load_id_list(bvector_type&         bv, 
                      serial_iterator_type& sit,
                      unsigned              id_count,
                      bool                  set_clear);

    /// Finalize the deserialization (zero target vector tail or bit-count tail)
    static
    size_type finalize_target_vector(blocks_manager_type& bman,
                                     set_operation        op,
                                     size_type            bv_block_idx);

    /// Process (obsolete) id-list serialization format
    static
    size_type process_id_list(bvector_type&         bv,
                              serial_iterator_type& sit,
                              set_operation         op);
    static
    const char* err_msg() { return "BM::de-serialization format error"; }
private:
    bool                       is_range_set_ = false;
    size_type                  nb_range_from_ = 0;
    size_type                  nb_range_to_ = 0;
};

/*!
    @brief Serialization stream iterator

    Iterates blocks and control tokens of serialized bit-stream
    \ingroup bvserial
    @internal
*/
template<class DEC>
class serial_stream_iterator : protected deseriaizer_base<DEC>
{
public:
    typedef typename deseriaizer_base<DEC>::decoder_type decoder_type;
    #ifdef BM64ADDR
        typedef bm::id64_t   block_idx_type;
    #else
        typedef bm::id_t     block_idx_type;
    #endif

public:
    serial_stream_iterator(const unsigned char* buf);
    ~serial_stream_iterator();

    /// serialized bitvector size
    block_idx_type bv_size() const { return bv_size_; }

    /// Returns true if end of bit-stream reached 
    bool is_eof() const { return end_of_stream_; }

    /// get next block
    void next();

	/// skip all zero or all-one blocks
	block_idx_type skip_mono_blocks();

    /// read bit block, using logical operation
    unsigned get_bit_block(bm::word_t*       dst_block, 
                           bm::word_t*       tmp_block,
                           set_operation     op);


    /// Read gap block data (with head)
    void get_gap_block(bm::gap_word_t* dst_block);

    /// Return current decoder size
    unsigned dec_size() const { return decoder_.size(); }

    /// Get low level access to the decoder (use carefully)
    decoder_type& decoder() { return decoder_; }

    /// iterator is a state machine, this enum encodes 
    /// its key value
    ///
    enum iterator_state 
    {
        e_unknown = 0,
        e_list_ids,     ///< plain int array
        e_blocks,       ///< stream of blocks
        e_zero_blocks,  ///< one or more zero bit blocks
        e_one_blocks,   ///< one or more all-1 bit blocks
        e_bit_block,    ///< one bit block
        e_gap_block     ///< one gap block

    };

    /// Returns iterator internal state
    iterator_state state() const { return this->state_; }

    iterator_state get_state() const { return this->state_; }
    /// Number of ids in the inverted list (valid for e_list_ids)
    unsigned get_id_count() const { return this->id_cnt_; }

    /// Get last id from the id list
    bm::id_t get_id() const { return this->last_id_; }

    /// Get current block index 
    block_idx_type block_idx() const { return this->block_idx_; }

public:
    /// member function pointer for bitset-bitset get operations
    /// 
    typedef 
        unsigned (serial_stream_iterator<DEC>::*get_bit_func_type)
                                                (bm::word_t*,bm::word_t*);

    unsigned 
    get_bit_block_ASSIGN(bm::word_t* dst_block, bm::word_t* tmp_block);
    unsigned 
    get_bit_block_OR    (bm::word_t* dst_block, bm::word_t* tmp_block);
    unsigned 
    get_bit_block_AND   (bm::word_t* dst_block, bm::word_t* tmp_block);
    unsigned 
    get_bit_block_SUB   (bm::word_t* dst_block, bm::word_t* tmp_block);
    unsigned 
    get_bit_block_XOR   (bm::word_t* dst_block, bm::word_t* tmp_block);
    unsigned 
    get_bit_block_COUNT (bm::word_t* dst_block, bm::word_t* tmp_block);
    unsigned 
    get_bit_block_COUNT_AND(bm::word_t* dst_block, bm::word_t* tmp_block);
    unsigned 
    get_bit_block_COUNT_OR(bm::word_t* dst_block, bm::word_t* tmp_block);
    unsigned 
    get_bit_block_COUNT_XOR(bm::word_t* dst_block, bm::word_t* tmp_block);
    unsigned 
    get_bit_block_COUNT_SUB_AB(bm::word_t* dst_block, bm::word_t* tmp_block);
    unsigned 
    get_bit_block_COUNT_SUB_BA(bm::word_t* dst_block, bm::word_t* tmp_block);
    unsigned 
    get_bit_block_COUNT_A(bm::word_t* dst_block, bm::word_t* tmp_block);
    unsigned 
    get_bit_block_COUNT_B(bm::word_t* dst_block, bm::word_t* tmp_block)
    {
        return get_bit_block_COUNT(dst_block, tmp_block);
    }

    /// Get array of bits out of the decoder into bit block
    /// (Converts inverted list into bits)
    /// Returns number of words (bits) being read
    unsigned get_arr_bit(bm::word_t* dst_block, 
                         bool clear_target=true);

	/// Get current block type
	unsigned get_block_type() const { return block_type_; }

	unsigned get_bit();
 
    void get_inv_arr(bm::word_t* block);

protected:
    get_bit_func_type  bit_func_table_[bm::set_END];

    decoder_type       decoder_;
    bool               end_of_stream_;
    block_idx_type     bv_size_;
    iterator_state     state_;
    unsigned           id_cnt_;  ///< Id counter for id list
    bm::id_t           last_id_; ///< Last id from the id list
    gap_word_t         glevels_[bm::gap_levels]; ///< GAP levels

    unsigned           block_type_;     ///< current block type
    block_idx_type     block_idx_;      ///< current block index
    block_idx_type     mono_block_cnt_; ///< number of 0 or 1 blocks

    gap_word_t         gap_head_;
    gap_word_t*        block_idx_arr_;
};

/**
    Deserializer, performs logical operations between bit-vector and
    serialized bit-vector. This utility class potentially provides faster
    and/or more memory efficient operation than more conventional deserialization
    into memory bvector and then logical operation

    \ingroup bvserial 
*/
template<class BV>
class operation_deserializer
{
public:
    typedef BV                               bvector_type;
    typedef typename bvector_type::size_type size_type;
public:
    /*!
    \brief Deserialize bvector using buffer as set operation argument
    
    \param bv - target bvector
    \param buf - serialized buffer as a logical argument
    \param temp_block - temporary block to avoid re-allocations
    \param op - set algebra operation (default: OR)
    \param exit_on_one - quick exit if set operation found some result
    
    \return bitcount
    */
    static
    size_type deserialize(bvector_type&       bv,
                         const unsigned char* buf, 
                         bm::word_t*          temp_block,
                         set_operation        op = bm::set_OR,
                         bool                 exit_on_one = false ///<! exit early if any one are found
                         );

    /*!
        Deserialize range using mask vector (AND)
        \param bv - target bvector (should be set ranged)
        \param temp_block - temporary block to avoid re-allocations
        \param idx_from - range of bits set for deserialization [from..to]
        \param idx_to - range of bits [from..to]
    */
    void deserialize_range(bvector_type&       bv,
                           const unsigned char* buf,
                           bm::word_t*          temp_block,
                           size_type            idx_from,
                           size_type            idx_to);
private:
    typedef 
        typename BV::blocks_manager_type               blocks_manager_type;
    typedef 
        serial_stream_iterator<bm::decoder>            serial_stream_current;
    typedef 
        serial_stream_iterator<bm::decoder_big_endian> serial_stream_be;
    typedef 
        serial_stream_iterator<bm::decoder_little_endian> serial_stream_le;
    typedef typename bvector_type::block_idx_type             block_idx_type;

};




//----------------------------------------------------------------------------
//
//----------------------------------------------------------------------------


/// \internal
/// \ingroup bvserial
enum serialization_header_mask {
    BM_HM_DEFAULT = 1,
    BM_HM_RESIZE  = (1 << 1),  ///< resized vector
    BM_HM_ID_LIST = (1 << 2),  ///< id list stored
    BM_HM_NO_BO   = (1 << 3),  ///< no byte-order
    BM_HM_NO_GAPL = (1 << 4),  ///< no GAP levels
    BM_HM_64_BIT  = (1 << 5)   ///< 64-bit vector
};



// Serialization stream block type constants
//

const unsigned char set_block_end               = 0;  //!< End of serialization
const unsigned char set_block_1zero             = 1;  //!< One all-zero block
const unsigned char set_block_1one              = 2;  //!< One block all-set (1111...)
const unsigned char set_block_8zero             = 3;  //!< Up to 256 zero blocks
const unsigned char set_block_8one              = 4;  //!< Up to 256 all-set blocks
const unsigned char set_block_16zero            = 5;  //!< Up to 65536 zero blocks
const unsigned char set_block_16one             = 6;  //!< UP to 65536 all-set blocks
const unsigned char set_block_32zero            = 7;  //!< Up to 4G zero blocks
const unsigned char set_block_32one             = 8;  //!< UP to 4G all-set blocks
const unsigned char set_block_azero             = 9;  //!< All other blocks zero
const unsigned char set_block_aone              = 10; //!< All other blocks one
const unsigned char set_block_bit               = 11; //!< Plain bit block
const unsigned char set_block_sgapbit           = 12; //!< SGAP compressed bitblock
const unsigned char set_block_sgapgap           = 13; //!< SGAP compressed GAP block
const unsigned char set_block_gap               = 14; //!< Plain GAP block
const unsigned char set_block_gapbit            = 15; //!< GAP compressed bitblock
const unsigned char set_block_arrbit            = 16; //!< List of bits ON
const unsigned char set_block_bit_interval      = 17; //!< Interval block
const unsigned char set_block_arrgap            = 18; //!< List of bits ON (GAP block)
const unsigned char set_block_bit_1bit          = 19; //!< Bit block with 1 bit ON
const unsigned char set_block_gap_egamma        = 20; //!< Gamma compressed GAP block
const unsigned char set_block_arrgap_egamma     = 21; //!< Gamma compressed delta GAP array
const unsigned char set_block_bit_0runs         = 22; //!< Bit block with encoded zero intervals
const unsigned char set_block_arrgap_egamma_inv = 23; //!< Gamma compressed inverted delta GAP array
const unsigned char set_block_arrgap_inv        = 24; //!< List of bits OFF (GAP block)
const unsigned char set_block_64zero            = 25; //!< lots of zero blocks
const unsigned char set_block_64one             = 26; //!< lots of all-set blocks

const unsigned char set_block_gap_bienc         = 27; //!< Interpolated GAP block
const unsigned char set_block_arrgap_bienc      = 28; //!< Interpolated GAP array
const unsigned char set_block_arrgap_bienc_inv  = 29; //!< Interpolated GAP array (inverted)
const unsigned char set_block_arrbit_inv        = 30; //!< List of bits OFF
const unsigned char set_block_arr_bienc         = 31; //!< Interpolated block as int array
const unsigned char set_block_arr_bienc_inv     = 32; //!< Interpolated inverted block int array
const unsigned char set_block_bitgap_bienc      = 33; //!< Interpolated bit-block as GAPs
const unsigned char set_block_bit_digest0       = 34; //!< H-compression with digest mask




template<class BV>
serializer<BV>::serializer(const allocator_type&   alloc,
                           bm::word_t*             temp_block)
: alloc_(alloc),
  compression_stat_(0),
  gap_serial_(false),
  byte_order_serial_(true),
  compression_level_(bm::set_compression_default)
{
    bit_idx_arr_.resize(65536);
    if (temp_block == 0)
    {
        temp_block_ = alloc_.alloc_bit_block();
        own_temp_block_ = true;
    }
    else
    {
        temp_block_ = temp_block;
        own_temp_block_ = false;
    }
    compression_stat_ = (size_type*) alloc_.alloc_bit_block();
    optimize_ = free_ = false;
}

template<class BV>
serializer<BV>::serializer(bm::word_t*    temp_block)
: alloc_(allocator_type()),
  compression_stat_(0),
  gap_serial_(false),
  byte_order_serial_(true),
  compression_level_(bm::set_compression_default)
{
    bit_idx_arr_.resize(bm::gap_max_bits);
    if (temp_block == 0)
    {
        temp_block_ = alloc_.alloc_bit_block();
        own_temp_block_ = true;
    }
    else
    {
        temp_block_ = temp_block;
        own_temp_block_ = false;
    }
    compression_stat_ = (size_type*) alloc_.alloc_bit_block();
    optimize_ = free_ = false;
}

template<class BV>
serializer<BV>::~serializer()
{
    if (own_temp_block_)
        alloc_.free_bit_block(temp_block_);
    if (compression_stat_)
        alloc_.free_bit_block((bm::word_t*)compression_stat_);
}


template<class BV>
void serializer<BV>::reset_compression_stats()
{
    for (unsigned i = 0; i < 256; ++i)
        compression_stat_[i] = 0;
}


template<class BV>
void serializer<BV>::set_compression_level(unsigned clevel)
{
    if (clevel <= bm::set_compression_max)
        compression_level_ = clevel;
}

template<class BV>
void serializer<BV>::gap_length_serialization(bool value)
{
    gap_serial_ = value;
}

template<class BV>
void serializer<BV>::byte_order_serialization(bool value)
{
    byte_order_serial_ = value;
}

template<class BV>
void serializer<BV>::encode_header(const BV& bv, bm::encoder& enc)
{
    const blocks_manager_type& bman = bv.get_blocks_manager();

    unsigned char header_flag = 0;
    if (bv.size() == bm::id_max) // no dynamic resize
        header_flag |= BM_HM_DEFAULT;
    else 
        header_flag |= BM_HM_RESIZE;

    if (!byte_order_serial_) 
        header_flag |= BM_HM_NO_BO;

    if (!gap_serial_) 
        header_flag |= BM_HM_NO_GAPL;

    #ifdef BM64ADDR
        header_flag |= BM_HM_64_BIT;
    #endif

    enc.put_8(header_flag);

    if (byte_order_serial_)
    {
        ByteOrder bo = globals<true>::byte_order();
        enc.put_8((unsigned char)bo);
    }
    // keep GAP levels information
    if (gap_serial_)
    {
        enc.put_16(bman.glen(), bm::gap_levels);
    }

    // save size (only if bvector has been down-sized)
    if (header_flag & BM_HM_RESIZE) 
    {
    #ifdef BM64ADDR
        enc.put_64(bv.size());
    #else
        enc.put_32(bv.size());
    #endif
    }
    
}

template<class BV>
void serializer<BV>::interpolated_encode_gap_block(
            const bm::gap_word_t* gap_block, bm::encoder& enc)
{
    unsigned len = bm::gap_length(gap_block);
    if (len > 3) // Use Elias Gamma encoding
    {
        encoder::position_type enc_pos0 = enc.get_pos();
        
        bm::gap_word_t min_v = gap_block[1];
        
        enc.put_8(bm::set_block_gap_bienc);
        enc.put_16(gap_block[0]); // gap header word
        enc.put_16(min_v);        // first word
        
        bit_out_type bout(enc);
        BM_ASSERT(gap_block[len-1] == 65535);
        bout.bic_encode_u16(&gap_block[2], len-3, min_v, 65535);
        bout.flush();
        
        // re-evaluate coding efficiency
        //
        encoder::position_type enc_pos1 = enc.get_pos();
        unsigned gamma_size = (unsigned)(enc_pos1 - enc_pos0);
        if (gamma_size > (len-1)*sizeof(gap_word_t))
        {
            enc.set_pos(enc_pos0);
        }
        else
        {
            compression_stat_[bm::set_block_gap_bienc]++;
            return;
        }
    }
    // save as plain GAP block
    enc.put_8(bm::set_block_gap);
    enc.put_16(gap_block, len-1);
    
    compression_stat_[bm::set_block_gap]++;
}


template<class BV>
void serializer<BV>::gamma_gap_block(const bm::gap_word_t* gap_block, bm::encoder& enc)
{
    unsigned len = gap_length(gap_block);
    if (len > 3 && (compression_level_ > 3)) // Use Elias Gamma encoding
    {
        encoder::position_type enc_pos0 = enc.get_pos();
        {
            bit_out_type bout(enc);
            gamma_encoder_func gamma(bout);

            enc.put_8(bm::set_block_gap_egamma);
            enc.put_16(gap_block[0]);

            for_each_dgap(gap_block, gamma);        
        }
        // re-evaluate coding efficiency
        //
        encoder::position_type enc_pos1 = enc.get_pos();
        unsigned gamma_size = (unsigned)(enc_pos1 - enc_pos0);        
        if (gamma_size > (len-1)*sizeof(gap_word_t))
        {
            enc.set_pos(enc_pos0);
        }
        else
        {
            compression_stat_[bm::set_block_gap_egamma]++;
            return;
        }
    }

    // save as plain GAP block 
    enc.put_8(bm::set_block_gap);
    enc.put_16(gap_block, len-1);
    
    compression_stat_[bm::set_block_gap]++;
}

template<class BV>
void serializer<BV>::gamma_gap_array(const bm::gap_word_t* gap_array, 
                                     unsigned              arr_len, 
                                     bm::encoder&          enc,
                                     bool                  inverted)
{
    unsigned char scode = inverted ? bm::set_block_arrgap_egamma_inv
                                   : bm::set_block_arrgap_egamma;
    if (compression_level_ > 3 && arr_len > 1)
    {        
        encoder::position_type enc_pos0 = enc.get_pos();
        {
            bit_out_type bout(enc);
            enc.put_8(scode);
            bout.gamma(arr_len);
            gap_word_t prev = gap_array[0];
            bout.gamma(prev + 1);

            for (unsigned i = 1; i < arr_len; ++i)
            {
                gap_word_t curr = gap_array[i];
                bout.gamma(curr - prev);
                prev = curr;
            }
        }
        encoder::position_type enc_pos1 = enc.get_pos();
        unsigned gamma_size = (unsigned)(enc_pos1 - enc_pos0);
        unsigned plain_size = (unsigned)(sizeof(gap_word_t)+arr_len*sizeof(gap_word_t));
        if (gamma_size >= plain_size)
        {
            enc.set_pos(enc_pos0); // rollback the bit stream
        }
        else
        {
            compression_stat_[scode]++;
            return;
        }
    }
    // save as a plain array
    scode = inverted ? bm::set_block_arrgap_inv : bm::set_block_arrgap;
    enc.put_prefixed_array_16(scode, gap_array, arr_len, true);
    compression_stat_[scode]++;
}

template<class BV>
void serializer<BV>::interpolated_gap_array(const bm::gap_word_t* gap_block,
                                            unsigned              arr_len,
                                            bm::encoder&          enc,
                                            bool                  inverted)
{
    BM_ASSERT(arr_len <= 65535);
    unsigned char scode = inverted ? bm::set_block_arrgap_bienc_inv
                                   : bm::set_block_arrgap_bienc;
    if (arr_len > 4)
    {
        encoder::position_type enc_pos0 = enc.get_pos();
        {
            bit_out_type bout(enc);
            
            bm::gap_word_t min_v = gap_block[0];
            bm::gap_word_t max_v = gap_block[arr_len-1];
            BM_ASSERT(max_v > min_v);

            enc.put_8(scode);
            enc.put_16(min_v);
            enc.put_16(max_v);

            bout.gamma(arr_len-4);
            bout.bic_encode_u16(&gap_block[1], arr_len-2, min_v, max_v);
            bout.flush();
        }
        encoder::position_type enc_pos1 = enc.get_pos();
        unsigned enc_size = (unsigned)(enc_pos1 - enc_pos0);
        unsigned raw_size = (unsigned)(sizeof(gap_word_t)+arr_len*sizeof(gap_word_t));
        if (enc_size >= raw_size)
        {
            enc.set_pos(enc_pos0); // rollback the bit stream
        }
        else
        {
            compression_stat_[scode]++;
            return;
        }
    }
    // save as a plain array
    scode = inverted ? bm::set_block_arrgap_inv : bm::set_block_arrgap;
    enc.put_prefixed_array_16(scode, gap_block, arr_len, true);
    compression_stat_[scode]++;
}

template<class BV>
void serializer<BV>::add_model(unsigned char mod, unsigned score)
{
    BM_ASSERT(mod_size_ < 64); // too many models (memory corruption?)
    scores_[mod_size_] = score; models_[mod_size_] = mod;
    ++mod_size_;
}

template<class BV>
unsigned char serializer<BV>::find_bit_best_encoding_l5(const bm::word_t* block)
{
    unsigned bc, bit_gaps;
    
    add_model(bm::set_block_bit, bm::gap_max_bits); // default model (bit-block)
    
    bit_model_0run_size_ = bm::bit_count_nonzero_size(block, bm::set_block_size);
    add_model(bm::set_block_bit_0runs, bit_model_0run_size_ * 8);

    bm::id64_t d0 = digest0_ = bm::calc_block_digest0(block);
    if (!d0)
    {
        add_model(bm::set_block_azero, 0);
        return bm::set_block_azero;
    }
    unsigned d0_bc = word_bitcount64(d0);
    bit_model_d0_size_ = unsigned(8 + (32 * d0_bc * sizeof(bm::word_t)));
    if (d0 != ~0ull)
        add_model(bm::set_block_bit_digest0, bit_model_d0_size_ * 8);


    bm::bit_block_change_bc32(block, &bit_gaps, &bc);
    BM_ASSERT(bm::bit_block_count(block) == bc);
    BM_ASSERT(bm::bit_block_calc_change(block) == bit_gaps);

    if (bc == 1)
    {
        add_model(bm::set_block_bit_1bit, 16);
        return bm::set_block_bit_1bit;
    }
    unsigned inverted_bc = bm::gap_max_bits - bc;
    if (!inverted_bc)
    {
        add_model(bm::set_block_aone, 0);
        return bm::set_block_aone;
    }
    unsigned arr_size =
        unsigned(sizeof(gap_word_t) + (bc * sizeof(gap_word_t)));
    unsigned arr_size_inv =
        unsigned(sizeof(gap_word_t) + (inverted_bc * sizeof(gap_word_t)));

    add_model(bm::set_block_arrbit, arr_size*8);
    add_model(bm::set_block_arrbit_inv, arr_size_inv*8);
    const unsigned bie_bits_per_int = 4;

    if (bit_gaps > 3 && bit_gaps < bm::gap_max_buff_len)
        add_model(bm::set_block_gap_bienc,
                  32 + (bit_gaps-1) * bie_bits_per_int);
    if (bc < bit_gaps && bc < bm::gap_equiv_len)
        add_model(bm::set_block_arrgap_bienc, 16*3 + bc*bie_bits_per_int);
    else
    if (inverted_bc < bit_gaps && inverted_bc < bm::gap_equiv_len)
        add_model(bm::set_block_arrgap_bienc_inv, 16*3 + inverted_bc*bie_bits_per_int);
    else
    if (bc >= bm::gap_equiv_len && bc < bie_cut_off)
        add_model(bm::set_block_arr_bienc, 16*3 + bc * bie_bits_per_int);
    else
    if (inverted_bc > 3 && inverted_bc >= bm::gap_equiv_len && inverted_bc < bie_cut_off)
        add_model(bm::set_block_arr_bienc_inv, 16*3 + inverted_bc * bie_bits_per_int);

    if (bit_gaps >= bm::gap_max_buff_len && bit_gaps < bie_cut_off)
        add_model(bm::set_block_bitgap_bienc, 16*4 + (bit_gaps-2) * bie_bits_per_int);

    // find the best representation based on computed approx.models
    //
    unsigned min_score = bm::gap_max_bits;
    unsigned char model = bm::set_block_bit;
    for (unsigned i = 0; i < mod_size_; ++i)
    {
        if (scores_[i] < min_score)
        {
            min_score = scores_[i];
            model = models_[i];
        }
    }
    return model;


}

template<class BV>
unsigned char serializer<BV>::find_bit_best_encoding(const bm::word_t* block)
{
    reset_models();
    
    if (compression_level_ >= 5)
        return find_bit_best_encoding_l5(block);
    
    unsigned bc, bit_gaps;
    
    // heuristics and hard-coded rules to determine
    // the best representation for bit-block
    //
    add_model(bm::set_block_bit, bm::gap_max_bits); // default model (bit-block)
    
    if (compression_level_ <= 1)
        return bm::set_block_bit;

    // check if it is a very sparse block with some areas of dense areas
    bit_model_0run_size_ = bm::bit_count_nonzero_size(block, bm::set_block_size);
    if (compression_level_ <= 5)
        add_model(bm::set_block_bit_0runs, bit_model_0run_size_ * 8);
    
    if (compression_level_ >= 2)
    {
        bm::id64_t d0 = digest0_ = bm::calc_block_digest0(block);
        if (!d0)
        {
            add_model(bm::set_block_azero, 0);
            return bm::set_block_azero;
        }
        unsigned d0_bc = word_bitcount64(d0);
        bit_model_d0_size_ = unsigned(8 + (32 * d0_bc * sizeof(bm::word_t)));
        if (d0 != ~0ull)
            add_model(bm::set_block_bit_digest0, bit_model_d0_size_ * 8);

        if (compression_level_ >= 4)
        {
            bm::bit_block_change_bc32(block, &bit_gaps, &bc);
        }
        else
        {
            bc = bm::bit_block_count(block);
            bit_gaps = 65535;
        }
        BM_ASSERT(bc);

        if (bc == 1)
        {
            add_model(bm::set_block_bit_1bit, 16);
            return bm::set_block_bit_1bit;
        }
        unsigned inverted_bc = bm::gap_max_bits - bc;
        if (!inverted_bc)
        {
            add_model(bm::set_block_aone, 0);
            return bm::set_block_aone;
        }
        
        if (compression_level_ >= 3)
        {
            unsigned arr_size =
                unsigned(sizeof(gap_word_t) + (bc * sizeof(gap_word_t)));
            unsigned arr_size_inv =
                unsigned(sizeof(gap_word_t) + (inverted_bc * sizeof(gap_word_t)));
            
            add_model(bm::set_block_arrbit, arr_size*8);
            add_model(bm::set_block_arrbit_inv, arr_size_inv*8);
            
            if (compression_level_ >= 4)
            {
                const unsigned gamma_bits_per_int = 6;
                //unsigned bit_gaps = bm::bit_block_calc_change(block);

                if (compression_level_ == 4)
                {
                    if (bit_gaps > 3 && bit_gaps < bm::gap_max_buff_len)
                        add_model(bm::set_block_gap_egamma,
                                  16 + (bit_gaps-1) * gamma_bits_per_int);
                    if (bc < bit_gaps && bc < bm::gap_equiv_len)
                        add_model(bm::set_block_arrgap_egamma,
                                  16 + bc * gamma_bits_per_int);
                    if (inverted_bc > 3 && inverted_bc < bit_gaps && inverted_bc < bm::gap_equiv_len)
                        add_model(bm::set_block_arrgap_egamma_inv,
                                  16 + inverted_bc * gamma_bits_per_int);
                }
            } // level >= 3
        } // level >= 3
    } // level >= 2
    
    // find the best representation based on computed approx.models
    //
    unsigned min_score = bm::gap_max_bits;
    unsigned char model = bm::set_block_bit;
    for (unsigned i = 0; i < mod_size_; ++i)
    {
        if (scores_[i] < min_score)
        {
            min_score = scores_[i];
            model = models_[i];
        }
    }
    return model;
}

template<class BV>
unsigned char
serializer<BV>::find_gap_best_encoding(const bm::gap_word_t* gap_block)
{
    // heuristics and hard-coded rules to determine
    // the best representation for d-GAP block
    //
    if (compression_level_ <= 2)
        return bm::set_block_gap;
    unsigned len = bm::gap_length(gap_block);
    unsigned bc = bm::gap_bit_count_unr(gap_block);
    if (bc == 1)
        return bm::set_block_bit_1bit;
    if (bc < len)
    {
        if (compression_level_ < 4)
            return bm::set_block_arrgap;
        if (compression_level_ == 4)
            return bm::set_block_arrgap_egamma;
        return bm::set_block_arrgap_bienc;
    }
    unsigned inverted_bc = bm::gap_max_bits - bc;
    if (inverted_bc < len)
    {
        if (compression_level_ < 4)
            return bm::set_block_arrgap_inv;
        if (compression_level_ == 4)
            return bm::set_block_arrgap_egamma_inv;
        return bm::set_block_arrgap_bienc_inv;
    }
    if (len < 6)
    {
        return bm::set_block_gap;
    }

    if (compression_level_ == 4)
        return bm::set_block_gap_egamma;
    return bm::set_block_gap_bienc;
}




template<class BV>
void serializer<BV>::encode_gap_block(const bm::gap_word_t* gap_block, bm::encoder& enc)
{
    gap_word_t*  gap_temp_block = (gap_word_t*) temp_block_;
    
    gap_word_t arr_len;
    bool invert = false;

    unsigned char enc_choice = find_gap_best_encoding(gap_block);
    switch (enc_choice)
    {
    case bm::set_block_gap:
        gamma_gap_block(gap_block, enc); // TODO: use plain encode (non-gamma)
        break;
        
    case bm::set_block_bit_1bit:
        arr_len = gap_convert_to_arr(gap_temp_block,
                                     gap_block,
                                     bm::gap_equiv_len-10);
        BM_ASSERT(arr_len == 1);
        enc.put_8(bm::set_block_bit_1bit);
        enc.put_16(gap_temp_block[0]);
        compression_stat_[bm::set_block_bit_1bit]++;
        break;
    case bm::set_block_arrgap_inv:
    case bm::set_block_arrgap_egamma_inv:
        invert = true;
        BM_FALLTHROUGH;
        // fall through
    case bm::set_block_arrgap:
        BM_FALLTHROUGH;
        // fall through
    case bm::set_block_arrgap_egamma:
        arr_len = gap_convert_to_arr(gap_temp_block,
                                     gap_block,
                                     bm::gap_equiv_len-10,
                                     invert);
        BM_ASSERT(arr_len);
        gamma_gap_array(gap_temp_block, arr_len, enc, invert);
        break;
    case bm::set_block_gap_bienc:
        interpolated_encode_gap_block(gap_block, enc);
        break;
    case bm::set_block_arrgap_bienc_inv:
        invert = true;
        BM_FALLTHROUGH;
        // fall through
    case bm::set_block_arrgap_bienc:
        arr_len = gap_convert_to_arr(gap_temp_block,
                                     gap_block,
                                     bm::gap_equiv_len-64,
                                     invert);
        BM_ASSERT(arr_len);
        interpolated_gap_array(gap_temp_block, arr_len, enc, invert);
        break;
    default:
        gamma_gap_block(gap_block, enc);
    } // switch
}

template<class BV>
void serializer<BV>::encode_bit_interval(const bm::word_t* blk, 
                                         bm::encoder&      enc,
                                         unsigned          //size_control
                                         )
{
    enc.put_8(bm::set_block_bit_0runs);
    enc.put_8((blk[0]==0) ? 0 : 1); // encode start
    
    unsigned i, j;
    for (i = 0; i < bm::set_block_size; ++i)
    {
        if (blk[i] == 0)
        {
            // scan fwd to find 0 island length
            for (j = i+1; j < bm::set_block_size; ++j)
            {
                if (blk[j] != 0)
                    break;
            }
            BM_ASSERT(j-i);
            enc.put_16((gap_word_t)(j-i)); 
            i = j - 1;
        }
        else
        {
            // scan fwd to find non-0 island length
            for (j = i+1; j < bm::set_block_size; ++j)
            {
                if (blk[j] == 0)
                {
                    // look ahead to identify and ignore short 0-run
                    if (((j+1 < bm::set_block_size) && blk[j+1]) ||
                        ((j+2 < bm::set_block_size) && blk[j+2]))
                    {
                        ++j; // skip zero word
                        continue;
                    }
                    break;
                }
            }
            BM_ASSERT(j-i);
            enc.put_16((gap_word_t)(j-i));
            enc.put_32(blk + i, j - i); // stream all bit-words now

            i = j - 1;
        }
    }
    compression_stat_[bm::set_block_bit_0runs]++;
}


template<class BV>
void serializer<BV>::encode_bit_digest(const bm::word_t* block,
                                       bm::encoder&     enc,
                                       bm::id64_t       d0)
{
    // evaluate a few "sure" models here and pick the best
    //
    if (d0 != ~0ull)
    {
        if (bit_model_0run_size_ < bit_model_d0_size_)
        {
            encode_bit_interval(block, enc, 0); // TODO: get rid of param 3 (0)
            return;
        }
        
        // encode using digest0 method
        //
        enc.put_8(bm::set_block_bit_digest0);
        enc.put_64(d0);

        while (d0)
        {
            bm::id64_t t = bm::bmi_blsi_u64(d0); // d & -d;
            
            unsigned wave = bm::word_bitcount64(t - 1);
            unsigned off = wave * bm::set_block_digest_wave_size;

            unsigned j = 0;
            do
            {
                enc.put_32(block[off+j+0]);
                enc.put_32(block[off+j+1]);
                enc.put_32(block[off+j+2]);
                enc.put_32(block[off+j+3]);
                j += 4;
            } while (j < bm::set_block_digest_wave_size);
            
            d0 = bm::bmi_bslr_u64(d0); // d &= d - 1;
        } // while
        
        compression_stat_[bm::set_block_bit_digest0]++;
    }
    else
    {
        if (bit_model_0run_size_ < unsigned(bm::set_block_size*sizeof(bm::word_t)))
        {
            encode_bit_interval(block, enc, 0); // TODO: get rid of param 3 (0)
            return;
        }

        enc.put_prefixed_array_32(bm::set_block_bit, block, bm::set_block_size);
        compression_stat_[bm::set_block_bit]++;
    }
}



template<class BV>
void serializer<BV>::serialize(const BV& bv,
                               typename serializer<BV>::buffer& buf,
                               const statistics_type* bv_stat)
{
    statistics_type stat;
    if (!bv_stat)
    {
        bv.calc_stat(&stat);
        bv_stat = &stat;
    }
    
    buf.resize(bv_stat->max_serialize_mem, false); // no-copy resize
    optimize_ = free_ = false;

    size_type slen = this->serialize(bv, buf.data(), buf.size());
    BM_ASSERT(slen <= buf.size()); // or we have a BIG problem with prediction
    BM_ASSERT(slen);
    
    buf.resize(slen);
}

template<class BV>
void serializer<BV>::optimize_serialize_destroy(BV& bv,
                                        typename serializer<BV>::buffer& buf)
{
    statistics_type st;
    optimize_ = free_ = true; // set the destructive mode

    typename bvector_type::mem_pool_guard mp_g_z;
    mp_g_z.assign_if_not_set(pool_, bv);

    bv.optimize(temp_block_, BV::opt_compress, &st);
    serialize(bv, buf, &st);
    
    optimize_ = free_ = false; // restore the default mode
}

template<class BV>
void serializer<BV>::encode_bit_array(const bm::word_t* block,
                                      bm::encoder&      enc,
                                      bool              inverted)
{
    unsigned arr_len;
    unsigned mask = inverted ? ~0u : 0u;
    // TODO: get rid of max bits
    arr_len = bit_convert_to_arr(bit_idx_arr_.data(),
                                 block,
                                 bm::gap_max_bits,
                                 bm::gap_max_bits_cmrz,
                                 mask);
    if (arr_len)
    {
        unsigned char scode =
                    inverted ? bm::set_block_arrbit_inv : bm::set_block_arrbit;
        enc.put_prefixed_array_16(scode, bit_idx_arr_.data(), arr_len, true);
        compression_stat_[scode]++;
        return;
    }
    encode_bit_digest(block, enc, digest0_);
}

template<class BV>
void serializer<BV>::gamma_gap_bit_block(const bm::word_t* block,
                                         bm::encoder&      enc)
{
    unsigned len = bm::bit_to_gap(bit_idx_arr_.data(), block, bm::gap_equiv_len);
    BM_ASSERT(len); (void)len;
    gamma_gap_block(bit_idx_arr_.data(), enc);
}

template<class BV>
void serializer<BV>::gamma_arr_bit_block(const bm::word_t* block,
                                         bm::encoder& enc, bool inverted)
{
    unsigned mask = inverted ? ~0u : 0u;
    unsigned arr_len = bit_convert_to_arr(bit_idx_arr_.data(),
                                          block,
                                          bm::gap_max_bits,
                                          bm::gap_equiv_len,
                                          mask);
    if (arr_len)
    {
        gamma_gap_array(bit_idx_arr_.data(), arr_len, enc, inverted);
        return;
    }
    enc.put_prefixed_array_32(bm::set_block_bit, block, bm::set_block_size);
    compression_stat_[bm::set_block_bit]++;
}

template<class BV>
void serializer<BV>::bienc_arr_bit_block(const bm::word_t* block,
                                        bm::encoder& enc, bool inverted)
{
    unsigned mask = inverted ? ~0u : 0u;
    unsigned arr_len = bit_convert_to_arr(bit_idx_arr_.data(),
                                          block,
                                          bm::gap_max_bits,
                                          bm::gap_equiv_len,
                                          mask);
    if (arr_len)
    {
        interpolated_gap_array(bit_idx_arr_.data(), arr_len, enc, inverted);
        return;
    }
    encode_bit_digest(block, enc, digest0_);
}

template<class BV>
void serializer<BV>::interpolated_gap_bit_block(const bm::word_t* block,
                                                bm::encoder&      enc)
{
    unsigned len = bm::bit_to_gap(bit_idx_arr_.data(), block, bm::gap_max_bits);
    BM_ASSERT(len); (void)len;
    interpolated_encode_gap_block(bit_idx_arr_.data(), enc);
}

template<class BV>
void serializer<BV>::bienc_gap_bit_block(const bm::word_t* block,
                                         bm::encoder& enc)
{
    unsigned len = bm::bit_to_gap(bit_idx_arr_.data(), block, bm::gap_max_bits);
    BM_ASSERT(len); (void)len;
    BM_ASSERT(len <= bie_cut_off);
    
    const unsigned char scode = bm::set_block_bitgap_bienc;

    encoder::position_type enc_pos0 = enc.get_pos();
    {
        bit_out_type bout(enc);
        
        bm::gap_word_t head = (bit_idx_arr_[0] & 1); // isolate start flag
        bm::gap_word_t min_v = bit_idx_arr_[1];

        BM_ASSERT(bit_idx_arr_[len] == 65535);
        BM_ASSERT(bit_idx_arr_[len] > min_v);

        enc.put_8(scode);
        
        enc.put_8((unsigned char)head);
        enc.put_16(bm::gap_word_t(len));
        enc.put_16(min_v);
        bout.bic_encode_u16(&bit_idx_arr_[2], len-2, min_v, 65535);
        bout.flush();
    }
    encoder::position_type enc_pos1 = enc.get_pos();
    unsigned enc_size = (unsigned)(enc_pos1 - enc_pos0);
    unsigned raw_size = sizeof(word_t) * bm::set_block_size;
    if (enc_size >= raw_size)
    {
        enc.set_pos(enc_pos0); // rollback the bit stream
    }
    else
    {
        compression_stat_[scode]++;
        return;
    }
    encode_bit_digest(block, enc, digest0_);
}

template<class BV>
void serializer<BV>::interpolated_arr_bit_block(const bm::word_t* block,
                                                bm::encoder& enc, bool inverted)
{
    unsigned mask = inverted ? ~0u : 0u;
    unsigned arr_len = bit_convert_to_arr(bit_idx_arr_.data(),
                                          block,
                                          bm::gap_max_bits,
                                          bm::gap_max_bits_cmrz,
                                          mask);
    if (arr_len)
    {
        unsigned char scode =
            inverted ? bm::set_block_arr_bienc_inv : bm::set_block_arr_bienc;
        
        encoder::position_type enc_pos0 = enc.get_pos();
        {
            bit_out_type bout(enc);
            
            bm::gap_word_t min_v = bit_idx_arr_[0];
            bm::gap_word_t max_v = bit_idx_arr_[arr_len-1];
            BM_ASSERT(max_v > min_v);

            enc.put_8(scode);
            enc.put_16(min_v);
            enc.put_16(max_v);
            enc.put_16(bm::gap_word_t(arr_len));
            bout.bic_encode_u16(&bit_idx_arr_[1], arr_len-2, min_v, max_v);
            bout.flush();
        }
        encoder::position_type enc_pos1 = enc.get_pos();
        unsigned enc_size = (unsigned)(enc_pos1 - enc_pos0);
        unsigned raw_size = sizeof(word_t) * bm::set_block_size;
        if (enc_size >= raw_size)
        {
            enc.set_pos(enc_pos0); // rollback the bit stream
        }
        else
        {
            if (digest0_ != ~0ull && enc_size > bit_model_d0_size_)
            {
                enc.set_pos(enc_pos0); // rollback the bit stream
            }
            else
            {
                compression_stat_[scode]++;
                return;
            }
        }
    }
    encode_bit_digest(block, enc, digest0_);
}



#define BM_SER_NEXT_GRP(enc, nb, B_1ZERO, B_8ZERO, B_16ZERO, B_32ZERO, B_64ZERO) \
   if (nb == 1u) \
      enc.put_8(B_1ZERO); \
   else if (nb < 256u) \
   { \
      enc.put_8(B_8ZERO); \
      enc.put_8((unsigned char)nb); \
   } \
   else if (nb < 65536u) \
   { \
      enc.put_8(B_16ZERO); \
      enc.put_16((unsigned short)nb); \
   } \
   else if (nb < bm::id_max32) \
   { \
      enc.put_8(B_32ZERO); \
      enc.put_32(unsigned(nb)); \
   } \
   else \
   {\
      enc.put_8(B_64ZERO); \
      enc.put_64(nb); \
   }

#define BM_SET_ONE_BLOCKS(x) \
    {\
         block_idx_type end_block = i + x; \
         for (;i < end_block; ++i) \
            bman.set_block_all_set(i); \
    } \
    --i


template<class BV>
typename serializer<BV>::size_type
serializer<BV>::serialize(const BV& bv,
                          unsigned char* buf, size_t buf_size)
{
    BM_ASSERT(temp_block_);
    
    reset_compression_stats();
    const blocks_manager_type& bman = bv.get_blocks_manager();

    bm::encoder enc(buf, buf_size);  // create the encoder
    encode_header(bv, enc);

    block_idx_type i, j;
    for (i = 0; i < bm::set_total_blocks; ++i)
    {
        unsigned i0, j0;
        bm::get_block_coord(i, i0, j0);
        
        const bm::word_t* blk = bman.get_block(i0, j0);

        // ----------------------------------------------------
        // Empty or ONE block serialization
        //
        // TODO: make a function to check this in ONE pass
        //
        bool flag;
        flag = bm::check_block_zero(blk, false/*shallow check*/);
        if (flag)
        {
        zero_block:
            flag = 1;
            block_idx_type next_nb = bman.find_next_nz_block(i+1, false);
            if (next_nb == bm::set_total_blocks) // no more blocks
            {
                enc.put_8(set_block_azero);
                return (size_type)enc.size();
            }
            block_idx_type nb = next_nb - i;
            
            if (nb > 1 && nb < 128)
            {
                // special (but frequent) case -- GAP delta fits in 7bits
                unsigned char c = (unsigned char)((1u << 7) | nb);
                enc.put_8(c);
            }
            else 
            {
                BM_SER_NEXT_GRP(enc, nb, set_block_1zero,
                                      set_block_8zero, 
                                      set_block_16zero, 
                                      set_block_32zero,
                                      set_block_64zero)
            }
            i = next_nb - 1;
            continue;
        }
        else
        {
            flag = bm::check_block_one(blk, false); // shallow scan
            if (flag)
            {
            full_block:
                flag = 1;
                // Look ahead for similar blocks
                // TODO: optimize search for next 0xFFFF block
                for(j = i+1; j < bm::set_total_blocks; ++j)
                {
                    bm::get_block_coord(j, i0, j0);
                    const bm::word_t* blk_next = bman.get_block(i0, j0);
                    if (flag != bm::check_block_one(blk_next, true)) // deep scan
                       break;
                }
                if (j == bm::set_total_blocks)
                {
                    enc.put_8(set_block_aone);
                    break;
                }
                else
                {
                   block_idx_type nb = j - i;
                   BM_SER_NEXT_GRP(enc, nb, set_block_1one,
                                         set_block_8one, 
                                         set_block_16one, 
                                         set_block_32one,
                                         set_block_64one)
                }
                i = j - 1;
                continue;
            }
        }

        // --------------------------------------------------
        // GAP serialization
        //
        if (BM_IS_GAP(blk))
        {
            // experimental, disabled
            #if 0
            {
                bm::gap_word_t tmp_buf[bm::gap_equiv_len * 3]; // temporary result

                unsigned kb_i, kb_j;
                bool kb_found =
                    bman.find_kgb(blk, i0, j0, &tmp_buf[0], &kb_i, &kb_j);
                if (kb_found) // key-block!
                {
                    BM_ASSERT(bm::gap_length(BMGAP_PTR(blk)) > bm::gap_length(&tmp_buf[0]));
                    encode_gap_block(tmp_buf, enc);
                    continue;
                }
            }
            #endif
            encode_gap_block(BMGAP_PTR(blk), enc);
        }
        else
        {
            // ----------------------------------------------
            // BIT BLOCK serialization
            //
        
            unsigned char model = find_bit_best_encoding(blk);
            switch (model)
            {
            case bm::set_block_bit:
                enc.put_prefixed_array_32(set_block_bit, blk, bm::set_block_size);
                break;
            case bm::set_block_bit_1bit:
            {
                unsigned bit_idx = 0;
                bm::bit_block_find(blk, bit_idx, &bit_idx);
                BM_ASSERT(bit_idx < 65536);
                enc.put_8(bm::set_block_bit_1bit); enc.put_16(bm::short_t(bit_idx));
                compression_stat_[bm::set_block_bit_1bit]++;
                continue;
            }
            break;
            case bm::set_block_azero: // empty block all of the sudden ?
                goto zero_block;
            case bm::set_block_aone:
                goto full_block;
            case bm::set_block_arrbit:
                encode_bit_array(blk, enc, false);
                break;
            case bm::set_block_arrbit_inv:
                encode_bit_array(blk, enc, true);
                break;
            case bm::set_block_gap_egamma:
                gamma_gap_bit_block(blk, enc);
                break;
            case bm::set_block_bit_0runs:
                encode_bit_interval(blk, enc, 0); // TODO: get rid of param 3 (0)
                break;
            case bm::set_block_arrgap_egamma:
                gamma_arr_bit_block(blk, enc, false);
                break;
            case bm::set_block_arrgap_egamma_inv:
                gamma_arr_bit_block(blk, enc, true);
                break;
            case bm::set_block_arrgap_bienc:
                bienc_arr_bit_block(blk, enc, false);
                break;
            case bm::set_block_arrgap_bienc_inv:
                bienc_arr_bit_block(blk, enc, true);
                break;
            case bm::set_block_arr_bienc:
                interpolated_arr_bit_block(blk, enc, false);
                break;
            case bm::set_block_arr_bienc_inv:
                interpolated_arr_bit_block(blk, enc, true); // inverted
                break;
            case bm::set_block_gap_bienc:
                interpolated_gap_bit_block(blk, enc);
                break;
            case bm::set_block_bitgap_bienc:
                bienc_gap_bit_block(blk, enc);
                break;
            case bm::set_block_bit_digest0:
                encode_bit_digest(blk, enc, digest0_);
                break;
            default:
                BM_ASSERT(0); // predictor returned an unknown model
                enc.put_prefixed_array_32(set_block_bit, blk, bm::set_block_size);
            }
        } // bit-block processing
        
        // destructive serialization mode
        //
        if (free_)
        {
            // const cast is ok, because it is a requested mode
            const_cast<blocks_manager_type&>(bman).zero_block(i);
        }
 
    } // for i
    enc.put_8(set_block_end);
    return (size_type)enc.size();
}



/// Bit mask flags for serialization algorithm
/// \ingroup bvserial 
enum serialization_flags {
    BM_NO_BYTE_ORDER = 1,       ///< save no byte-order info (save some space)
    BM_NO_GAP_LENGTH = (1 << 1) ///< save no GAP info (save some space)
};

/*!
   \brief Saves bitvector into memory.

   Function serializes content of the bitvector into memory.
   Serialization adaptively uses compression(variation of GAP encoding) 
   when it is benefitial. 
   
   \param bv - source bvecor
   \param buf - pointer on target memory area. No range checking in the
   function. It is responsibility of programmer to allocate sufficient 
   amount of memory using information from calc_stat function.

   \param temp_block - pointer on temporary memory block. Cannot be 0; 
   If you want to save memory across multiple bvectors
   allocate temporary block using allocate_tempblock and pass it to 
   serialize.
   (Serialize does not deallocate temp_block.)

   \param serialization_flags
   Flags controlling serilization (bit-mask) 
   (use OR-ed serialization flags)

   \ingroup bvserial 

   \return Size of serialization block.
   \sa calc_stat, serialization_flags

*/
/*!
 Serialization format:
 <pre>

 | HEADER | BLOCKS |

 Header structure:
   BYTE : Serialization header (bit mask of BM_HM_*)
   BYTE : Byte order ( 0 - Big Endian, 1 - Little Endian)
   INT16: Reserved (0)
   INT16: Reserved Flags (0)

 </pre>
*/
template<class BV>
size_t serialize(const BV& bv,
                   unsigned char* buf, 
                   bm::word_t*    temp_block = 0,
                   unsigned       serialization_flags = 0)
{
    bm::serializer<BV> bv_serial(bv.get_allocator(), temp_block);
    
    if (serialization_flags & BM_NO_BYTE_ORDER) 
        bv_serial.byte_order_serialization(false);
        
    if (serialization_flags & BM_NO_GAP_LENGTH) 
        bv_serial.gap_length_serialization(false);
    else
        bv_serial.gap_length_serialization(true);

    return bv_serial.serialize(bv, buf, 0);
}

/*!
   @brief Saves bitvector into memory.
   Allocates temporary memory block for bvector.
 
   \param bv - source bvecor
   \param buf - pointer on target memory area. No range checking in the
   function. It is responsibility of programmer to allocate sufficient 
   amount of memory using information from calc_stat function.

   \param serialization_flags
   Flags controlling serilization (bit-mask) 
   (use OR-ed serialization flags)
 
   \ingroup bvserial
*/
template<class BV>
size_t serialize(BV& bv,
                 unsigned char* buf,
                 unsigned  serialization_flags=0)
{
    return bm::serialize(bv, buf, 0, serialization_flags);
}



/*!
    @brief Bitvector deserialization from memory.

    @param bv - target bvector
    @param buf - pointer on memory which keeps serialized bvector
    @param temp_block - pointer on temporary block, 
            if NULL bvector allocates own.
    @return Number of bytes consumed by deserializer.

    Function deserializes bitvector from memory block containig results
    of previous serialization. Function does not remove bits 
    which are currently set. Effectively it means OR logical operation 
    between current bitset and previously serialized one.

    @ingroup bvserial
*/
template<class BV>
size_t deserialize(BV& bv,
                   const unsigned char* buf,
                   bm::word_t* temp_block=0)
{
    ByteOrder bo_current = globals<true>::byte_order();

    bm::decoder dec(buf);
    unsigned char header_flag = dec.get_8();
    ByteOrder bo = bo_current;
    if (!(header_flag & BM_HM_NO_BO))
    {
        bo = (bm::ByteOrder) dec.get_8();
    }

    if (bo_current == bo)
    {
        deserializer<BV, bm::decoder> deserial;
        return deserial.deserialize(bv, buf, temp_block);
    }
    switch (bo_current) 
    {
    case BigEndian:
        {
        deserializer<BV, bm::decoder_big_endian> deserial;
        return deserial.deserialize(bv, buf, temp_block);
        }
    case LittleEndian:
        {
        deserializer<BV, bm::decoder_little_endian> deserial;
        return deserial.deserialize(bv, buf, temp_block);
        }
    default:
        BM_ASSERT(0);
    };
    return 0;
}

template<class DEC>
unsigned deseriaizer_base<DEC>::read_id_list(decoder_type&   decoder, 
		    								 unsigned        block_type, 
				   					         bm::gap_word_t* dst_arr)
{
    typedef bit_in<DEC> bit_in_type;

	gap_word_t len = 0;

    switch (block_type)
    {
    case set_block_bit_1bit:
        dst_arr[0] = decoder.get_16();
		len = 1;
		break;
    case set_block_arrgap:
    case set_block_arrgap_inv:
        len = decoder.get_16();
        decoder.get_16(dst_arr, len);
		break;
    case set_block_arrgap_egamma:
    case set_block_arrgap_egamma_inv:
        {
            bit_in_type bin(decoder);
            len = (gap_word_t)bin.gamma();
            gap_word_t prev = 0;
            for (gap_word_t k = 0; k < len; ++k)
            {
                gap_word_t bit_idx = (gap_word_t)bin.gamma();
                if (k == 0) --bit_idx; // TODO: optimization
                bit_idx = (gap_word_t)(bit_idx + prev);
                prev = bit_idx;
				dst_arr[k] = bit_idx;
            } // for
        }
        break;
    case set_block_arrgap_bienc:
    case set_block_arrgap_bienc_inv:
        {
            bm::gap_word_t min_v = decoder.get_16();
            bm::gap_word_t max_v = decoder.get_16();

            bit_in_type bin(decoder);
            len = bm::gap_word_t(bin.gamma() + 4);
            dst_arr[0] = min_v;
            dst_arr[len-1] = max_v;
            bin.bic_decode_u16(&dst_arr[1], len-2, min_v, max_v);
        }
        break;
    default:
        BM_ASSERT(0);
        #ifndef BM_NO_STL
            throw std::logic_error(err_msg());
        #else
            BM_THROW(BM_ERR_SERIALFORMAT);
        #endif
    }
	return len;
}

template<class DEC>
void deseriaizer_base<DEC>::read_bic_arr(decoder_type& dec, bm::word_t* blk)
{
    BM_ASSERT(!BM_IS_GAP(blk));
    
    typedef bit_in<DEC> bit_in_type;
    bm::gap_word_t min_v = dec.get_16();
    bm::gap_word_t max_v = dec.get_16();
    unsigned arr_len = dec.get_16();
    
    bit_in_type bin(dec);

    if (!IS_VALID_ADDR(blk))
    {
        bin.bic_decode_u16_dry(arr_len-2, min_v, max_v);
        return;
    }
    bm::set_bit(blk, min_v);
    bm::set_bit(blk, max_v);
    bin.bic_decode_u16_bitset(blk, arr_len-2, min_v, max_v);
}

template<class DEC>
void deseriaizer_base<DEC>::read_bic_arr_inv(decoder_type&   decoder, bm::word_t* blk)
{
    // TODO: optimization
    bm::bit_block_set(blk, 0);
    this->read_bic_arr(decoder, blk);
    bm::bit_invert(blk);
}

template<class DEC>
void deseriaizer_base<DEC>::read_bic_gap(decoder_type& dec, bm::word_t* blk)
{
    BM_ASSERT(!BM_IS_GAP(blk));
    
    typedef bit_in<DEC> bit_in_type;

    bm::gap_word_t head = dec.get_8();
    unsigned arr_len = dec.get_16();
    bm::gap_word_t min_v = dec.get_16();
    
    BM_ASSERT(arr_len <= bie_cut_off);

    
    id_array_[0] = head;
    id_array_[1] = min_v;
    id_array_[arr_len] = 65535;
    
    bit_in_type bin(dec);
    bin.bic_decode_u16(&id_array_[2], arr_len-2, min_v, 65535);

    if (!IS_VALID_ADDR(blk))
    {
        return;
    }
    bm::gap_add_to_bitset(blk, id_array_, arr_len);
}

template<class DEC>
void deseriaizer_base<DEC>::read_digest0_block(decoder_type& dec,
                                               bm::word_t*   block)
{
    bm::id64_t d0 = dec.get_64();
    while (d0)
    {
        bm::id64_t t = bm::bmi_blsi_u64(d0); // d & -d;
        
        unsigned wave = bm::word_bitcount64(t - 1);
        unsigned off = wave * bm::set_block_digest_wave_size;
        unsigned j = 0;
        if (!IS_VALID_ADDR(block))
        {
            do
            {
                dec.get_32();
                dec.get_32();
                dec.get_32();
                dec.get_32();
                j += 4;
            } while (j < bm::set_block_digest_wave_size);
        }
        else
        {
            do
            {
                block[off+j+0] |= dec.get_32();
                block[off+j+1] |= dec.get_32();
                block[off+j+2] |= dec.get_32();
                block[off+j+3] |= dec.get_32();
                j += 4;
            } while (j < bm::set_block_digest_wave_size);
        }
        
        d0 = bm::bmi_bslr_u64(d0); // d &= d - 1;
    } // while
}

template<class DEC>
void deseriaizer_base<DEC>::read_0runs_block(decoder_type& dec, bm::word_t* blk)
{
    //TODO: optimization if block exists and it is OR-ed read
    bm::bit_block_set(blk, 0);

    unsigned char run_type = dec.get_8();
    for (unsigned j = 0; j < bm::set_block_size; run_type = !run_type)
    {
        unsigned run_length = dec.get_16();
        if (run_type)
        {
            unsigned run_end = j + run_length;
            BM_ASSERT(run_end <= bm::set_block_size);
            for (;j < run_end; ++j)
            {
                unsigned w = dec.get_32();
                blk[j] = w;
            }
        }
        else
        {
            j += run_length;
        }
    } // for j
}


template<class DEC>
void deseriaizer_base<DEC>::read_gap_block(decoder_type&   decoder, 
                                           unsigned        block_type, 
                                           bm::gap_word_t* dst_block,
                                           bm::gap_word_t& gap_head)
{
    typedef bit_in<DEC> bit_in_type;

    switch (block_type)
    {
    case set_block_gap:
        {
            unsigned len = gap_length(&gap_head);
            --len;
            *dst_block = gap_head;
            decoder.get_16(dst_block+1, len - 1);
            dst_block[len] = gap_max_bits - 1;
        }
        break;
    case set_block_bit_1bit:
        {
			gap_set_all(dst_block, bm::gap_max_bits, 0);
            gap_word_t bit_idx = decoder.get_16();
			gap_add_value(dst_block, bit_idx);
        }
        break;
    case set_block_arrgap:
    case set_block_arrgap_inv:
        {
            gap_set_all(dst_block, bm::gap_max_bits, 0);
            gap_word_t len = decoder.get_16();
            for (gap_word_t k = 0; k < len; ++k)
            {
                gap_word_t bit_idx = decoder.get_16();
				gap_add_value(dst_block, bit_idx);
            } // for
        }
        break;
    case set_block_arrgap_egamma:
    case set_block_arrgap_egamma_inv:
    case set_block_arrgap_bienc:
    case set_block_arrgap_bienc_inv:
        {
        	unsigned arr_len = read_id_list(decoder, block_type, id_array_);
            dst_block[0] = 0;
            unsigned gap_len =
                gap_set_array(dst_block, id_array_, arr_len);
            BM_ASSERT(gap_len == bm::gap_length(dst_block));
            (void)(gap_len);
        }
        break;
    case set_block_gap_egamma:
        {
        unsigned len = (gap_head >> 3);
        --len;
        // read gamma GAP block into a dest block
        {
            *dst_block = gap_head;
            gap_word_t* gap_data_ptr = dst_block + 1;

            bit_in_type bin(decoder);
            {
				gap_word_t v = (gap_word_t)bin.gamma();
                gap_word_t gap_sum = *gap_data_ptr = (gap_word_t)(v - 1);
                for (unsigned i = 1; i < len; ++i)
                {					
                    v = (gap_word_t)bin.gamma();
                    gap_sum = (gap_word_t)(gap_sum + v);
                    *(++gap_data_ptr) = gap_sum;
                }
                dst_block[len+1] = bm::gap_max_bits - 1;
            }
        }

        }
        break;
    case set_block_gap_bienc:
        {
            unsigned len = (gap_head >> 3);
            *dst_block = gap_head;
            bm::gap_word_t min_v = decoder.get_16();
            dst_block[1] = min_v;
            bit_in_type bin(decoder);
            bin.bic_decode_u16(&dst_block[2], len-2, min_v, 65535);
            dst_block[len] = bm::gap_max_bits - 1;
        }
        break;
    default:
        BM_ASSERT(0);
        #ifndef BM_NO_STL
            throw std::logic_error(err_msg());
        #else
            BM_THROW(BM_ERR_SERIALFORMAT);
        #endif
    }

    if (block_type == set_block_arrgap_egamma_inv || 
        block_type == set_block_arrgap_inv ||
        block_type == set_block_arrgap_bienc_inv)
    {
        gap_invert(dst_block);
    }
}

// -------------------------------------------------------------------------

template<class BV, class DEC>
deserializer<BV, DEC>::deserializer()
{
    temp_block_ = alloc_.alloc_bit_block();
    bit_idx_arr_.resize(bm::gap_max_bits);
    this->id_array_ = bit_idx_arr_.data();
    gap_temp_block_.resize(bm::gap_max_bits);
}

template<class BV, class DEC>
deserializer<BV, DEC>::~deserializer()
{
     alloc_.free_bit_block(temp_block_);
}


template<class BV, class DEC>
void 
deserializer<BV, DEC>::deserialize_gap(unsigned char btype, decoder_type& dec, 
                                       bvector_type&  bv, blocks_manager_type& bman,
                                       block_idx_type nb,
                                       bm::word_t* blk)
{
    gap_word_t gap_head; 
    bm::gap_word_t* gap_temp_block = gap_temp_block_.data();
    
    switch (btype)
    {
    case set_block_gap: 
    case set_block_gapbit:
    {
        gap_head = (gap_word_t)
            (sizeof(gap_word_t) == 2 ? dec.get_16() : dec.get_32());

        unsigned len = gap_length(&gap_head);
        int level = gap_calc_level(len, bman.glen());
        --len;
        if (level == -1)  // Too big to be GAP: convert to BIT block
        {
            *gap_temp_block = gap_head;
            dec.get_16(gap_temp_block+1, len - 1);
            gap_temp_block[len] = gap_max_bits - 1;

            if (blk == 0)  // block does not exist yet
            {
                blk = bman.get_allocator().alloc_bit_block();
                bman.set_block(nb, blk);
                gap_convert_to_bitset(blk, gap_temp_block);
            }
            else  // We have some data already here. Apply OR operation.
            {
                gap_convert_to_bitset(temp_block_, 
                                      gap_temp_block);
                bv.combine_operation_with_block(nb,
                                                temp_block_, 
                                                0, 
                                                BM_OR);
            }
            return;
        } // level == -1

        set_gap_level(&gap_head, level);

        if (blk == 0)
        {
            BM_ASSERT(level >= 0);
            gap_word_t* gap_blk = 
              bman.get_allocator().alloc_gap_block(unsigned(level), bman.glen());
            gap_word_t* gap_blk_ptr = BMGAP_PTR(gap_blk);
            *gap_blk_ptr = gap_head;
            bm::set_gap_level(gap_blk_ptr, level);
            blk = bman.set_block(nb, (bm::word_t*)BMPTR_SETBIT0(gap_blk));
            BM_ASSERT(blk == 0);
            
            dec.get_16(gap_blk + 1, len - 1);
            gap_blk[len] = bm::gap_max_bits - 1;
        }
        else // target block exists
        {
            // read GAP block into a temp memory and perform OR
            *gap_temp_block = gap_head;
            dec.get_16(gap_temp_block + 1, len - 1);
            gap_temp_block[len] = bm::gap_max_bits - 1;
            break;
        }
        return;
    }
    case set_block_arrgap: 
    case set_block_arrgap_egamma:
    case set_block_arrgap_bienc:
        {
        	unsigned arr_len = this->read_id_list(dec, btype, this->id_array_);
            gap_temp_block[0] = 0; // reset unused bits in gap header
            unsigned gap_len =
              gap_set_array(gap_temp_block, this->id_array_, arr_len);
            
            BM_ASSERT(gap_len == bm::gap_length(gap_temp_block));
            int level = gap_calc_level(gap_len, bman.glen());
            if (level == -1)  // Too big to be GAP: convert to BIT block
            {
                gap_convert_to_bitset(temp_block_, gap_temp_block);
                bv.combine_operation_with_block(nb,
                                                temp_block_,
                                                0,
                                                BM_OR);
                return;
            }

            break;
        }
    case set_block_gap_egamma:            
        gap_head = dec.get_16();
        BM_FALLTHROUGH;
        // fall through
    case set_block_arrgap_egamma_inv:
        BM_FALLTHROUGH;
        // fall through
    case set_block_arrgap_inv:
    case set_block_arrgap_bienc_inv:
        this->read_gap_block(dec, btype, gap_temp_block, gap_head);
        break;
    case bm::set_block_gap_bienc:
        gap_head = dec.get_16();
        this->read_gap_block(dec, btype, gap_temp_block, gap_head);
        break;
    default:
        BM_ASSERT(0);
        #ifndef BM_NO_STL
            throw std::logic_error(this->err_msg());
        #else
            BM_THROW(BM_ERR_SERIALFORMAT);
        #endif
    }

    bv.combine_operation_with_block(nb,
                                   (bm::word_t*)gap_temp_block,
                                   1, 
                                   BM_OR);
}

template<class BV, class DEC>
void deserializer<BV, DEC>::decode_bit_block(unsigned char btype,
                              decoder_type&        dec,
                              blocks_manager_type& bman,
                              block_idx_type       nb,
                              bm::word_t*          blk)
{
    if (!blk)
    {
        blk = bman.get_allocator().alloc_bit_block();
        bman.set_block(nb, blk);
        bm::bit_block_set(blk, 0);
    }
    else
    if (BM_IS_GAP(blk))
        blk = bman.deoptimize_block(nb);
    
    BM_ASSERT(blk != temp_block_);
    
    switch (btype)
    {
    case set_block_arrbit_inv:
        if (IS_FULL_BLOCK(blk))
            blk = bman.deoptimize_block(nb);
        bm::bit_block_set(temp_block_, ~0u);
        {
            gap_word_t len = dec.get_16();
            for (unsigned k = 0; k < len; ++k)
            {
                gap_word_t bit_idx = dec.get_16();
                bm::clear_bit(temp_block_, bit_idx);
            } // for
        }
        bm::bit_block_or(blk, temp_block_);
        break;
    case bm::set_block_arr_bienc:
        this->read_bic_arr(dec, blk);
        break;
    case bm::set_block_arr_bienc_inv:
        BM_ASSERT(blk != temp_block_);
        if (IS_FULL_BLOCK(blk))
            blk = bman.deoptimize_block(nb);
        // TODO: optimization
        bm::bit_block_set(temp_block_, 0);
        this->read_bic_arr(dec, temp_block_);
        bm::bit_invert(temp_block_);
        bm::bit_block_or(blk, temp_block_);
        break;
    case bm::set_block_bitgap_bienc:
        this->read_bic_gap(dec, blk);
        break;
    case bm::set_block_bit_digest0:
        this->read_digest0_block(dec, blk);
        break;
    default:
        BM_ASSERT(0);
        #ifndef BM_NO_STL
            throw std::logic_error(this->err_msg());
        #else
            BM_THROW(BM_ERR_SERIALFORMAT);
        #endif
    } // switch
}




template<class BV, class DEC>
size_t deserializer<BV, DEC>::deserialize(bvector_type&        bv,
                                          const unsigned char* buf,
                                          bm::word_t*          /*temp_block*/)
{
    blocks_manager_type& bman = bv.get_blocks_manager();
    if (!bman.is_init())
        bman.init_tree();
    
    bm::word_t* temp_block = temp_block_;

    bm::strategy  strat = bv.get_new_blocks_strat();
    bv.set_new_blocks_strat(BM_GAP);

    decoder_type dec(buf);

    // Reading th serialization header
    //
    unsigned char header_flag =  dec.get_8();
    if (!(header_flag & BM_HM_NO_BO))
    {
        /*ByteOrder bo = (bm::ByteOrder)*/dec.get_8();
    }
    if (header_flag & BM_HM_64_BIT)
    {
    #ifndef BM64ADDR
        BM_ASSERT(0); // 64-bit address vector cannot read on 32
        #ifndef BM_NO_STL
            throw std::logic_error(this->err_msg());
        #else
            BM_THROW(BM_ERR_SERIALFORMAT);
        #endif
    #endif
    }

    if (header_flag & BM_HM_ID_LIST)
    {
        // special case: the next comes plain list of integers
        if (header_flag & BM_HM_RESIZE)
        {
            block_idx_type bv_size;
            if (header_flag & BM_HM_64_BIT)
            {
                BM_ASSERT(sizeof(block_idx_type)==8);
                bv_size = (block_idx_type)dec.get_64();
            }
            else
                bv_size = dec.get_32();
            if (bv_size > bv.size())
                bv.resize(bv_size);
        }
        for (unsigned cnt = dec.get_32(); cnt; --cnt)
        {
            bm::id_t idx = dec.get_32();
            bv.set(idx);
        } // for
        // -1 for compatibility with other deserialization branches
        return dec.size()-1;
    }

    block_idx_type i;

    if (!(header_flag & BM_HM_NO_GAPL)) 
    {
        //gap_word_t glevels[bm::gap_levels];
        // read GAP levels information
        for (unsigned k = 0; k < bm::gap_levels; ++k)
        {
            /*glevels[i] =*/ dec.get_16();
        }
    }
    if (header_flag & BM_HM_RESIZE)
    {
        block_idx_type bv_size;
        if (header_flag & BM_HM_64_BIT)
        {
            // 64-bit vector cannot be deserialized into 32-bit
            BM_ASSERT(sizeof(block_idx_type)==8);
            #ifndef BM64ADDR
                #ifndef BM_NO_STL
                    throw std::logic_error(this->err_msg());
                #else
                    BM_THROW(BM_ERR_SERIALFORMAT);
                #endif
            #endif
            bv_size = (block_idx_type)dec.get_64();
        }
        else
            bv_size = dec.get_32();
        if (bv_size > bv.size())
            bv.resize(bv_size);
    }

    unsigned char btype;
    block_idx_type nb;
    unsigned i0, j0;

    for (i = 0; i < bm::set_total_blocks; ++i)
    {
        btype = dec.get_8();
        
        bm::get_block_coord(i, i0, j0);
        bm::word_t* blk = bman.get_block_ptr(i0, j0);
        // pre-check if we have short zero-run packaging here
        //
        if (btype & (1 << 7))
        {
            nb = btype & ~(1 << 7);
            i += nb-1;
            continue;
        }        

        switch (btype)
        {
        case set_block_azero: 
        case set_block_end:
            i = bm::set_total_blocks;
            break;
        case set_block_1zero:
            continue;
        case set_block_8zero:
            nb = dec.get_8();
            i += nb-1;
            continue;
        case set_block_16zero:
            nb = dec.get_16();
            i += nb-1;
            continue;
        case set_block_32zero:
            nb = dec.get_32();
            i += nb-1;
            continue;
        case set_block_64zero:
            #ifdef BM64ADDR
                nb = dec.get_64();
                i += nb-1;
            #else
                BM_ASSERT(0); // attempt to read 64-bit vector in 32-bit mode
                #ifndef BM_NO_STL
                    throw std::logic_error(this->err_msg());
                #else
                    BM_THROW(BM_ERR_SERIALFORMAT);
                #endif
                i = bm::set_total_blocks;
            #endif
            continue;
        case set_block_aone:
            bman.set_all_set(i, bm::set_total_blocks-1);
            i = bm::set_total_blocks;
            break;
        case set_block_1one:
            bman.set_block_all_set(i);
            continue;
        case set_block_8one:
            BM_SET_ONE_BLOCKS(dec.get_8());
            continue;
        case set_block_16one:
            BM_SET_ONE_BLOCKS(dec.get_16());
            continue;
        case set_block_32one:
            BM_SET_ONE_BLOCKS(dec.get_32());
            continue;
        case set_block_64one:
    #ifdef BM64ADDR
            BM_SET_ONE_BLOCKS(dec.get_64());
    #else
            BM_ASSERT(0); // 32-bit vector cannot read 64-bit
            #ifndef BM_NO_STL
                throw std::logic_error(this->err_msg());
            #else
                BM_THROW(BM_ERR_SERIALFORMAT);
            #endif
            dec.get_64();
    #endif
            continue;
        case set_block_bit:
        {
            if (blk == 0)
            {
                blk = bman.get_allocator().alloc_bit_block();
                bman.set_block(i, blk);
                dec.get_32(blk, bm::set_block_size);
                continue;                
            }
			
            dec.get_32(temp_block_, bm::set_block_size);
            bv.combine_operation_with_block(i, 
                                            temp_block,
                                            0, BM_OR);
			
            continue;
        }
        case set_block_bit_1bit:
        {
            size_type bit_idx = dec.get_16();
            bit_idx += i * bm::bits_in_block; 
            bv.set_bit_no_check(bit_idx);
            continue;
        }
        case set_block_bit_0runs:
        {
            //TODO: optimization if block exists
            this->read_0runs_block(dec, temp_block);
            bv.combine_operation_with_block(i,
                                            temp_block,
                                            0, BM_OR);            
            continue;
        }
        case set_block_bit_interval: 
        {
            unsigned head_idx, tail_idx;
            head_idx = dec.get_16();
            tail_idx = dec.get_16();

            if (blk == 0)
            {
                blk = bman.get_allocator().alloc_bit_block();
                bman.set_block(i, blk);
                for (unsigned k = 0; k < head_idx; ++k)
                {
                    blk[k] = 0;
                }
                dec.get_32(blk + head_idx, tail_idx - head_idx + 1);
                for (unsigned j = tail_idx + 1; j < bm::set_block_size; ++j)
                {
                    blk[j] = 0;
                }
                continue;
            }
            bm::bit_block_set(temp_block, 0);
            dec.get_32(temp_block + head_idx, tail_idx - head_idx + 1);

            bv.combine_operation_with_block(i, 
                                            temp_block,
                                            0, BM_OR);
            continue;
        }
        case set_block_gap: 
        case set_block_gapbit:
        case set_block_arrgap:
        case set_block_gap_egamma:
        case set_block_arrgap_egamma:
        case set_block_arrgap_egamma_inv:
        case set_block_arrgap_inv:
        case set_block_gap_bienc:
        case set_block_arrgap_bienc:
        case set_block_arrgap_bienc_inv:
            deserialize_gap(btype, dec, bv, bman, i, blk);
            continue;
        case set_block_arrbit:
        {
            gap_word_t len = dec.get_16();
            if (BM_IS_GAP(blk))
            {
                // convert from GAP cause generic bitblock is faster
                blk = bman.deoptimize_block(i);
            }
            else
            {
                if (!blk)  // block does not exists yet
                {
                    blk = bman.get_allocator().alloc_bit_block();
                    bman.set_block(i, blk);
                    bm::bit_block_set(blk, 0);
                }
                else
                if (IS_FULL_BLOCK(blk)) // nothing to do
                {
                    for (unsigned k = 0; k < len; ++k) // dry read
                    {
                        dec.get_16();
                    }
                    continue;
                }
            }
            
            // Get the array one by one and set the bits.
            for (unsigned k = 0; k < len; ++k)
            {
                gap_word_t bit_idx = dec.get_16();
				bm::set_bit(blk, bit_idx);
            }
            continue;
        }
        case bm::set_block_arr_bienc:
        case bm::set_block_arrbit_inv:
        case bm::set_block_arr_bienc_inv:
        case bm::set_block_bitgap_bienc:
            decode_bit_block(btype, dec, bman, i, blk);
            continue;
        case bm::set_block_bit_digest0:
            decode_bit_block(btype, dec, bman, i, blk);
            continue;
        default:
            BM_ASSERT(0); // unknown block type
            #ifndef BM_NO_STL
                throw std::logic_error(this->err_msg());
            #else
                BM_THROW(BM_ERR_SERIALFORMAT);
            #endif
        } // switch
    } // for i

    bv.set_new_blocks_strat(strat);

    return dec.size();
}

// ---------------------------------------------------------------------------

template<class DEC>
serial_stream_iterator<DEC>::serial_stream_iterator(const unsigned char* buf)
  : decoder_(buf),
    end_of_stream_(false),
    bv_size_(0),
    state_(e_unknown),
    id_cnt_(0),
    block_idx_(0),
    mono_block_cnt_(0),
    block_idx_arr_(0)
{
    ::memset(bit_func_table_, 0, sizeof(bit_func_table_));

    bit_func_table_[bm::set_AND] = 
        &serial_stream_iterator<DEC>::get_bit_block_AND;

    bit_func_table_[bm::set_ASSIGN] = 
        &serial_stream_iterator<DEC>::get_bit_block_ASSIGN;
    bit_func_table_[bm::set_OR]     = 
        &serial_stream_iterator<DEC>::get_bit_block_OR;
    bit_func_table_[bm::set_SUB] = 
        &serial_stream_iterator<DEC>::get_bit_block_SUB;
    bit_func_table_[bm::set_XOR] = 
        &serial_stream_iterator<DEC>::get_bit_block_XOR;
    bit_func_table_[bm::set_COUNT] = 
        &serial_stream_iterator<DEC>::get_bit_block_COUNT;
    bit_func_table_[bm::set_COUNT_AND] = 
        &serial_stream_iterator<DEC>::get_bit_block_COUNT_AND;
    bit_func_table_[bm::set_COUNT_XOR] = 
        &serial_stream_iterator<DEC>::get_bit_block_COUNT_XOR;
    bit_func_table_[bm::set_COUNT_OR] = 
        &serial_stream_iterator<DEC>::get_bit_block_COUNT_OR;
    bit_func_table_[bm::set_COUNT_SUB_AB] = 
        &serial_stream_iterator<DEC>::get_bit_block_COUNT_SUB_AB;
    bit_func_table_[bm::set_COUNT_SUB_BA] = 
        &serial_stream_iterator<DEC>::get_bit_block_COUNT_SUB_BA;
    bit_func_table_[bm::set_COUNT_A] = 
        &serial_stream_iterator<DEC>::get_bit_block_COUNT_A;
    bit_func_table_[bm::set_COUNT_B] = 
        &serial_stream_iterator<DEC>::get_bit_block_COUNT;


    // reading stream header
    unsigned char header_flag =  decoder_.get_8();
    if (!(header_flag & BM_HM_NO_BO))
    {
        /*ByteOrder bo = (bm::ByteOrder)*/decoder_.get_8();
    }

    // check if bitvector comes as an inverted, sorted list of ints
    //
    if (header_flag & BM_HM_ID_LIST)
    {
        // special case: the next comes plain list of unsigned integers
        if (header_flag & BM_HM_RESIZE)
        {
            if (header_flag & BM_HM_64_BIT)
            {
                BM_ASSERT(sizeof(block_idx_type)==8);
                bv_size_ = (block_idx_type)decoder_.get_64();
            }
            else
                bv_size_ = decoder_.get_32();
        }

        state_ = e_list_ids;
        id_cnt_ = decoder_.get_32();
        next(); // read first id
    }
    else
    {
        if (!(header_flag & BM_HM_NO_GAPL))
        {
            for (unsigned i = 0; i < bm::gap_levels; ++i) // load the GAP levels
                glevels_[i] = decoder_.get_16();
        }
        if (header_flag & BM_HM_RESIZE)
        {
            if (header_flag & BM_HM_64_BIT)
            {
                BM_ASSERT(sizeof(block_idx_type)==8);
                bv_size_ = (block_idx_type)decoder_.get_64();
            }
            else
                bv_size_ = decoder_.get_32();
        }
        state_ = e_blocks;
    }
    block_idx_arr_ = (gap_word_t*) ::malloc(sizeof(gap_word_t) * bm::gap_max_bits);
    this->id_array_ = block_idx_arr_;
}

template<class DEC>
serial_stream_iterator<DEC>::~serial_stream_iterator()
{
    if (block_idx_arr_)
        ::free(block_idx_arr_);
}


template<class DEC>
void serial_stream_iterator<DEC>::next()
{
    if (is_eof())
    {
        ++block_idx_;
        return;
    }

    switch (state_) 
    {
    case e_list_ids:
        // read inverted ids one by one
        if (id_cnt_ == 0)
        {
            end_of_stream_ = true;
            state_ = e_unknown;
            break;
        }
        last_id_ = decoder_.get_32();
        --id_cnt_;
        break;

    case e_blocks:
        if (block_idx_ == bm::set_total_blocks)
        {
            end_of_stream_ = true;
            state_ = e_unknown;
            break;
        }

        block_type_ = decoder_.get_8();

        // pre-check for 7-bit zero block
        //
        if (block_type_ & (1u << 7u))
        {
            mono_block_cnt_ = (block_type_ & ~(1u << 7u)) - 1;
            state_ = e_zero_blocks;
            break;
        }

        switch (block_type_)
        {
        case set_block_azero:
        case set_block_end:
            end_of_stream_ = true; state_ = e_unknown;
            break;
        case set_block_1zero:
            state_ = e_zero_blocks;
            mono_block_cnt_ = 0;
            break;
        case set_block_8zero:
            state_ = e_zero_blocks;
            mono_block_cnt_ = decoder_.get_8()-1;
            break;
        case set_block_16zero:
            state_ = e_zero_blocks;
            mono_block_cnt_ = decoder_.get_16()-1;
            break;
        case set_block_32zero:
            state_ = e_zero_blocks;
            mono_block_cnt_ = decoder_.get_32()-1;
            break;
        case set_block_aone:
            state_ = e_one_blocks;
            mono_block_cnt_ = bm::set_total_blocks - block_idx_;
            break;
        case set_block_1one:
            state_ = e_one_blocks;
            mono_block_cnt_ = 0;
            break;
        case set_block_8one:
            state_ = e_one_blocks;
            mono_block_cnt_ = decoder_.get_8()-1;
            break;
        case set_block_16one:
            state_ = e_one_blocks;
            mono_block_cnt_ = decoder_.get_16()-1;
            break;
        case set_block_32one:
            state_ = e_one_blocks;
            mono_block_cnt_ = decoder_.get_32()-1;
            break;

        case bm::set_block_bit:
        case bm::set_block_bit_interval:
        case bm::set_block_bit_0runs:
        case bm::set_block_arrbit:
        case bm::set_block_arrbit_inv:
        case bm::set_block_arr_bienc:
        case bm::set_block_arr_bienc_inv:
        case bm::set_block_bitgap_bienc:
        case bm::set_block_bit_digest0:
            state_ = e_bit_block;
            break;
        case set_block_gap:
            BM_FALLTHROUGH;
            // fall through
        case set_block_gap_egamma:
            BM_FALLTHROUGH;
            // fall through
        case set_block_gap_bienc:
            gap_head_ = decoder_.get_16();
            BM_FALLTHROUGH;
            // fall through
        case set_block_arrgap:
            BM_FALLTHROUGH;
            // fall through
        case set_block_arrgap_egamma:
            BM_FALLTHROUGH;
            // fall through
        case set_block_arrgap_egamma_inv:
            BM_FALLTHROUGH;
            // fall through
        case set_block_arrgap_inv:
            BM_FALLTHROUGH;
            // fall through
		case set_block_bit_1bit:
            BM_FALLTHROUGH;
            // fall through
        case set_block_arrgap_bienc:
            BM_FALLTHROUGH;
            // fall through
        case set_block_arrgap_bienc_inv:
            state_ = e_gap_block;
            break;        
        case set_block_gapbit:
            state_ = e_gap_block; //e_bit_block; // TODO: make a better decision here
            break;
        default:
            BM_ASSERT(0);
            #ifndef BM_NO_STL
                throw std::logic_error(this->err_msg());
            #else
                BM_THROW(BM_ERR_SERIALFORMAT);
            #endif
        } // switch

        break;

    case e_zero_blocks:
    case e_one_blocks:
        ++block_idx_;
        if (!mono_block_cnt_)
        {
            state_ = e_blocks; // get new block token
            break;
        }
        --mono_block_cnt_;
        break;

    case e_unknown:
    default:
        BM_ASSERT(0);
        #ifndef BM_NO_STL
            throw std::logic_error(this->err_msg());
        #else
            BM_THROW(BM_ERR_SERIALFORMAT);
        #endif
    } // switch
}

template<class DEC>
typename serial_stream_iterator<DEC>::block_idx_type
serial_stream_iterator<DEC>::skip_mono_blocks()
{
	BM_ASSERT(state_ == e_zero_blocks || state_ == e_one_blocks);
    if (!mono_block_cnt_)
		++block_idx_;
	else
	{
		block_idx_ += mono_block_cnt_+1;
		mono_block_cnt_ = 0;
	}
    state_ = e_blocks;
    return block_idx_;
}

template<class DEC>
void serial_stream_iterator<DEC>::get_inv_arr(bm::word_t* block)
{
    gap_word_t len = decoder_.get_16();
    if (block)
    {
        bm::bit_block_set(block, ~0u);
        for (unsigned k = 0; k < len; ++k)
        {
            gap_word_t bit_idx = decoder_.get_16();
            bm::clear_bit(block, bit_idx);
        }
    }
    else // dry read
    {
        for (unsigned k = 0; k < len; ++k)
            decoder_.get_16();
    }
}


template<class DEC>
unsigned 
serial_stream_iterator<DEC>::get_bit_block_ASSIGN(
                                            bm::word_t*  dst_block,
                                            bm::word_t*  tmp_block)
{
    //  ASSIGN should be ready for dry read (dst_block can be NULL)
    //
    BM_ASSERT(this->state_ == e_bit_block);
    unsigned count = 0;

    switch (this->block_type_)
    {
    case set_block_bit:
        decoder_.get_32(dst_block, bm::set_block_size);
        break;
    case set_block_bit_0runs: 
        {
        if (IS_VALID_ADDR(dst_block))
            bm::bit_block_set(dst_block, 0);
        unsigned char run_type = decoder_.get_8();
        for (unsigned j = 0; j < bm::set_block_size;run_type = !run_type)
        {
            unsigned run_length = decoder_.get_16();
            if (run_type)
            {
				decoder_.get_32(dst_block ? dst_block + j : dst_block, run_length);
			}
			j += run_length;
        } // for
        }
        break;
    case set_block_bit_interval:
        {
            unsigned head_idx = decoder_.get_16();
            unsigned tail_idx = decoder_.get_16();
            if (dst_block) 
            {
                for (unsigned i = 0; i < head_idx; ++i)
                    dst_block[i] = 0;
                decoder_.get_32(dst_block + head_idx, 
                                tail_idx - head_idx + 1);
                for (unsigned j = tail_idx + 1; j < bm::set_block_size; ++j)
                    dst_block[j] = 0;
            }
            else
            {
                int pos = int(tail_idx - head_idx) + 1;
                pos *= 4u;
                decoder_.seek(pos);
            }
        }
        break;
    case set_block_arrbit:
    case set_block_bit_1bit:
        get_arr_bit(dst_block, true /*clear target*/);
        break;
    case set_block_gapbit:
        BM_ASSERT(0);
        #ifndef BM_NO_STL
            throw std::logic_error(this->err_msg());
        #else
            BM_THROW(BM_ERR_SERIALFORMAT);
        #endif
        break;
    case set_block_arrbit_inv:
        get_inv_arr(dst_block);
        break;
    case bm::set_block_arr_bienc:
        if (IS_VALID_ADDR(dst_block))
            bm::bit_block_set(dst_block, 0);
        this->read_bic_arr(decoder_, dst_block);
        break;
    case bm::set_block_arr_bienc_inv:
        this->read_bic_arr_inv(decoder_, tmp_block);
        if (IS_VALID_ADDR(dst_block))
            bm::bit_block_copy(dst_block, tmp_block);
        break;
    case bm::set_block_bitgap_bienc:
        if (IS_VALID_ADDR(dst_block))
            bm::bit_block_set(dst_block, 0);
        this->read_bic_gap(decoder_, dst_block);
        break;
    case bm::set_block_bit_digest0:
        if (IS_VALID_ADDR(dst_block))
            bm::bit_block_set(dst_block, 0);
        this->read_digest0_block(decoder_, dst_block);
        break;
    default:
        BM_ASSERT(0);
        #ifndef BM_NO_STL
            throw std::logic_error(this->err_msg());
        #else
            BM_THROW(BM_ERR_SERIALFORMAT);
        #endif
    } // switch
    return count;
}

template<class DEC>
unsigned 
serial_stream_iterator<DEC>::get_bit_block_OR(bm::word_t*  dst_block,
                                              bm::word_t*  tmp_block)
{
    BM_ASSERT(this->state_ == e_bit_block);
    unsigned count = 0;
    switch (block_type_)
    {
    case set_block_bit:
        decoder_.get_32_OR(dst_block, bm::set_block_size);
        break;
    case set_block_bit_interval:
        {
        unsigned head_idx = decoder_.get_16();
        unsigned tail_idx = decoder_.get_16();
        for (unsigned i = head_idx; i <= tail_idx; ++i)
            dst_block[i] |= decoder_.get_32();
        }
        break;
    case set_block_bit_0runs:
        {
        unsigned char run_type = decoder_.get_8();
        for (unsigned j = 0; j < bm::set_block_size;run_type = !run_type)
        {
            unsigned run_length = decoder_.get_16();
            if (run_type)
            {
                unsigned run_end = j + run_length;
                for (;j < run_end; ++j)
                {
                    BM_ASSERT(j < bm::set_block_size);
                    dst_block[j] |= decoder_.get_32();
                }
            }
            else
            {
                j += run_length;
            }
        } // for
        }
        break;
    case set_block_bit_1bit:
    case set_block_arrbit:
        get_arr_bit(dst_block, false /*don't clear target*/);
        break;
    case set_block_arrbit_inv:
        get_inv_arr(tmp_block);
        bm::bit_block_or(dst_block, tmp_block);
        break;
    case bm::set_block_arr_bienc:
        this->read_bic_arr(decoder_, dst_block);
        break;
    case bm::set_block_arr_bienc_inv:
        this->read_bic_arr_inv(decoder_, tmp_block);
        bm::bit_block_or(dst_block, tmp_block);
        break;
    case bm::set_block_bitgap_bienc:
        this->read_bic_gap(decoder_, dst_block);
        break;
    case bm::set_block_bit_digest0:
        this->read_digest0_block(decoder_, dst_block);
        break;
    default:
        BM_ASSERT(0);
        #ifndef BM_NO_STL
            throw std::logic_error(this->err_msg());
        #else
            BM_THROW(BM_ERR_SERIALFORMAT);
        #endif
    } // switch
    return count;
}

template<class DEC>
unsigned 
serial_stream_iterator<DEC>::get_bit_block_AND(bm::word_t* BMRESTRICT dst_block,
                                               bm::word_t* BMRESTRICT tmp_block)
{
    BM_ASSERT(this->state_ == e_bit_block);
    BM_ASSERT(dst_block != tmp_block);
    unsigned count = 0;
    switch (block_type_)
    {
    case set_block_bit:
        decoder_.get_32_AND(dst_block, bm::set_block_size);
        break;
    case set_block_bit_0runs:
        {
        unsigned char run_type = decoder_.get_8();
        for (unsigned j = 0; j < bm::set_block_size;run_type = !run_type)
        {
            unsigned run_length = decoder_.get_16();

            unsigned run_end = j + run_length;
            if (run_type)
            {
                for (;j < run_end; ++j)
                {
                    BM_ASSERT(j < bm::set_block_size);
                    dst_block[j] &= decoder_.get_32();
                }
            }
            else
            {
                for (;j < run_end; ++j)
                {
                    BM_ASSERT(j < bm::set_block_size);
                    dst_block[j] = 0;
                }
            }
        } // for
        }
        break;
    case set_block_bit_interval:
        {
            unsigned head_idx = decoder_.get_16();
            unsigned tail_idx = decoder_.get_16();
            unsigned i;
            for ( i = 0; i < head_idx; ++i)
                dst_block[i] = 0;
            for ( i = head_idx; i <= tail_idx; ++i)
                dst_block[i] &= decoder_.get_32();
            for ( i = tail_idx + 1; i < bm::set_block_size; ++i)
                dst_block[i] = 0;
        }
        break;
    case set_block_bit_1bit:
    case set_block_arrbit:
        get_arr_bit(tmp_block, true /*clear target*/);
        if (dst_block)
            bm::bit_block_and(dst_block, tmp_block);
        break;		
    case set_block_arrbit_inv:
        get_inv_arr(tmp_block);
        if (dst_block)
            bm::bit_block_and(dst_block, tmp_block);
        break;
    case set_block_arr_bienc:
        if (dst_block)
        {
            bm::bit_block_set(tmp_block, 0);
            this->read_bic_arr(decoder_, tmp_block);
            bm::bit_block_and(dst_block, tmp_block);
        }
        else
            this->read_bic_arr(decoder_, 0); // dry read
        break;
    case bm::set_block_arr_bienc_inv:
        this->read_bic_arr_inv(decoder_, tmp_block);
        if (dst_block)
            bm::bit_block_and(dst_block, tmp_block);
        break;
    case bm::set_block_bitgap_bienc:
        if (dst_block)
        {
            BM_ASSERT(IS_VALID_ADDR(dst_block));
            bm::bit_block_set(tmp_block, 0);
            this->read_bic_gap(decoder_, tmp_block);
            bm::bit_block_and(dst_block, tmp_block);
        }
        else
            this->read_bic_gap(decoder_, 0); // dry read
        break;
    case bm::set_block_bit_digest0:
        if (dst_block)
        {
            BM_ASSERT(IS_VALID_ADDR(dst_block));
            bm::bit_block_set(tmp_block, 0);
            this->read_digest0_block(decoder_, tmp_block);
            bm::bit_block_and(dst_block, tmp_block);
        }
        else
            this->read_digest0_block(decoder_, 0); // dry read
        break;
    default:
        BM_ASSERT(0);
        #ifndef BM_NO_STL
            throw std::logic_error(this->err_msg());
        #else
            BM_THROW(BM_ERR_SERIALFORMAT);
        #endif
    } // switch
    return count;
}

template<class DEC>
unsigned 
serial_stream_iterator<DEC>::get_bit_block_XOR(bm::word_t*  dst_block,
                                               bm::word_t*  tmp_block)
{
    BM_ASSERT(this->state_ == e_bit_block);
    BM_ASSERT(dst_block != tmp_block);

    unsigned count = 0;
    switch (block_type_)
    {
    case set_block_bit:
        for (unsigned i = 0; i < bm::set_block_size; ++i)
            dst_block[i] ^= decoder_.get_32();
        break;
    case set_block_bit_0runs:
        {
        unsigned char run_type = decoder_.get_8();
        for (unsigned j = 0; j < bm::set_block_size;run_type = !run_type)
        {
            unsigned run_length = decoder_.get_16();
            if (run_type)
            {
                unsigned run_end = j + run_length;
                for (;j < run_end; ++j)
                {
                    BM_ASSERT(j < bm::set_block_size);
                    dst_block[j] ^= decoder_.get_32();
                }
            }
            else
            {
                j += run_length;
            }
        } // for
        }
        break;
    case set_block_bit_interval:
        {
            unsigned head_idx = decoder_.get_16();
            unsigned tail_idx = decoder_.get_16();
            for (unsigned i = head_idx; i <= tail_idx; ++i)
                dst_block[i] ^= decoder_.get_32();
        }
        break;
    case set_block_bit_1bit:
        // TODO: optimization
    case set_block_arrbit:
        get_arr_bit(tmp_block, true /*clear target*/);
        if (dst_block)
            bm::bit_block_xor(dst_block, tmp_block);
        break;
    case set_block_arrbit_inv:
        get_inv_arr(tmp_block);
        if (dst_block)
            bm::bit_block_xor(dst_block, tmp_block);
        break;
    case set_block_arr_bienc:
        bm::bit_block_set(tmp_block, 0);
        this->read_bic_arr(decoder_, tmp_block);
        if (dst_block)
            bm::bit_block_xor(dst_block, tmp_block);
        break;
    case bm::set_block_arr_bienc_inv:
        this->read_bic_arr_inv(decoder_, tmp_block);
        if (dst_block)
        {
            BM_ASSERT(IS_VALID_ADDR(dst_block));
            bm::bit_block_xor(dst_block, tmp_block);
        }
        break;
    case bm::set_block_bitgap_bienc:
        if (dst_block)
        {
            BM_ASSERT(IS_VALID_ADDR(dst_block));
            bm::bit_block_set(tmp_block, 0);
            this->read_bic_gap(decoder_, tmp_block);
            bm::bit_block_xor(dst_block, tmp_block);
        }
        else
            this->read_bic_gap(decoder_, 0); // dry read
        break;
    case bm::set_block_bit_digest0:
        if (dst_block)
        {
            BM_ASSERT(IS_VALID_ADDR(dst_block));
            bm::bit_block_set(tmp_block, 0);
            this->read_digest0_block(decoder_, tmp_block);
            bm::bit_block_xor(dst_block, tmp_block);
        }
        else
            this->read_digest0_block(decoder_, 0); // dry read
        break;
    default:
        BM_ASSERT(0);
        #ifndef BM_NO_STL
            throw std::logic_error(this->err_msg());
        #else
            BM_THROW(BM_ERR_SERIALFORMAT);
        #endif
    } // switch
    return count;
}

template<class DEC>
unsigned 
serial_stream_iterator<DEC>::get_bit_block_SUB(bm::word_t*  dst_block,
                                               bm::word_t*  tmp_block)
{
    BM_ASSERT(this->state_ == e_bit_block);
    BM_ASSERT(dst_block != tmp_block);

    unsigned count = 0;
    switch (block_type_)
    {
    case set_block_bit:
        for (unsigned i = 0; i < bm::set_block_size; ++i)
            dst_block[i] &= ~decoder_.get_32();
        break;
    case set_block_bit_0runs:
        {
        unsigned char run_type = decoder_.get_8();
        for (unsigned j = 0; j < bm::set_block_size;run_type = !run_type)
        {
            unsigned run_length = decoder_.get_16();
            if (run_type)
            {
                unsigned run_end = j + run_length;
                for (;j < run_end; ++j)
                {
                    BM_ASSERT(j < bm::set_block_size);
                    dst_block[j] &= ~decoder_.get_32();
                }
            }
            else
            {
                j += run_length;
            }
        } // for
        }
        break;
    case set_block_bit_interval:
        {
            unsigned head_idx = decoder_.get_16();
            unsigned tail_idx = decoder_.get_16();
            for (unsigned i = head_idx; i <= tail_idx; ++i)
                dst_block[i] &= ~decoder_.get_32();
        }
        break;
    case set_block_bit_1bit:
        // TODO: optimization
    case set_block_arrbit:
        get_arr_bit(tmp_block, true /*clear target*/);
        if (dst_block)
            bm::bit_block_sub(dst_block, tmp_block);
        break;
    case set_block_arrbit_inv:
        get_inv_arr(tmp_block);
        if (dst_block)
            bm::bit_block_sub(dst_block, tmp_block);
        break;
    case set_block_arr_bienc:
        bm::bit_block_set(tmp_block, 0);
        this->read_bic_arr(decoder_, tmp_block);
        if (dst_block)
            bm::bit_block_sub(dst_block, tmp_block);
        break;
    case bm::set_block_arr_bienc_inv:
        this->read_bic_arr_inv(decoder_, tmp_block);
        if (dst_block)
            bm::bit_block_sub(dst_block, tmp_block);
        break;
    case bm::set_block_bitgap_bienc:
        if (dst_block)
        {
            BM_ASSERT(IS_VALID_ADDR(dst_block));
            bm::bit_block_set(tmp_block, 0);
            this->read_bic_gap(decoder_, tmp_block);
            bm::bit_block_sub(dst_block, tmp_block);
        }
        else
            this->read_bic_gap(decoder_, 0); // dry read
        break;
    case bm::set_block_bit_digest0:
        if (dst_block)
        {
            BM_ASSERT(IS_VALID_ADDR(dst_block));
            bm::bit_block_set(tmp_block, 0);
            this->read_digest0_block(decoder_, tmp_block);
            bm::bit_block_sub(dst_block, tmp_block);
        }
        else
            this->read_digest0_block(decoder_, 0); // dry read
        break;
    default:
        BM_ASSERT(0);
        #ifndef BM_NO_STL
            throw std::logic_error(this->err_msg());
        #else
            BM_THROW(BM_ERR_SERIALFORMAT);
        #endif
    } // switch
    return count;
}


template<class DEC>
unsigned 
serial_stream_iterator<DEC>::get_bit_block_COUNT(bm::word_t*  /*dst_block*/,
                                                 bm::word_t*  tmp_block)
{
    BM_ASSERT(this->state_ == e_bit_block);

    unsigned count = 0;
    switch (block_type_)
    {
    case set_block_bit:
        for (unsigned i = 0; i < bm::set_block_size; ++i)
            count += bm::word_bitcount(decoder_.get_32());
        break;
    case set_block_bit_0runs:
        {
        //count = 0;
        unsigned char run_type = decoder_.get_8();
        for (unsigned j = 0; j < bm::set_block_size;run_type = !run_type)
        {
            unsigned run_length = decoder_.get_16();
            if (run_type)
            {
                unsigned run_end = j + run_length;
                for (;j < run_end; ++j)
                {
                    count += word_bitcount(decoder_.get_32());
                }
            }
            else
            {
                j += run_length;
            }
        } // for
        return count;
        }
    case set_block_bit_interval:
        {
            unsigned head_idx = decoder_.get_16();
            unsigned tail_idx = decoder_.get_16();
            for (unsigned i = head_idx; i <= tail_idx; ++i)
                count += bm::word_bitcount(decoder_.get_32());
        }
        break;
    case set_block_arrbit:
        count += get_arr_bit(0);
        break;
    case set_block_bit_1bit:
        ++count;
        decoder_.get_16();
        break;
    case set_block_arrbit_inv:
        get_inv_arr(tmp_block);
        goto count_tmp;
    case set_block_arr_bienc:
        bm::bit_block_set(tmp_block, 0); // TODO: just add a counted read
        this->read_bic_arr(decoder_, tmp_block);
        goto count_tmp;
    case bm::set_block_arr_bienc_inv:
        this->read_bic_arr_inv(decoder_, tmp_block);
        goto count_tmp;
    case bm::set_block_bit_digest0:
        bm::bit_block_set(tmp_block, 0);
        this->read_digest0_block(decoder_, tmp_block);
        goto count_tmp;
    case bm::set_block_bitgap_bienc:
        bm::bit_block_set(tmp_block, 0);
        this->read_bic_gap(decoder_, tmp_block);
    count_tmp:
        count += bm::bit_block_count(tmp_block);
        break;
    default:
        BM_ASSERT(0);
        #ifndef BM_NO_STL
            throw std::logic_error(this->err_msg());
        #else
            BM_THROW(BM_ERR_SERIALFORMAT);
        #endif

    } // switch
    return count;
}

template<class DEC>
unsigned 
serial_stream_iterator<DEC>::get_bit_block_COUNT_A(bm::word_t*  dst_block,
                                                   bm::word_t*  tmp_block)
{
    BM_ASSERT(this->state_ == e_bit_block);
    unsigned count = 0;
    if (dst_block)
    {
        // count the block bitcount
        count = bm::bit_block_count(dst_block);
    }

    switch (block_type_)
    {
    case set_block_bit:
        decoder_.get_32(0, bm::set_block_size);
        break;
    case set_block_bit_0runs:
        {
        unsigned char run_type = decoder_.get_8();
        for (unsigned j = 0; j < bm::set_block_size;run_type = !run_type)
        {
            unsigned run_length = decoder_.get_16();
            if (run_type)
            {
                unsigned run_end = j + run_length;
                for (;j < run_end; ++j)
                {
                    decoder_.get_32();
                }
            }
            else
            {
                j += run_length;
            }
        } // for
        }
        break;

    case set_block_bit_interval:
        {
            unsigned head_idx = decoder_.get_16();
            unsigned tail_idx = decoder_.get_16();
            for (unsigned i = head_idx; i <= tail_idx; ++i)
                decoder_.get_32();
        }
        break;
    case set_block_arrbit:
        get_arr_bit(0);
        break;
    case set_block_bit_1bit:
        decoder_.get_16();
        break;
    case set_block_arrbit_inv:
        get_inv_arr(tmp_block);
        break;
    case set_block_arr_bienc:
        this->read_bic_arr(decoder_, tmp_block); // TODO: add dry read
        break;
    case bm::set_block_arr_bienc_inv:
        this->read_bic_arr_inv(decoder_, tmp_block); // TODO: add dry read
        break;
    case bm::set_block_bitgap_bienc:
        this->read_bic_gap(decoder_, tmp_block);
        break;
    case bm::set_block_bit_digest0:
        this->read_digest0_block(decoder_, 0); // dry read
        break;
    default:
        BM_ASSERT(0);
        #ifndef BM_NO_STL
            throw std::logic_error(this->err_msg());
        #else
            BM_THROW(BM_ERR_SERIALFORMAT);
        #endif

    } // switch
    return count;
}


template<class DEC>
unsigned 
serial_stream_iterator<DEC>::get_bit_block_COUNT_AND(bm::word_t*  dst_block,
                                                     bm::word_t*  tmp_block)
{
    BM_ASSERT(this->state_ == e_bit_block);
    BM_ASSERT(dst_block);

    unsigned count = 0;
    switch (block_type_)
    {
    case set_block_bit:
        for (unsigned i = 0; i < bm::set_block_size; ++i)
            count += word_bitcount(dst_block[i] & decoder_.get_32());
        break;
    case set_block_bit_0runs:
        {
        //count = 0;
        unsigned char run_type = decoder_.get_8();
        for (unsigned j = 0; j < bm::set_block_size;run_type = !run_type)
        {
            unsigned run_length = decoder_.get_16();
            if (run_type)
            {
                unsigned run_end = j + run_length;
                for (;j < run_end; ++j)
                {
                    count += word_bitcount(dst_block[j] & decoder_.get_32());
                }
            }
            else
            {
                j += run_length;
            }
        } // for
        return count;
        }
    case set_block_bit_interval:
        {
        unsigned head_idx = decoder_.get_16();
        unsigned tail_idx = decoder_.get_16();
        for (unsigned i = head_idx; i <= tail_idx; ++i)
            count += word_bitcount(dst_block[i] & decoder_.get_32());
        }
        break;
    case set_block_bit_1bit:
        // TODO: optimization
    case set_block_arrbit:
        get_arr_bit(tmp_block, true /*clear target*/);
        goto count_tmp;
        break;
    case set_block_arrbit_inv:
        get_inv_arr(tmp_block);
        goto count_tmp;
        break;
    case set_block_arr_bienc:
        bm::bit_block_set(tmp_block, 0);
        this->read_bic_arr(decoder_, tmp_block);
        goto count_tmp;
    case bm::set_block_arr_bienc_inv:
        this->read_bic_arr_inv(decoder_, tmp_block);
        goto count_tmp;
    case bm::set_block_bit_digest0:
        bm::bit_block_set(tmp_block, 0);
        this->read_digest0_block(decoder_, tmp_block);
        goto count_tmp;
    case bm::set_block_bitgap_bienc:
        bm::bit_block_set(tmp_block, 0);
        this->read_bic_gap(decoder_, tmp_block);
    count_tmp:
        count += bm::bit_operation_and_count(dst_block, tmp_block);
        break;
    default:
        BM_ASSERT(0);
        #ifndef BM_NO_STL
            throw std::logic_error(this->err_msg());
        #else
            BM_THROW(BM_ERR_SERIALFORMAT);
        #endif

    } // switch
    return count;
}

template<class DEC>
unsigned 
serial_stream_iterator<DEC>::get_bit_block_COUNT_OR(bm::word_t*  dst_block,
                                                    bm::word_t*  tmp_block)
{
    BM_ASSERT(this->state_ == e_bit_block);
    BM_ASSERT(dst_block);

    bitblock_sum_adapter count_adapter;
    switch (block_type_)
    {
    case set_block_bit:
        {
        bitblock_get_adapter ga(dst_block);
        bit_COUNT_OR<bm::word_t> func;
        
        bit_recomb(ga,
                   decoder_,
                   func,
                   count_adapter
                  );
        }
        break;
    case set_block_bit_0runs: 
        {
        unsigned count = 0;
        unsigned char run_type = decoder_.get_8();
        for (unsigned j = 0; j < bm::set_block_size;run_type = !run_type)
        {
            unsigned run_length = decoder_.get_16();
            unsigned run_end = j + run_length;
            if (run_type)
            {
                for (;j < run_end; ++j)
                {
                    BM_ASSERT(j < bm::set_block_size);
                    count += word_bitcount(dst_block[j] | decoder_.get_32());
                }
            }
            else
            {
                for (;j < run_end; ++j)
                {
                    BM_ASSERT(j < bm::set_block_size);
                    count += word_bitcount(dst_block[j]);
                }
            }
        } // for
        return count;
        }
    case set_block_bit_interval:
        {
        unsigned head_idx = decoder_.get_16();
        unsigned tail_idx = decoder_.get_16();
        unsigned count = 0;
        unsigned i;
        for (i = 0; i < head_idx; ++i)
            count += bm::word_bitcount(dst_block[i]);
        for (i = head_idx; i <= tail_idx; ++i)
            count += bm::word_bitcount(dst_block[i] | decoder_.get_32());
        for (i = tail_idx + 1; i < bm::set_block_size; ++i)
            count += bm::word_bitcount(dst_block[i]);
        return count;
        }
    case set_block_bit_1bit:
        // TODO: optimization
    case set_block_arrbit:
        get_arr_bit(tmp_block, true /* clear target*/);
        return bit_operation_or_count(dst_block, tmp_block);
    case set_block_arrbit_inv:
        get_inv_arr(tmp_block);
        goto count_tmp;
    case set_block_arr_bienc:
        bm::bit_block_set(tmp_block, 0);
        this->read_bic_arr(decoder_, tmp_block);
        goto count_tmp;
    case bm::set_block_arr_bienc_inv:
        this->read_bic_arr_inv(decoder_, tmp_block);
        goto count_tmp;
    case bm::set_block_bit_digest0:
        bm::bit_block_set(tmp_block, 0);
        this->read_digest0_block(decoder_, tmp_block);
        goto count_tmp;
    case bm::set_block_bitgap_bienc:
        bm::bit_block_set(tmp_block, 0);
        this->read_bic_gap(decoder_, tmp_block);
    count_tmp:
        return bm::bit_operation_or_count(dst_block, tmp_block);
    default:
        BM_ASSERT(0);
        #ifndef BM_NO_STL
            throw std::logic_error(this->err_msg());
        #else
            BM_THROW(BM_ERR_SERIALFORMAT);
        #endif

    } // switch
    return count_adapter.sum();
}

template<class DEC>
unsigned 
serial_stream_iterator<DEC>::get_bit_block_COUNT_XOR(bm::word_t*  dst_block,
                                                     bm::word_t*  tmp_block)
{
    BM_ASSERT(this->state_ == e_bit_block);
    BM_ASSERT(dst_block);

    bitblock_sum_adapter count_adapter;
    switch (block_type_)
    {
    case set_block_bit:
        {
        bitblock_get_adapter ga(dst_block);
        bit_COUNT_XOR<bm::word_t> func;
        
        bit_recomb(ga,
                   decoder_,
                   func,
                   count_adapter
                  );
        }
        break;
    case set_block_bit_0runs: 
        {
        unsigned count = 0;
        unsigned char run_type = decoder_.get_8();
        for (unsigned j = 0; j < bm::set_block_size;run_type = !run_type)
        {
            unsigned run_length = decoder_.get_16();
            unsigned run_end = j + run_length;
            if (run_type)
            {
                for (;j < run_end; ++j)
                {
                    BM_ASSERT(j < bm::set_block_size);
                    count += bm::word_bitcount(dst_block[j] ^ decoder_.get_32());
                }
            }
            else
            {
                for (;j < run_end; ++j)
                {
                    BM_ASSERT(j < bm::set_block_size);
                    count += bm::word_bitcount(dst_block[j]);
                }
            }
        } // for
        return count;
        }
    case set_block_bit_interval:
        {
        unsigned head_idx = decoder_.get_16();
        unsigned tail_idx = decoder_.get_16();
        unsigned count = 0;
        unsigned i;
        for (i = 0; i < head_idx; ++i)
            count += bm::word_bitcount(dst_block[i]);
        for (i = head_idx; i <= tail_idx; ++i)
            count += bm::word_bitcount(dst_block[i] ^ decoder_.get_32());
        for (i = tail_idx + 1; i < bm::set_block_size; ++i)
            count += bm::word_bitcount(dst_block[i]);
        return count;
        }
    case set_block_bit_1bit:
        // TODO: optimization
    case set_block_arrbit:
        get_arr_bit(tmp_block, true /* clear target*/);
        goto count_tmp;
    case set_block_arrbit_inv:
        get_inv_arr(tmp_block);
        goto count_tmp;
    case set_block_arr_bienc:
        bm::bit_block_set(tmp_block, 0);
        this->read_bic_arr(decoder_, tmp_block);
        goto count_tmp;
    case bm::set_block_arr_bienc_inv:
        this->read_bic_arr_inv(decoder_, tmp_block);
        goto count_tmp;
        break;
    case bm::set_block_bit_digest0:
        bm::bit_block_set(tmp_block, 0);
        this->read_digest0_block(decoder_, tmp_block);
        goto count_tmp;
    case bm::set_block_bitgap_bienc:
        bm::bit_block_set(tmp_block, 0);
        this->read_bic_gap(decoder_, tmp_block);
    count_tmp:
        return bm::bit_operation_xor_count(dst_block, tmp_block);
    default:
        BM_ASSERT(0);
        #ifndef BM_NO_STL
            throw std::logic_error(this->err_msg());
        #else
            BM_THROW(BM_ERR_SERIALFORMAT);
        #endif

    } // switch
    return count_adapter.sum();
}

template<class DEC>
unsigned 
serial_stream_iterator<DEC>::get_bit_block_COUNT_SUB_AB(bm::word_t*  dst_block,
                                                        bm::word_t*  tmp_block)
{
    BM_ASSERT(this->state_ == e_bit_block);
    BM_ASSERT(dst_block);

    bitblock_sum_adapter count_adapter;
    switch (block_type_)
    {
    case set_block_bit:
        {
        bitblock_get_adapter ga(dst_block);
        bit_COUNT_SUB_AB<bm::word_t> func;
        
        bit_recomb(ga, 
                   decoder_,
                   func,
                   count_adapter
                  );
        }
        break;
    case set_block_bit_0runs: 
        {
        unsigned count = 0;
        unsigned char run_type = decoder_.get_8();
        for (unsigned j = 0; j < bm::set_block_size;run_type = !run_type)
        {
            unsigned run_length = decoder_.get_16();
            unsigned run_end = j + run_length;
            if (run_type)
            {
                for (;j < run_end; ++j)
                {
                    BM_ASSERT(j < bm::set_block_size);
                    count += word_bitcount(dst_block[j] & ~decoder_.get_32());
                }
            }
            else
            {
                for (;j < run_end; ++j)
                {
                    BM_ASSERT(j < bm::set_block_size);
                    count += bm::word_bitcount(dst_block[j]);
                }
            }
        } // for
        return count;
        }
    case set_block_bit_interval:
        {
        unsigned head_idx = decoder_.get_16();
        unsigned tail_idx = decoder_.get_16();
        unsigned count = 0;
        unsigned i;
        for (i = 0; i < head_idx; ++i)
            count += bm::word_bitcount(dst_block[i]);
        for (i = head_idx; i <= tail_idx; ++i)
            count += bm::word_bitcount(dst_block[i] & (~decoder_.get_32()));
        for (i = tail_idx + 1; i < bm::set_block_size; ++i)
            count += bm::word_bitcount(dst_block[i]);
        return count;
        }
        break;
    case set_block_bit_1bit:
        //TODO: optimization
    case set_block_arrbit:
        get_arr_bit(tmp_block, true /* clear target*/);
        goto count_tmp;
    case set_block_arrbit_inv:
        get_inv_arr(tmp_block);
        goto count_tmp;
    case set_block_arr_bienc:
        bm::bit_block_set(tmp_block, 0);
        this->read_bic_arr(decoder_, tmp_block);
        goto count_tmp;
    case bm::set_block_arr_bienc_inv:
        this->read_bic_arr_inv(decoder_, tmp_block);
        goto count_tmp;
    case bm::set_block_bit_digest0:
        bm::bit_block_set(tmp_block, 0);
        this->read_digest0_block(decoder_, tmp_block);
        goto count_tmp;
    case bm::set_block_bitgap_bienc:
        bm::bit_block_set(tmp_block, 0);
        this->read_bic_gap(decoder_, tmp_block);
    count_tmp:
        return bm::bit_operation_sub_count(dst_block, tmp_block);
    default:
        BM_ASSERT(0);
        #ifndef BM_NO_STL
            throw std::logic_error(this->err_msg());
        #else
            BM_THROW(BM_ERR_SERIALFORMAT);
        #endif

    } // switch
    return count_adapter.sum();
}

template<class DEC>
unsigned 
serial_stream_iterator<DEC>::get_bit_block_COUNT_SUB_BA(bm::word_t*  dst_block,
                                                        bm::word_t*  tmp_block)
{
    BM_ASSERT(this->state_ == e_bit_block);
    BM_ASSERT(dst_block);

    bitblock_sum_adapter count_adapter;
    switch (block_type_)
    {
    case set_block_bit:
        {
        bitblock_get_adapter ga(dst_block);
        bit_COUNT_SUB_BA<bm::word_t> func;

        bit_recomb(ga,
                   decoder_,
                   func,
                   count_adapter
                  );
        }
        break;
    case set_block_bit_0runs: 
        {
        unsigned count = 0;
        unsigned char run_type = decoder_.get_8();
        for (unsigned j = 0; j < bm::set_block_size;run_type = !run_type)
        {
            unsigned run_length = decoder_.get_16();
            unsigned run_end = j + run_length;
            if (run_type)
            {
                for (;j < run_end; ++j)
                {
                    BM_ASSERT(j < bm::set_block_size);
                    count += word_bitcount(decoder_.get_32() & (~dst_block[j]));
                }
            }
            else
            {
                j += run_length;
            }
        } // for
        return count;
        }

    case set_block_bit_interval:
        {
        unsigned head_idx = decoder_.get_16();
        unsigned tail_idx = decoder_.get_16();
        unsigned count = 0;
        unsigned i;
        for (i = head_idx; i <= tail_idx; ++i)
            count += bm::word_bitcount(decoder_.get_32() & (~dst_block[i]));
        return count;
        }
        break;
    case set_block_bit_1bit:
        // TODO: optimization
    case set_block_arrbit:
        get_arr_bit(tmp_block, true /* clear target*/);
        goto count_tmp;
    case set_block_arrbit_inv:
        get_inv_arr(tmp_block);
        goto count_tmp;
    case set_block_arr_bienc:
        bm::bit_block_set(tmp_block, 0);
        this->read_bic_arr(decoder_, tmp_block);
        goto count_tmp;
    case bm::set_block_arr_bienc_inv:
        this->read_bic_arr_inv(decoder_, tmp_block);
        goto count_tmp;
    case bm::set_block_bit_digest0:
        bm::bit_block_set(tmp_block, 0);
        this->read_digest0_block(decoder_, tmp_block);
        goto count_tmp;
    case bm::set_block_bitgap_bienc:
        bm::bit_block_set(tmp_block, 0);
        this->read_bic_gap(decoder_, tmp_block);
    count_tmp:
        return bm::bit_operation_sub_count(tmp_block, dst_block);
    default:
        BM_ASSERT(0);
        #ifndef BM_NO_STL
            throw std::logic_error(this->err_msg());
        #else
            BM_THROW(BM_ERR_SERIALFORMAT);
        #endif
    } // switch
    return count_adapter.sum();
}



template<class DEC>
unsigned serial_stream_iterator<DEC>::get_arr_bit(bm::word_t* dst_block, 
                                                  bool        clear_target)
{
    BM_ASSERT(this->block_type_ == set_block_arrbit || 
              this->block_type_ == set_block_bit_1bit);
    
    gap_word_t len = decoder_.get_16(); // array length / 1bit_idx
    if (dst_block)
    {
        if (clear_target)
            bm::bit_block_set(dst_block, 0);

        if (this->block_type_ == set_block_bit_1bit)
        {
            // len contains idx of 1 bit set
            set_bit(dst_block, len);
            return 1;
        }

        for (unsigned k = 0; k < len; ++k)
        {
            gap_word_t bit_idx = decoder_.get_16();
            bm::set_bit(dst_block, bit_idx);
        }
    }
    else
    {
        if (this->block_type_ == set_block_bit_1bit)
        {
            return 1; // nothing to do: len var already consumed 16bits
        }
        // fwd the decocing stream
        decoder_.seek(len * 2);
    }
    return len;
}

template<class DEC>
unsigned serial_stream_iterator<DEC>::get_bit()
{
    BM_ASSERT(this->block_type_ == set_block_bit_1bit);
    ++(this->block_idx_);
    this->state_ = e_blocks;

	return decoder_.get_16(); // 1bit_idx	
}

template<class DEC>
void 
serial_stream_iterator<DEC>::get_gap_block(bm::gap_word_t* dst_block)
{
    BM_ASSERT(this->state_ == e_gap_block || 
              this->block_type_ == set_block_bit_1bit);
    BM_ASSERT(dst_block);

    this->read_gap_block(this->decoder_,
                   this->block_type_,
                   dst_block,
                   this->gap_head_);

    ++(this->block_idx_);
    this->state_ = e_blocks;
}


template<class DEC>
unsigned 
serial_stream_iterator<DEC>::get_bit_block(bm::word_t*    dst_block,
                                           bm::word_t*    tmp_block,
                                           set_operation  op)
{
    BM_ASSERT(this->state_ == e_bit_block);
    
    get_bit_func_type bit_func = bit_func_table_[op];
    BM_ASSERT(bit_func);
    unsigned cnt = ((*this).*(bit_func))(dst_block, tmp_block);
    this->state_ = e_blocks;
    ++(this->block_idx_);
    return cnt;
}



template<class BV>
typename operation_deserializer<BV>::size_type
operation_deserializer<BV>::deserialize(bvector_type&        bv,
                                        const unsigned char* buf, 
                                        bm::word_t*          temp_block,
                                        set_operation        op,
                                        bool                 exit_on_one)
{
    ByteOrder bo_current = globals<true>::byte_order();
    bm::decoder dec(buf);
    unsigned char header_flag = dec.get_8();
    ByteOrder bo = bo_current;
    if (!(header_flag & BM_HM_NO_BO))
        bo = (bm::ByteOrder) dec.get_8();

    blocks_manager_type& bman = bv.get_blocks_manager();
    bit_block_guard<blocks_manager_type> bg(bman);
    if (!temp_block)
        temp_block = bg.allocate();

    if (op == bm::set_ASSIGN)
    {
        bv.clear(true);
        op = bm::set_OR;
    }

    if (bo_current == bo)
    {
        serial_stream_current ss(buf);
        bm::iterator_deserializer<BV, serial_stream_current> it_d;
        return it_d.deserialize(bv, ss, temp_block, op, exit_on_one);
    }
    switch (bo_current) 
    {
    case BigEndian:
        {
        serial_stream_be ss(buf);
        iterator_deserializer<BV, serial_stream_be> it_d;
        return it_d.deserialize(bv, ss, temp_block, op, exit_on_one);
        }
    case LittleEndian:
        {
        serial_stream_le ss(buf);
        iterator_deserializer<BV, serial_stream_le> it_d;
        return it_d.deserialize(bv, ss, temp_block, op, exit_on_one);
        }
    default:
        BM_ASSERT(0);
        #ifndef BM_NO_STL
            throw std::logic_error("BM::platform error: unknown endianness");
        #else
            BM_THROW(BM_ERR_SERIALFORMAT);
        #endif
    };
    return 0;
}

template<class BV>
void operation_deserializer<BV>::deserialize_range(
                       bvector_type&        bv,
                       const unsigned char* buf,
                       bm::word_t*          temp_block,
                       size_type            idx_from,
                       size_type            idx_to)
{
    ByteOrder bo_current = globals<true>::byte_order();
    bm::decoder dec(buf);
    unsigned char header_flag = dec.get_8();
    ByteOrder bo = bo_current;
    if (!(header_flag & BM_HM_NO_BO))
        bo = (bm::ByteOrder) dec.get_8();

    blocks_manager_type& bman = bv.get_blocks_manager();
    bit_block_guard<blocks_manager_type> bg(bman);
    if (!temp_block)
        temp_block = bg.allocate();

    const bm::set_operation op = bm::set_AND;

    if (bo_current == bo)
    {
        serial_stream_current ss(buf);
        bm::iterator_deserializer<BV, serial_stream_current> it_d;
        it_d.set_range(idx_from, idx_to);
        it_d.deserialize(bv, ss, temp_block, op, false);
        return;
    }
    switch (bo_current)
    {
    case BigEndian:
        {
        serial_stream_be ss(buf);
        iterator_deserializer<BV, serial_stream_be> it_d;
        it_d.set_range(idx_from, idx_to);
        it_d.deserialize(bv, ss, temp_block, op, false);
        return;
        }
    case LittleEndian:
        {
        serial_stream_le ss(buf);
        iterator_deserializer<BV, serial_stream_le> it_d;
        it_d.set_range(idx_from, idx_to);
        it_d.deserialize(bv, ss, temp_block, op, false);
        return;
        }
    default:
        BM_ASSERT(0);
        #ifndef BM_NO_STL
            throw std::logic_error("BM::platform error: unknown endianness");
        #else
            BM_THROW(BM_ERR_SERIALFORMAT);
        #endif
    };
    return;
}



// ------------------------------------------------------------------

template<class BV, class SerialIterator>
void iterator_deserializer<BV, SerialIterator>::set_range(
                                        size_type from, size_type to)
{
    is_range_set_ = true;
    nb_range_from_ = (from >> bm::set_block_shift);
    nb_range_to_ = (to >> bm::set_block_shift);
}


template<class BV, class SerialIterator>
void iterator_deserializer<BV, SerialIterator>::load_id_list(
                                            bvector_type&         bv, 
                                            serial_iterator_type& sit,
                                            unsigned              id_count,
                                            bool                  set_clear)
{
    const unsigned win_size = 64;
    bm::id_t id_buffer[win_size+1];

    if (set_clear)  // set bits
    {
        for (unsigned i = 0; i <= id_count;)
        {
            unsigned j;
            for (j = 0; j < win_size && i <= id_count; ++j, ++i) 
            {
                id_buffer[j] = sit.get_id();
                sit.next();
            } // for j
            bm::combine_or(bv, id_buffer, id_buffer + j);
        } // for i
    } 
    else // clear bits
    {
        for (unsigned i = 0; i <= id_count;)
        {
            unsigned j;
            for (j = 0; j < win_size && i <= id_count; ++j, ++i) 
            {
                id_buffer[j] = sit.get_id();
                sit.next();
            } // for j
            bm::combine_sub(bv, id_buffer, id_buffer + j);
        } // for i
    }
}

template<class BV, class SerialIterator>
typename iterator_deserializer<BV, SerialIterator>::size_type
iterator_deserializer<BV, SerialIterator>::finalize_target_vector(
                                        blocks_manager_type& bman,
                                        set_operation        op,
                                        size_type            bv_block_idx)
{
    size_type count = 0;
    switch (op)
    {
    case set_OR:    case set_SUB:     case set_XOR:
    case set_COUNT: case set_COUNT_B: case set_COUNT_AND:
    case set_COUNT_SUB_BA:
        // nothing to do
        break;
    case set_ASSIGN: case set_AND:
        {
            block_idx_type nblock_last = (bm::id_max >> bm::set_block_shift);
            if (bv_block_idx <= nblock_last)
                bman.set_all_zero(bv_block_idx, nblock_last); // clear the tail
        }
        break;
    case set_COUNT_A: case set_COUNT_OR: case set_COUNT_XOR:
    case set_COUNT_SUB_AB:
        // count bits in the target vector
        {
            unsigned i, j;
            bm::get_block_coord(bv_block_idx, i, j);
            bm::word_t*** blk_root = bman.top_blocks_root();
            unsigned top_size = bman.top_block_size();
            for (;i < top_size; ++i)
            {
                bm::word_t** blk_blk = blk_root[i];
                if (blk_blk == 0) 
                {
                    bv_block_idx+=bm::set_sub_array_size-j;
                    j = 0;
                    continue;
                }
                // TODO: optimize for FULL top level
                for (; j < bm::set_sub_array_size; ++j, ++bv_block_idx)
                {
                    if (blk_blk[j])
                        count += bman.block_bitcount(blk_blk[j]);
                } // for j
                j = 0;
            } // for i
        }
        break;
    case set_END:
    default:
        BM_ASSERT(0);
        #ifndef BM_NO_STL
            throw std::logic_error(err_msg());
        #else
            BM_THROW(BM_ERR_SERIALFORMAT);
        #endif
    }
    return count;
}

template<class BV, class SerialIterator>
typename iterator_deserializer<BV, SerialIterator>::size_type
iterator_deserializer<BV, SerialIterator>::process_id_list(
                                    bvector_type&         bv, 
                                    serial_iterator_type& sit,
                                    set_operation         op)
{
    size_type count = 0;
    unsigned id_count = sit.get_id_count();
    bool set_clear = true;
    switch (op)
    {
    case set_AND:
        {
            // TODO: use some more optimal algorithm without temp vector
            BV bv_tmp(BM_GAP);
            load_id_list(bv_tmp, sit, id_count, true);
            bv &= bv_tmp;
        }
        break;
    case set_ASSIGN:
        BM_ASSERT(0);
        BM_FALLTHROUGH;
        // fall through
    case set_OR:
        set_clear = true;
        BM_FALLTHROUGH;
        // fall through
    case set_SUB:
        load_id_list(bv, sit, id_count, set_clear);
        break;
    case set_XOR:
        for (unsigned i = 0; i < id_count; ++i)
        {
            bm::id_t id = sit.get_id();
            bv[id] ^= true;
            sit.next();
        } // for
        break;
    case set_COUNT: case set_COUNT_B:
        for (unsigned i = 0; i < id_count; ++i)
        {
            /* bm::id_t id = */ sit.get_id();
            ++count;
            sit.next();
        } // for
        break;
    case set_COUNT_A:
        return bv.count();
    case set_COUNT_AND:
        for (size_type i = 0; i < id_count; ++i)
        {
            bm::id_t id = sit.get_id();
            count += bv.get_bit(id);
            sit.next();
        } // for
        break;
    case set_COUNT_XOR:
        {
            // TODO: get rid of the temp vector
            BV bv_tmp(BM_GAP);
            load_id_list(bv_tmp, sit, id_count, true);
            count += bm::count_xor(bv, bv_tmp);
        }
        break;
    case set_COUNT_OR:
        {
            // TODO: get rid of the temp. vector
            BV bv_tmp(BM_GAP);
            load_id_list(bv_tmp, sit, id_count, true);
            count += bm::count_or(bv, bv_tmp);
        }
        break;
    case set_COUNT_SUB_AB:
        {
            // TODO: get rid of the temp. vector
            BV bv_tmp(bv);
            load_id_list(bv_tmp, sit, id_count, false);
            count += bv_tmp.count();
        }
        break;
    case set_COUNT_SUB_BA:
        {
            BV bv_tmp(BM_GAP);
            load_id_list(bv_tmp, sit, id_count, true);
            count += bm::count_sub(bv_tmp, bv);        
        }
        break;
    case set_END:
    default:
        BM_ASSERT(0);
        #ifndef BM_NO_STL
            throw std::logic_error(err_msg());
        #else
            BM_THROW(BM_ERR_SERIALFORMAT);
        #endif
    } // switch

    return count;
}


template<class BV, class SerialIterator>
typename iterator_deserializer<BV, SerialIterator>::size_type
iterator_deserializer<BV, SerialIterator>::deserialize(
                                       bvector_type&         bv, 
                                       serial_iterator_type& sit, 
                                       bm::word_t*           temp_block,
                                       set_operation         op,
                                       bool                  exit_on_one)
{
    BM_ASSERT(temp_block);

    size_type count = 0;
    gap_word_t   gap_temp_block[bm::gap_equiv_len * 4];
    gap_temp_block[0] = 0;

    blocks_manager_type& bman = bv.get_blocks_manager();
    if (!bman.is_init())
        bman.init_tree();

    if (sit.bv_size() && (sit.bv_size() > bv.size()))
        bv.resize(sit.bv_size());

    typename serial_iterator_type::iterator_state state;
    state = sit.get_state();
    if (state == serial_iterator_type::e_list_ids)
    {
        count = process_id_list(bv, sit, op);
        return count;
    }

    size_type bv_block_idx = 0;
/*
    size_type last_block_idx = 0;
    if (op == bm::set_AND)
    {
        size_type last_pos;
        bool found = bv.find_reverse(last_pos); // TODO: opt: find last block
        if (found)
            last_block_idx = (last_pos >> bm::set_block_shift);
        else
            return count;
    }
*/
    for (;1;)
    {
        bm::set_operation sop = op;
        if (sit.is_eof()) // argument stream ended
        {
            count += finalize_target_vector(bman, op, bv_block_idx);
            return count;
        }

        state = sit.state();
        switch (state)
        {
        case serial_iterator_type::e_blocks:
            sit.next();
            continue;
        case serial_iterator_type::e_bit_block:
            {
            BM_ASSERT(sit.block_idx() == bv_block_idx);
            unsigned i0, j0;
            bm::get_block_coord(bv_block_idx, i0, j0);
            bm::word_t* blk = bman.get_block_ptr(i0, j0);
            if (!blk)
            {
                switch (op)
                {
                case set_AND:          case set_SUB: case set_COUNT_AND:
                case set_COUNT_SUB_AB: case set_COUNT_A:
                    // one arg is 0, so the operation gives us zero
                    // all we need to do is to seek the input stream
                    sop = set_ASSIGN;
                    break;

                case set_OR: case set_XOR: case set_ASSIGN:
                    blk = bman.make_bit_block(bv_block_idx);
                    break;

                case set_COUNT:        case set_COUNT_XOR: case set_COUNT_OR:
                case set_COUNT_SUB_BA: case set_COUNT_B:
                    // first arg is not required (should work as is)
                    sop = set_COUNT;
                    break;

                case set_END:
                default:
                    BM_ASSERT(0);
                    #ifndef BM_NO_STL
                        throw std::logic_error(err_msg());
                    #else
                        BM_THROW(BM_ERR_SERIALFORMAT);
                    #endif
                }
            }
            else // block exists
            {
                int is_gap = BM_IS_GAP(blk);
                if (is_gap || IS_FULL_BLOCK(blk))
                {
                    if (IS_FULL_BLOCK(blk) && is_const_set_operation(op))
                    {
                        blk = FULL_BLOCK_REAL_ADDR;
                    }
                    else
                    {
                        // TODO: make sure const operations do not 
                        // deoptimize GAP blocks
                        blk = bman.deoptimize_block(bv_block_idx);
                    }
                }
            }

            // 2 bit-blocks recombination
            unsigned c = sit.get_bit_block(blk, temp_block, sop);
            count += c;
			if (exit_on_one && count) // early exit
				return count;
            switch (op) // target block optimization for non-const operations
            {
            case set_AND: case set_SUB: case set_XOR: case set_OR:
                bman.optimize_bit_block(i0, j0);
                break;
            default: break;
            } // switch

            }
            break;

        case serial_iterator_type::e_zero_blocks:
            {
            BM_ASSERT(bv_block_idx == sit.block_idx());
            
            switch (op)
            {
            case set_ASSIGN: // nothing to do to rewind fwd
            case set_SUB: case set_COUNT_AND:    case set_OR:
            case set_XOR: case set_COUNT_SUB_BA: case set_COUNT_B:
                bv_block_idx = sit.skip_mono_blocks();
                continue;
                
            case set_AND: // clear the range
                {
                    size_type nb_start = bv_block_idx;
                    bv_block_idx = sit.skip_mono_blocks();
                    bman.set_all_zero(nb_start, bv_block_idx-1);
                }
                continue;
            case set_END:
            default:
                break;
            } // switch op

            
            unsigned i0, j0;
            bm::get_block_coord(bv_block_idx, i0, j0);
            bm::word_t* blk = bman.get_block_ptr(i0, j0);

            sit.next();

            if (blk)
            {
                switch (op)
                {
                case set_AND: case set_ASSIGN:
                    // the result is 0
                    //blk =
                    bman.zero_block(bv_block_idx);
                    break;

                case set_SUB: case set_COUNT_AND:    case set_OR:
                case set_XOR: case set_COUNT_SUB_BA: case set_COUNT_B:
                    // nothing to do
                    break;
                
                case set_COUNT_SUB_AB: case set_COUNT_A: case set_COUNT_OR:
                case set_COUNT:        case set_COUNT_XOR:
                    // valid bit block recombined with 0 block
                    // results with same block data
                    // all we need is to bitcount bv block
                    {
                    count += blk ? bman.block_bitcount(blk) : 0;
					if (exit_on_one && count) // early exit
						return count;
                    }
                    break;
                case set_END:
                default:
                    BM_ASSERT(0);
                } // switch op
            } // if blk
            }
            break;

        case serial_iterator_type::e_one_blocks:
            {
            BM_ASSERT(bv_block_idx == sit.block_idx());
            unsigned i0, j0;
            bm::get_block_coord(bv_block_idx, i0, j0);
            bm::word_t* blk = bman.get_block_ptr(i0, j0);

            sit.next();

            switch (op)
            {
            case set_OR: case set_ASSIGN:
                bman.set_block_all_set(bv_block_idx);
                break;
            case set_COUNT_OR: case set_COUNT_B: case set_COUNT:
                // block becomes all set
                count += bm::bits_in_block;
                break;
            case set_SUB:
                //blk =
                bman.zero_block(bv_block_idx);
                break;
            case set_COUNT_SUB_AB: case set_AND:
                // nothing to do
                break;
            case set_COUNT_AND: case set_COUNT_A:
                count += blk ? bman.block_bitcount(blk) : 0;
                break;
            default:
                if (blk)
                {
                    switch (op)
                    {
                    case set_XOR:
                        blk = bman.deoptimize_block(bv_block_idx);
                        bm::bit_block_xor(blk, FULL_BLOCK_REAL_ADDR);
                        break;
                    case set_COUNT_XOR:
                        {
                        count += 
                            combine_count_operation_with_block(
                                                blk,
                                                FULL_BLOCK_REAL_ADDR,
                                                bm::COUNT_XOR);
                        }
                        break;
                    case set_COUNT_SUB_BA:
                        {
                        count += 
                            combine_count_operation_with_block(
                                                blk,
                                                FULL_BLOCK_REAL_ADDR,
                                                bm::COUNT_SUB_BA);
                        }
                        break;
                    default:
                        BM_ASSERT(0);
                    } // switch
                }
                else // blk == 0 
                {
                    switch (op)
                    {
                    case set_XOR:
                        // 0 XOR 1 = 1
                        bman.set_block_all_set(bv_block_idx);
                        break;
                    case set_COUNT_XOR:
                        count += bm::bits_in_block;
                        break;
                    case set_COUNT_SUB_BA:
                        // 1 - 0 = 1
                        count += bm::bits_in_block;
                        break;
                    default:
                        break;
                    } // switch
                } // else
            } // switch
            if (exit_on_one && count) // early exit
				   return count;
            }
            break;

        case serial_iterator_type::e_gap_block:
            {
            BM_ASSERT(bv_block_idx == sit.block_idx());
            unsigned i0, j0;
            bm::get_block_coord(bv_block_idx, i0, j0);
            const bm::word_t* blk = bman.get_block(i0, j0);

            sit.get_gap_block(gap_temp_block);

            unsigned len = gap_length(gap_temp_block);
            int level = gap_calc_level(len, bman.glen());
            --len;

            bool const_op = is_const_set_operation(op);
            if (const_op)
            {
                distance_metric metric = operation2metric(op);
                bm::word_t* gptr = (bm::word_t*)gap_temp_block;
                BMSET_PTRGAP(gptr);
                unsigned c = 
                    combine_count_operation_with_block(
                                        blk,
                                        gptr,
                                        metric);
                count += c;
                if (exit_on_one && count) // early exit
				    return count;

            }
            else // non-const set operation
            {
                if ((sop == set_ASSIGN) && blk) // target block override
                {
                    bman.zero_block(bv_block_idx);
                    sop = set_OR;
                }
                if (blk == 0) // target block not found
                {
                    switch (sop)
                    {
                    case set_AND: case set_SUB:
                        break;
                    case set_OR: case set_XOR: case set_ASSIGN:
                        bman.set_gap_block(
                            bv_block_idx, gap_temp_block, level);
                        break;
                    
                    default:
                        BM_ASSERT(0);
                    } // switch
                }
                else  // target block exists
                {
                    bm::operation bop = bm::setop2op(op);
                    if (level == -1) // too big to GAP
                    {
                        gap_convert_to_bitset(temp_block, gap_temp_block);
                        bv.combine_operation_with_block(bv_block_idx, 
                                                        temp_block, 
                                                        0, // BIT
                                                        bop);
                    }
                    else // GAP fits
                    {
                        set_gap_level(gap_temp_block, level);
                        bv.combine_operation_with_block(
                                                bv_block_idx, 
                                                (bm::word_t*)gap_temp_block, 
                                                1,  // GAP
                                                bop);
                    }
                }
                if (exit_on_one) 
                {
                    bm::get_block_coord(bv_block_idx, i0, j0);
                    blk = bman.get_block_ptr(i0, j0);
                    if (blk)
                    {
                        bool z = bm::check_block_zero(blk, true/*deep check*/);
                        if (!z) 
                            return 1;
                    } 
                } // if exit_on_one

            } // if else non-const op
            }
            break;

        default:
            BM_ASSERT(0);
            #ifndef BM_NO_STL
                throw std::logic_error(err_msg());
            #else
                BM_THROW(BM_ERR_SERIALFORMAT);
            #endif
        } // switch

        ++bv_block_idx;
        BM_ASSERT(bv_block_idx);

        if (is_range_set_ && (bv_block_idx > nb_range_to_))
            break;

    } // for (deserialization)

    return count;
}




} // namespace bm

#include "bmundef.h"

#ifdef _MSC_VER
#pragma warning( pop )
#endif


#endif
