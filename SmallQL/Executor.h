#pragma once
#include "DataFile.h"
#include "SystemInfoManager.h"
#include "DataSequence.h"
#include <map>

using namespace std;

struct QTableNode;
using QTablePtr = unique_ptr<QTableNode>;

enum class QueryType {
    Select,
    Insert
};

class Executor {
public:
    DataFile& addDataFile(uint16_t id);
    IndexFile& addIndexFile(uint16_t tableId, uint16_t indexId, const Schema& keySchema);
    map<uint16_t, unique_ptr<DataFile>> dataFiles;
    map<pair<uint16_t, uint16_t>, unique_ptr<IndexFile>> indexFiles;
    vector<unique_ptr<DataSequence>> sequences;
    SystemInfoManager& sysMan;
    QueryType queryType;
    uint16_t tableId;

    Executor(SystemInfoManager& sysMan): sysMan(sysMan) {}
    void prepare(QTablePtr tree);
    vector<ValueArray> execute();
};
