#pragma once
#include "DataFile.h"
#include "TransactionManager.h"
#include "SystemInfoManager.h"
#include "DataSequence.h"
#include "GroupDataSequence.h"
#include "BlobManager.h"
#include <map>

using namespace std;

struct QTableNode;
using QTablePtr = unique_ptr<QTableNode>;

enum class QueryType {
    Select,
    Insert,
    Delete,
    Update
};

class Executor {
public:
    DataFile& addDataFile(uint16_t id, const Schema& schema);
    IndexFile& addIndexFile(uint16_t tableId, uint16_t indexId, const Schema& keySchema, bool isUnique);
    map<uint16_t, unique_ptr<DataFile>> dataFiles;
    map<pair<uint16_t, uint16_t>, unique_ptr<IndexFile>> indexFiles;
    vector<unique_ptr<DataSequence>> sequences;
    vector<unique_ptr<GroupDataSequence>> groupSequences;
    TransactionManager& trMan;
    SystemInfoManager& sysMan;
    BlobManager& blobManager;
    QueryType queryType;
    uint16_t tableId;
    IntermediateType resultType;
    string message;

    vector<pair<uint16_t, unique_ptr<QScalarNode>>> setData;
    set<uint16_t> affectedIndexes;
    bool affectsVarData;

    Executor(TransactionManager& trMan, SystemInfoManager& sysMan, BlobManager& blobManager)
        : trMan(trMan), sysMan(sysMan), blobManager(blobManager) {}
    void prepare(QTablePtr tree);
    pair<bool, vector<ValueArray>> execute();
};
