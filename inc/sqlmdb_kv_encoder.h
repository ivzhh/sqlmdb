#ifndef SQLMDB_KV_ENCODER_H
#define SQLMDB_KV_ENCODER_H

#include <string>
#include <vector>

namespace Sqlmdb
{
class ReadWriteUtil
{
public:
    /*
     * \brief This code borrows from msgpack, but with great changes
     *
     * The drive for the design is binary comparable.
     * Binary comparable is essential to prefix search.
     * So it is not ideal to put a length byte before a string,
     * because you cannot search any string (any length) with
     * the same prefix.
     *
     **/
    enum class Type
    {
        /**
         * Bound string         0x00 - 0x7f     128 items
         **/
        BND_STR = 0x00,
        /**
         * fixmap               0x80 - 0x8f     16 items
         **/
        FIX_MAP = 0X80,
        /**
         * fixarray             0x90 - 0x9f     16 items
         **/
        FIX_ARR = 0X90,
        /**
         * Unbounded string 	0xa0
         *
         * all bytes after this byte are part of the string/binary array
         **/
        UNB_STR = 0xa0,
        /**
         * \brief Big-endian `int64_t`
         */
        FIX_INT = 0xb0,
        /**
         * \brief Big-endian 8-bytes float
         */
        FIX_FLT = 0xb8,
        /*
         * \brief Nothing encoded
         **/
        NIL = 0xc0
    };

public:
    bool pack(std::vector<char> & buffer, const std::string & data);
    bool pack(std::vector<char> & buffer, const std::vector<char> & data);
    bool pack(std::vector<char> & buffer, const int64_t data);
    bool pack(std::vector<char> & buffer, const double data);

    template <typename T>
    static typename std::enable_if<std::is_integral<T>::value, bool>::type
    pack(std::vector<char> & buffer, const T data)
    {
        return pack(buffer, static_cast<int64_t>(data));
    }
};

class Encoder
{
public:
    template <typename T>
    static typename std::enable_if<std::is_integral<T>::value, void>::type
    encode(std::string & buffer, T value)
    {
    }

    template <typename T>
    static typename std::enable_if<std::is_floating_point<T>::value, void>::type
    encode(std::string & buffer, T value)
    {
    }
};

class Decoder
{
};

class Index;
class UniqueIndex;
class Table;

class KvEncoder
{
public:
    static void encode(const Index & idx, std::string & key, std::string & value);
};

} // namespace Sqlmdb

#endif // SQLMDB_KV_ENCODER_H
