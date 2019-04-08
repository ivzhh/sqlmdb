#include "sqlmdb.h"

Sqlmdb::LmdbErr::LmdbErr()
{
    reset();
}

Sqlmdb::LmdbErr::LmdbErr(int rc) : LmdbErr()
{
    *this = rc;
}

Sqlmdb::LmdbErr::LmdbErr(LmdbErr && other) : LmdbErr()
{
    *this = other;
    other.reset();
}

Sqlmdb::LmdbErr::LmdbErr(const LmdbErr & other) : LmdbErr()
{
    *this = other;
}

void Sqlmdb::LmdbErr::reset()
{
    mRc      = 0;
    mChecked = false;
}

Sqlmdb::strv Sqlmdb::LmdbErr::toString()
{
    markChecked();
    return mdb_strerror(mRc);
}

int Sqlmdb::LmdbErr::rc()
{
    markChecked();
    return mRc;
}

void Sqlmdb::LmdbErr::markChecked()
{
    mChecked = true;
}

Sqlmdb::LmdbErr & Sqlmdb::LmdbErr::operator=(const LmdbErr & other)
{
    if (!mRc && !mChecked)
        throw ErrorNotChecked();

    mRc      = other.mRc;
    mChecked = other.mChecked;

    return *this;
}

Sqlmdb::LmdbErr & Sqlmdb::LmdbErr::operator=(int rc)
{
    if (!mRc && !mChecked)
        throw ErrorNotChecked();

    mRc      = rc;
    mChecked = false;

    return *this;
}

Sqlmdb::Transaction::Transaction() : Transaction(nullptr) {}

Sqlmdb::LmdbErr && Sqlmdb::Transaction::commit()
{
    LmdbErr rc;
    rc = mdb_txn_commit(mTxn);
    if (rc)
        mTxn = nullptr;
    return std::move(rc);
}

void Sqlmdb::Transaction::abort()
{
    if (mTxn)
        mdb_txn_abort(mTxn);
    mTxn = nullptr;
}

Sqlmdb::Transaction::~Transaction()
{
    if (mTxn)
        mdb_txn_abort(mTxn);
    mTxn = nullptr;
}

MDB_txn ** Sqlmdb::Transaction::operator&()
{
    return &mTxn;
}

MDB_txn * Sqlmdb::Transaction::operator()()
{
    return mTxn;
}

Sqlmdb::Transaction::Transaction(Transaction && other) : mTxn(other.mTxn)
{
    other.mTxn = nullptr;
}

Sqlmdb::Transaction::Transaction(MDB_txn * txn) : mTxn(txn) {}

Sqlmdb::Lmdb::Lmdb() : mEnv(nullptr), mDbs() {}

Sqlmdb::LmdbErr Sqlmdb::Lmdb::init(strv envPath, int flags /*= 0*/)
{
    mRc = mdb_env_create(&mEnv);

    if (mRc)
        return mRc;

    mRc = mdb_env_set_mapsize(mEnv, INCREMENT_STEP);

    if (mRc)
        return mRc;

    mRc = mdb_env_open(mEnv, envPath.to_string().c_str(), flags, 0664);

    Transaction txn = beginTransaction();
    mRc             = mdb_dbi_open(txn(), NULL, 0, &mDbs[0]);
    mRc             = txn.commit();

    return mRc;
}

Sqlmdb::Transaction && Sqlmdb::Lmdb::beginTransaction(int flags /*= 0*/)
{
    Transaction txn;
    mRc = mdb_txn_begin(mEnv, NULL, 0, &txn);

    return std::move(txn);
}

Sqlmdb::strv Sqlmdb::Lmdb::errorMessage()
{
    return mRc.toString();
}

Sqlmdb::Lmdb::Lmdb(Lmdb && other) : mEnv(other.mEnv), mDbs(other.mDbs)
{
    other.mEnv = nullptr;
    other.mDbs.fill(MDB_dbi());
}

Sqlmdb::Lmdb::Lmdb(Lmdb & other) : Lmdb(std::move(other)) {}

Sqlmdb::Index::Index(std::initializer_list<strv> & columns)
{
    mColumns.reserve(columns.size());
    for (auto c : columns)
    {
        mColumns.emplace_back(c.to_string());
    }
}

Sqlmdb::UniqueIndex::UniqueIndex(std::initializer_list<strv> & columns) : Index(columns) {}

Sqlmdb::TableBuilder::TableBuilder(strv tableName) :
    mTableName(tableName.to_string()),
    mStatus(TableBuilderStatus::Ok)
{
}

Sqlmdb::TableBuilder & Sqlmdb::TableBuilder::init(
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

Sqlmdb::TableBuilderStatus Sqlmdb::TableBuilder::build(Lmdb & db)
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

constexpr Sqlmdb::strv Sqlmdb::TableBuilder::defaultPk() const
{
    return "_rid_";
}

constexpr Sqlmdb::strv Sqlmdb::TableBuilder::hiddenPk() const
{
    return "_pk_";
}

void Sqlmdb::TableBuilder::setDefaultPkSchema()
{
    mPk     = defaultPk().to_string();
    mPkType = ColumnType::IntAuto;
}

void Sqlmdb::TableBuilder::buildUniqueIndex(strv indexName, std::initializer_list<strv> & columns)
{
    mIndices.emplace(indexName.to_string(), std::make_unique<UniqueIndex>(columns));
}
