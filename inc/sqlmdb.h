#ifndef SQLMDB_H
#define SQLMDB_H

#include <lmdb.h>
#include <boost/endian/buffers.hpp>
#include <boost/utility/string_view.hpp>

namespace Sqlmdb
{
///
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

/**
 * @brief The types of data that are supported by Sqlmdb
 *
 * Only Int and Blob can be primary keys
 */
enum class ColumnType
{
    Int, ///< int64_t
    Float, ///< float64_t, **cannot be primary key**
    Blob, ///< string or binary blob
};

/**
 * @brief A class to collect all important parameters of building a SQL table.
 *
 * The idea is a Table is constrained by schema defined in TableBuilder.
 * So user input is processed by TableBuilder and the verified information is
 * stored in Table.
 */
class TableBuilder
{
public:
    /**
     * @brief Construct a new TableBuilder object
     *
     * @param tableName
     * @param types Each type must be an enum in ColumnType
     * @param columnNames
     * @param pks Primary keys. These names must exist in columnNames
     */
    TableBuilder(
        boost::string_view tableName,
        std::initializer_list<ColumnType> && types,
        std::initializer_list<boost::string_view> && columnNames,
        std::initializer_list<boost::string_view> && pks) :
        mTableName(tableName),
        mTypes(types),
        mColumnNames(columnNames),
        mPks(pks)
    {
    }

    std::string build() {}

private:
    std::string mTableName;
    std::initializer_list<ColumnType> mTypes;
    std::initializer_list<boost::string_view> mColumnNames;
    std::initializer_list<boost::string_view> mPks;
};

} // namespace Sqlmdb

#endif // SQLMDB_H