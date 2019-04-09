#include "sqlmdb_kv_encoder.h"
#include "sqlmdb.h"

void Sqlmdb::KvEncoder::encode(const Index & idx, std::string & key, std::string & value) {}

bool Sqlmdb::ReadWriteUtil::pack(std::vector<char> & buffer, const std::string & data) {}

bool Sqlmdb::ReadWriteUtil::pack(std::vector<char> & buffer, const double data) {}

bool Sqlmdb::ReadWriteUtil::pack(std::vector<char> & buffer, const int64_t data) {}
` bool Sqlmdb::ReadWriteUtil::pack(std::vector<char> & buffer, const std::vector<char> & data) {}
