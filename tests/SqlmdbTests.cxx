#include <gtest/gtest.h>
#include <QTemporaryDir>

#include "sqlmdb.h"

namespace
{
using namespace Sqlmdb;
/// Test basic SQL support.
class SqlmdbTests : public ::testing::Test
{
protected:
    SqlmdbTests()
    {
        QTemporaryDir dir;
        MDB_txn * txn = nullptr;
        auto rc       = mdb_env_create(&mEnv);
        rc            = mdb_env_set_mapsize(mEnv, (size_t)10485760);
        rc            = mdb_env_open(mEnv, dir.path().toStdString().c_str(), 0, 0664);
        rc            = mdb_txn_begin(mEnv, NULL, 0, &txn);
        rc            = mdb_open(txn, NULL, 0, &mDbi);
        mdb_txn_commit(txn);
    }
    ~SqlmdbTests()
    {
        mdb_close(mEnv, mDbi);
        mdb_env_close(mEnv);
    }

protected:
    MDB_env * mEnv;
    MDB_dbi mDbi;
};

TEST_F(SqlmdbTests, EndianTests)
{
    {
        EXPECT_NE(mEnv, nullptr);
        EXPECT_NE(mDbi, 0);
    }
    {
        std::string buf;
        int a;

        Encoder::encode(buf, a);
        Encoder::encode(buf, 1.0);
    }

    {
        boost::endian::big_int32_buf_t buf;
        buf = (int32_t)1;
        EXPECT_EQ(buf.data()[0], 0);
        EXPECT_EQ(buf.data()[1], 0);
        EXPECT_EQ(buf.data()[2], 0);
        EXPECT_EQ(buf.data()[3], 1);
        EXPECT_EQ(buf.value(), 1);
    }

    {
        TableBuilder tb(
            "a",
            { ColumnType::Int, ColumnType::Float, ColumnType::Blob },
            { "c", "d", "e" },
            { "a" });
    }
}
} // namespace