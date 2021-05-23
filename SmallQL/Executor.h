#pragma once
#include "DataFile.h"
#include "PageManager.h"
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
    PageManager& pageManager;
    SystemInfoManager& sysMan;
    BlobManager& blobManager;
    QueryType queryType;
    uint16_t tableId;
    IntermediateType resultType;
    string message;

    vector<pair<uint16_t, unique_ptr<QScalarNode>>> setData;
    set<uint16_t> affectedIndexes;
    bool affectsVarData;

    Executor(PageManager& pageManager, SystemInfoManager& sysMan, BlobManager& blobManager)
        : pageManager(pageManager), sysMan(sysMan), blobManager(blobManager) {}
    void prepare(QTablePtr tree);
    vector<ValueArray> execute();
};
