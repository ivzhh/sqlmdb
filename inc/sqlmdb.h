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
#include <msgpack.hpp>

// my headers
#include "sqlmdb_kv_encoder.h"

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
    LmdbErr();
    LmdbErr(int rc);
    LmdbErr(LmdbErr && other);
    LmdbErr(const LmdbErr & other);

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
    LmdbErr & operator=(int rc);

    LmdbErr & operator=(const LmdbErr & other);

    void reset();

    strv toString();

    int rc();

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
    void markChecked();

private:
    int mRc;
    bool mChecked;
};

class Transaction
{
public:
    Transaction();
    Transaction(MDB_txn * txn);
    Transaction(Transaction && other);
    Transaction(const Transaction & other) = delete;

    LmdbErr && commit();

    void abort();

    ~Transaction();

    MDB_txn ** operator&();
    MDB_txn * operator()();

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
    Lmdb();
    Lmdb(Lmdb & other);
    Lmdb(Lmdb && other);
    Lmdb & operator=(const Lmdb & other) = delete;
    Lmdb & operator=(Lmdb && other) = delete;

    operator bool() { return !!mEnv; }

public:
    LmdbErr init(strv envPath, int flags = 0);

    /**
     * @brief Create a transaction for operation
     *
     * If creation fails, the error will trigger exception on first operation on transaction
     *
     * @param flags
     * @return Transaction&&
     */
    Transaction && beginTransaction(int flags = 0);

public:
    strv errorMessage();

private:
    MDB_env * mEnv;
    std::array<MDB_dbi, NECESSARY_DBS> mDbs;
    LmdbErr mRc;
};

/**
 * @brief The types of data that are supported by Sqlmdb
 *
 * Only Int and Blob can be primary keys
 */
enum class ColumnType
{
    Int, ///< int64_t
    AutoInt, ///< Auto-incremetal int64_t, **only for primary key**
    Float, ///< float64_t, **cannot be primary key**
    Blob, ///< string or binary blob
};

/**
 * @brief The status of checking TableBuilder and update database metadata
 *
 * Neither reordering the items nor inserting item before existing items are allowed.
 * **Append-only** enforced.
 */
enum class TableBuilderStatus
{
    Ok                       = 0, ///< Table schema passes checking and database update successfully
    ErrSchemaMismatchColumns = 1, ///< Table schema has some issues
    ErrSchemaColumnNameDuplicate = 2,
    ErrSchemaPkNotFound          = 3,
    /**
     * @brief There is only one auto-incremental primary key
     *
     * - If there is an `Int` PK, then `# AutoInt == 0`;
     * - If there is an `AutoInt` PK, then `# AutoInt == 1`;
     * - If there is a `Blob` PK or a combination `PK`, then a hidden `AutoInt` PK is created as
     * true PK. The user defined PK(s) are treated as unique index.
     */
    ErrSchemaAutoIntPk = 4,
    ErrDbNotValid      = 5
};

class TableBuilder;
class Table;

class Index
{
public:
    /**
     * @brief Create an index with the **order** given by param columns
     *
     * @param tableName
     * @param indexName
     * @param columns
     */
    Index(strv tableName, strv indexName, std::initializer_list<strv> & columns);
    virtual ~Index() {}

    LmdbErr serialize(Lmdb & db);

public:
    void serialize(std::string & key, std::string & value);

protected:
    std::string mTableName;
    std::string mIndexName;
    std::vector<std::string> mColumns;
};

class UniqueIndex : public Index
{
public:
    UniqueIndex(strv tableName, strv indexName, std::initializer_list<strv> & columns);

    virtual ~UniqueIndex() {}
};

class Table
{
public:
    Table(TableBuilder && tb);

private:
    std::string mTableName;
    std::map<std::string, ColumnType> mColumns;
    std::string mPk;
    ColumnType mPkType;
    std::map<std::string, std::unique_ptr<Index>> mIndices;
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
    TableBuilder(strv tableName);
    /**
     * @brief Construct a new TableBuilder object
     *
     * The auto-incremental `rid` (row id) is the default/hidden
     * primary key. It is generated by default except:
     *
     * - Only one filed is primary key, and it is `Int` or `AutoInt`
     *
     * Other cases:
     *
     * - Multiple columns are primary keys
     * - Primary key is one column, but it is not `Int` or `AutoInt`
     *
     * @param types Each type must be an enum in ColumnType
     * @param columnNames
     * @param pks Primary keys. These names must exist in columnNames
     */
    TableBuilder & init(
        std::initializer_list<ColumnType> && types,
        std::initializer_list<strv> && columnNames,
        std::initializer_list<strv> && pks);

    TableBuilderStatus build(Lmdb & db);

public:
    constexpr strv defaultPk() const;
    constexpr strv hiddenPk() const;

protected:
    /**
     * @brief Row ID is the default/hidden primary key
     *
     */
    void setDefaultPkSchema();

    /**
     * @brief Recording the columns in order and make an unique index
     *
     * @param indexName
     * @param columns
     */
    void buildUniqueIndex(strv indexName, std::initializer_list<strv> & columns);

private:
    TableBuilderStatus mStatus;
    std::string mTableName;
    std::map<std::string, ColumnType> mColumns;
    std::string mPk;
    ColumnType mPkType;
    std::map<std::string, std::unique_ptr<Index>> mIndices;

    friend class Table;
    friend class Index;
};

} // namespace Sqlmdb

#endif // SQLMDB_H
