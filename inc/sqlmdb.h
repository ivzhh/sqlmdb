#ifndef SQLMDB_H
#define SQLMDB_H

// C/C++ headers
#include <map>
#include <vector>
#include <memory>
#include <array>

// Boost headers
#include <boost/endian/buffers.hpp>
#include <boost/utility/string_view.hpp>

// 3rd party headers
#include <lmdb.h>

// my headers

namespace Sqlmdb
{
using strv = boost::string_view;

class LmdbErr
{
public:
    class ErrorNotChecked : public std::exception
    {
    };

public:
    LmdbErr() { reset(); }
    LmdbErr(int rc) : LmdbErr() { *this = rc; }
    LmdbErr(LmdbErr && other) : LmdbErr()
    {
        *this = other;
        other.reset();
    }
    LmdbErr(const LmdbErr & other) : LmdbErr() { *this = other; }

    /**
     * @brief Set current error code
     *
     * If previously error code != 0 and error has not been checked,
     * then throw exception to warn caller.
     *
     * An error is checked if LmdbErr has been implicitly used as boolean,
     * or error string has been checked.
     *
     * @param rc LMDB error code
     * @return LmdbErr&
     */
    LmdbErr & operator=(int rc)
    {
        if (!mRc && !mChecked)
            throw ErrorNotChecked();

        mRc      = rc;
        mChecked = false;

        return *this;
    }

    LmdbErr & operator=(const LmdbErr & other)
    {
        if (!mRc && !mChecked)
            throw ErrorNotChecked();

        mRc      = other.mRc;
        mChecked = other.mChecked;

        return *this;
    }

    inline void reset()
    {
        mRc      = 0;
        mChecked = false;
    }

    strv toString()
    {
        markChecked();
        return mdb_strerror(mRc);
    }

    int rc()
    {
        markChecked();
        return mRc;
    }

    /**
     * @brief Error happens
     *
     * @return true
     * @return false
     */
    operator bool()
    {
        markChecked();
        return !!mRc;
    }

protected:
    void markChecked() { mChecked = true; }

private:
    int mRc;
    bool mChecked;
};

class Transaction
{
public:
    Transaction() : Transaction(nullptr) {}
    Transaction(MDB_txn * txn) : mTxn(txn) {}
    Transaction(Transaction && other) : mTxn(other.mTxn) { other.mTxn = nullptr; }
    Transaction(Transaction & other)       = delete;
    Transaction(const Transaction & other) = delete;

    LmdbErr && commit()
    {
        LmdbErr rc;
        rc = mdb_txn_commit(mTxn);
        if (rc)
            mTxn = nullptr;
        return std::move(rc);
    }

    void abort()
    {
        if (mTxn)
            mdb_txn_abort(mTxn);
        mTxn = nullptr;
    }

    ~Transaction()
    {
        if (mTxn)
            mdb_txn_abort(mTxn);
        mTxn = nullptr;
    }

    MDB_txn ** operator&() { return &mTxn; }
    MDB_txn * operator()() { return mTxn; }

private:
    MDB_txn * mTxn;
};

/**
 * @brief A thin C++ wrapper on Lmdb C API
 *
 * The handle for this library should be either
 * `std::unique_ptr` or `std::shared_ptr`.
 *
 */
class Lmdb
{
private:
    enum
    {
        NECESSARY_DBS  = 1,
        INCREMENT_STEP = 10485760
    };

public:
    Lmdb() : mEnv(nullptr), mDbs() {}
    Lmdb(Lmdb & other) : Lmdb(std::move(other)) {}
    Lmdb(Lmdb && other) : mEnv(other.mEnv), mDbs(other.mDbs)
    {
        other.mEnv = nullptr;
        other.mDbs.fill(MDB_dbi());
    }

    Lmdb(const Lmdb & other) = delete;
    Lmdb & operator=(const Lmdb & other) = delete;
    Lmdb & operator=(Lmdb && other) = delete;

    operator bool() { return !!mEnv; }

public:
    LmdbErr init(strv envPath, int flags = 0)
    {
        mRc = mdb_env_create(&mEnv);

        if (mRc)
            return mRc;

        mRc = mdb_env_set_mapsize(mEnv, INCREMENT_STEP);

        if (mRc)
            return mRc;

        mRc = mdb_env_open(mEnv, envPath.to_string().c_str(), flags, 0664);

        Transaction txn = beginTransaction();
        mRc             = mdb_open(txn(), NULL, 0, &mDbs[0]);
        mRc             = txn.commit();

        return mRc;
    }

    /**
     * @brief Create a transaction for operation
     *
     * If creation fails, the error will trigger exception on first operation on transaction
     *
     * @param flags
     * @return Transaction&&
     */
    Transaction && beginTransaction(int flags = 0)
    {
        Transaction txn;
        mRc = mdb_txn_begin(mEnv, NULL, 0, &txn);

        return std::move(txn);
    }

public:
    strv errorMessage() { return mRc.toString(); }

private:
    MDB_env * mEnv;
    std::array<MDB_dbi, NECESSARY_DBS> mDbs;
    LmdbErr mRc;
};

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
    ErrDbNotValid
};

class Index
{
public:
    /**
     * @brief Create an index with the **order** given by param columns
     *
     * @param columns
     */
    Index(std::initializer_list<strv> & columns)
    {
        mColumns.reserve(columns.size());
        for (auto c : columns)
        {
            mColumns.emplace_back(c.to_string());
        }
    }

protected:
    std::vector<std::string> mColumns;
};

class UniqueIndex : public Index
{
public:
    UniqueIndex(std::initializer_list<strv> & columns) : Index(columns) {}
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
    TableBuilder(strv tableName) :
        mTableName(tableName.to_string()),
        mStatus(TableBuilderStatus::Ok)
    {
    }
    /**
     * @brief Construct a new TableBuilder object
     *
     * The auto-incremental `rid` (row id) is the default/hidden
     * primary key. It is generated by default except:
     *
     * - Only one filed is primary key, and it is `Int` or `IntAuto`
     *
     * Other cases:
     *
     * - Multiple columns are primary keys
     * - Primary key is one column, but it is not `Int` or `IntAuto`
     *
     * @param types Each type must be an enum in ColumnType
     * @param columnNames
     * @param pks Primary keys. These names must exist in columnNames
     */
    TableBuilder & init(
        std::initializer_list<ColumnType> && types,
        std::initializer_list<strv> && columnNames,
        std::initializer_list<strv> && pks)
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

                    buildUniqueIndex(hiddenPk(), pks);
                }
                break;
                }
            }
            else
            {
                setDefaultPkSchema();

                buildUniqueIndex(hiddenPk(), pks);
            }
        }

        return *this;
    }

    TableBuilderStatus build(Lmdb & db)
    {
        if (TableBuilderStatus::Ok != mStatus)
            return mStatus;

        if (db)
        {
        }
        else
            return TableBuilderStatus::ErrDbNotValid;

        return TableBuilderStatus::Ok;
    }

public:
    constexpr strv defaultPk() { return "_rid_"; }
    constexpr strv hiddenPk() { return "_pk_"; }

protected:
    /**
     * @brief Row ID is the default/hidden primary key
     *
     */
    void setDefaultPkSchema()
    {
        mPk     = defaultPk().to_string();
        mPkType = ColumnType::IntAuto;
    }

    /**
     * @brief Recording the columns in order and make an unique index
     *
     * @param indexName
     * @param columns
     */
    void buildUniqueIndex(strv indexName, std::initializer_list<strv> & columns)
    {
        mIndices.emplace(indexName.to_string(), std::make_unique<UniqueIndex>(columns));
    }

private:
    TableBuilderStatus mStatus;
    std::string mTableName;
    std::map<std::string, ColumnType> mColumns;
    std::string mPk;
    ColumnType mPkType;
    std::map<std::string, std::unique_ptr<Index>> mIndices;
};

} // namespace Sqlmdb

#endif // SQLMDB_H
