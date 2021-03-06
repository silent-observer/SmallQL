#pragma once

#include "DataFile.h"
#include "IndexFile.h"
#include "SystemInfoManager.h"
#include "BlobManager.h"

#include <iostream>

struct RecordPtr {
    IntermediateType type;
    ValueArray* record;
    RecordId recordId;
    RecordPtr() : record(NULL) {}
    RecordPtr(const IntermediateType& type) : record(NULL), type(type) {}
};

class DataSequence {
protected:
    RecordPtr record;
    DataSequence() {}
    DataSequence(const IntermediateType& type): record(type) {}
public:
    virtual ~DataSequence() {}
    virtual void reset() = 0;
    inline const RecordPtr& get() const {
        return record;
    }
    virtual void advance() = 0;
    virtual bool hasEnded() const = 0;
    inline const IntermediateType& getType() const {
        return record.type;
    }
};

class TableFullScanDS : public DataSequence {
private:
    DataFile& data;
    BlobManager& blobManager;
    DataFile::const_iterator iter;
    const Schema& schema;
    unique_ptr<ValueArray> recordData;
public:
    TableFullScanDS(const Schema& schema, DataFile& data, BlobManager& blobManager);
    virtual void reset();
    virtual void advance();
    virtual bool hasEnded() const;
};

class TableIndexScanDS : public DataSequence {
private:
    DataFile& data;
    IndexFile& index;
    BlobManager& blobManager;
    IndexFile::const_iterator iter;
    const Schema& schema;
    ValueArray from, to;
    bool incFrom, incTo;
    unique_ptr<ValueArray> recordData;
public:
    TableIndexScanDS(const Schema& schema, DataFile& data, IndexFile& index, BlobManager& blobManager,
        ValueArray from, ValueArray to, bool incFrom, bool incTo);
    virtual void reset();
    virtual void advance();
    virtual bool hasEnded() const;
};

class ProjectorDS : public DataSequence {
private:
    DataSequence* source;
    vector<uint16_t> columns;
    unique_ptr<ValueArray> recordData;
    void update();
public:
    ProjectorDS(const IntermediateType& type, 
        DataSequence* source, 
        vector<uint16_t> columns);
    virtual void reset();
    virtual void advance();
    virtual bool hasEnded() const;
};

struct QScalarNode;
class ComputerVisitor;
class FuncProjectorDS : public DataSequence {
private:
    DataSequence* source;
    vector<unique_ptr<QScalarNode>> funcs;
    unique_ptr<ValueArray> recordData;
    unique_ptr<ComputerVisitor> visitor;
    void update();
public:
    FuncProjectorDS(const IntermediateType& type,
        DataSequence* source, 
        vector<unique_ptr<QScalarNode>> funcs);
    virtual void reset();
    virtual void advance();
    virtual bool hasEnded() const;
};

struct QConditionNode;
class CondCheckerVisitor;
class FilterDS : public DataSequence {
private:
    DataSequence* source;
    unique_ptr<QConditionNode> cond;
    unique_ptr<CondCheckerVisitor> visitor;
    void update();
public:
    FilterDS(const IntermediateType& type, 
        DataSequence* source, 
        unique_ptr<QConditionNode> cond);
    virtual void reset();
    virtual void advance();
    virtual bool hasEnded() const;
};

class UnionDS : public DataSequence {
private:
    vector<DataSequence*> sources;
    int currentIndex;
    void update();
public:
    UnionDS(const IntermediateType& type, vector<DataSequence*> sources);
    virtual void reset();
    virtual void advance();
    virtual bool hasEnded() const;
};

class CrossJoinDS : public DataSequence {
private:
    DataSequence* left;
    DataSequence* right;
    unique_ptr<ValueArray> recordData;
    int offset;
    void update(bool newLeft);
public:
    CrossJoinDS(const IntermediateType& type, DataSequence* left, DataSequence* right);
    virtual void reset();
    virtual void advance();
    virtual bool hasEnded() const;
};

class CondJoinDS : public DataSequence {
private:
    JoinType joinType;
    DataSequence* left;
    DataSequence* right;
    unique_ptr<QConditionNode> cond;
    unique_ptr<CondCheckerVisitor> visitor;

    unique_ptr<ValueArray> recordData;
    int offset;
    bool noPair;
    bool hasJustAddedNull;

    bool isLeftJoin, isRightJoin;

    vector<bool> rightNulls;
    int rightIndex;
    bool isFillingRight;
    bool rightNullWalk;

    void update(bool newLeft);
    void updateLeftNull();
    void updateRightNull();
    void skipToNext();
    void crossStep();
    void rightNullStep();
public:
    CondJoinDS(const IntermediateType& type, DataSequence* left, DataSequence* right, JoinType joinType, unique_ptr<QConditionNode> cond);
    virtual void reset();
    virtual void advance();
    virtual bool hasEnded() const;
};

class SorterDS : public DataSequence {
private:
    DataSequence* source;
    vector<ValueArray> values;
    int index;
    vector<pair<int, bool>> cmpPlan;
    bool hasBeenReset;
public:
    SorterDS(const IntermediateType& type, DataSequence* source, vector<pair<int, bool>> cmpPlan);
    virtual void reset();
    virtual void advance();
    virtual bool hasEnded() const;
};

class ConstTableDS : public DataSequence {
private:
    vector<ValueArray> values;
    int index;
    void update();
public:
    ConstTableDS(const IntermediateType& type,
        vector<ValueArray> values);
    virtual void reset();
    virtual void advance();
    virtual bool hasEnded() const;
};

vector<ValueArray> selectData(DataSequence* source);

class Inserter {
private:
    DataFile dataFile;
    const Schema& schema;
    BlobManager& blobManager;
    vector<IndexFile> indexFiles;
public:
    Inserter(TransactionManager& trMan, const SystemInfoManager& sysMan, BlobManager& blobManager, uint16_t tableId);
    bool insert(RecordPtr record);
    pair<bool, int> insert(DataSequence* source);
};

class Deleter {
private:
    vector<pair<RecordId, ValueArray>> data;
    TransactionManager& trMan;
    const SystemInfoManager& sysMan;
    BlobManager& blobManager;
    uint16_t tableId;
public:
    Deleter(TransactionManager& trMan, const SystemInfoManager& sysMan, BlobManager& blobManager, uint16_t tableId);
    void prepareAll(DataSequence* source);
    pair<bool, int> deleteAll();
};

class Updater {
private:
    vector<pair<RecordId, ValueArray>> data;
    TransactionManager& trMan;
    const SystemInfoManager& sysMan;
    BlobManager& blobManager;
    uint16_t tableId;
    vector < pair < uint16_t, unique_ptr < QScalarNode >> > setData;
    set<uint16_t> affectedIndexes;
    bool affectsVarData;
public:
    Updater(TransactionManager& trMan, 
        const SystemInfoManager& sysMan, 
        BlobManager& blobManager, 
        uint16_t tableId,
        vector<pair<uint16_t, unique_ptr<QScalarNode>>> setData,
        set<uint16_t> affectedIndexes,
        bool affectsVarData);
    void prepareAll(DataSequence* source);
    pair<bool, int> updateAll();
};