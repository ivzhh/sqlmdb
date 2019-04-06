#ifndef SQLMDB_H
#define SQLMDB_H

// C/C++ headers
#include <map>

// Boost headers
#include <boost/endian/buffers.hpp>
#include <boost/utility/string_view.hpp>

// 3rd party headers
#include <lmdb.h>

// my headers

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
    IntAuto, ///< Auto-incremetal int64_t, **only for primary key**
    Float, ///< float64_t, **cannot be primary key**
    Blob, ///< string or binary blob
};

/**
 * @brief The status of checking TableBuilder and update database metadata
 *
 */
enum class TableBuilderStatus
{
    Ok = 0, ///< Table schema passes checking and database update successfully
    ErrSchemaMismatchColumns, ///< Table schema has some issues
    ErrSchemaColumnNameDuplicate,
    ErrSchemaPkNotFound,
    /**
     * @brief There is only one auto-incremental primary key
     *
     * - If there is an `Int` PK, then `#AutoInt == 0`;
     * - If there is an `IntAuto` PK, then `#AutoInt == 1`;
     * - If there is a `Blob` PK or a combination `PK`, then a hidden `IntAuto` PK is created as
     * true PK. The user defined PK(s) are treated as unique index.
     */
    ErrSchemaAutoIntPk,
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
    TableBuilder(boost::string_view tableName) :
        mTableName(tableName.to_string()),
        mStatus(TableBuilderStatus::Ok)
    {
    }
    /**
     * @brief Construct a new TableBuilder object
     *
     * @param tableName
     * @param types Each type must be an enum in ColumnType
     * @param columnNames
     * @param pks Primary keys. These names must exist in columnNames
     */
    TableBuilder & init(
        std::initializer_list<ColumnType> && types,
        std::initializer_list<boost::string_view> && columnNames,
        std::initializer_list<boost::string_view> && pks)
    {
        if (types.size() != columnNames.size())
        {
            mStatus = TableBuilderStatus::ErrSchemaMismatchColumns;
            return *this;
        }

        size_t numAutoInt = 0;

        {
            /*
             * Construct all column names and types
             * This block is to prevent scope leakage of typeIter and nameIter
             */
            auto typeIter = types.begin();
            auto nameIter = columnNames.begin();

            for (; types.end() != typeIter && columnNames.end() != nameIter; ++typeIter, ++nameIter)
            {
                /*
                 * Each table has maximum **one** auto incremental column
                 * that one must be primary key
                 */
                if (ColumnType::IntAuto == *typeIter)
                {
                    numAutoInt++;
                }
                /*
                 * nsert into map. Pretend this opertaion is `move()`
                 * and try to finish all tasks before this.
                 */
                auto result = mColumns.emplace(*nameIter, *typeIter);

                if (!result.second)
                {
                    mStatus = TableBuilderStatus::ErrSchemaColumnNameDuplicate;
                    return *this;
                }
            }
        }

        auto numAutoIntInPk = 0;
        /*
         * If any primary key is given, the name must exist
         */
        for (auto pk_ : pks)
        {
            auto pk         = pk_.to_string();
            const auto iter = mColumns.find(pk);

            if (mColumns.end() == iter)
            {
                mStatus = TableBuilderStatus::ErrSchemaPkNotFound;
            }

            if (ColumnType::IntAuto == iter->second)
            {
                numAutoIntInPk++;
            }
        }

        if (numAutoInt != numAutoIntInPk)
        {
            mStatus = TableBuilderStatus::ErrSchemaAutoIntPk;
            return *this;
        }

        if (1 < numAutoInt)
        {
            mStatus = TableBuilderStatus::ErrSchemaAutoIntPk;
            return *this;
        }
        else // numAutoInt == 0 || == 1
        {
            if (0 == pks.size())
            {
                setDefaultPkSchema();
            }
            else if (1 == pks.size())
            {
                const auto pk   = pks.begin()->to_string();
                const auto type = mColumns[pk];

                switch (type)
                {
                case ColumnType::Int:
                case ColumnType::IntAuto:
                    mPk     = pk;
                    mPkType = type;
                    break;
                default:
                {
                    setDefaultPkSchema();

                    buildUniqueIndex();
                }
                break;
                }
            }
            else
            {
                setDefaultPkSchema();

                buildUniqueIndex();
            }
        }

        return *this;
    }

    TableBuilderStatus build()
    {
        if (TableBuilderStatus::Ok != mStatus)
            return mStatus;

        return TableBuilderStatus::Ok;
    }

public:
    constexpr boost::string_view defaultPk() { return "rid"; }

protected:
    void setDefaultPkSchema()
    {
        mPk     = defaultPk().to_string();
        mPkType = ColumnType::IntAuto;
    }

    void buildUniqueIndex() {}

private:
    TableBuilderStatus mStatus;
    std::string mTableName;
    std::map<std::string, ColumnType> mColumns;
    std::string mPk;
    ColumnType mPkType;
};

} // namespace Sqlmdb

#endif // SQLMDB_H
