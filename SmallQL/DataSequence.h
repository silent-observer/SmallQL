#pragma once

#include "DataFile.h"
#include "IndexFile.h"
#include "SystemInfoManager.h"
#include "SystemInfoManager.h"


#include <iostream>

struct RecordPtr {
    Schema type;
    const char* record;
    RecordPtr() : record(NULL) {}
    RecordPtr(const Schema& type) : record(NULL), type(type) {}
};

class DataSequence {
protected:
    RecordPtr record;
    DataSequence() {
        cout << "!!!" << endl;
    }
    DataSequence(const Schema& type): record(type) {}
public:
    virtual ~DataSequence() {}
    virtual void reset() = 0;
    inline const RecordPtr& get() const {
        return record;
    }
    virtual void advance() = 0;
    virtual bool hasEnded() const = 0;
    inline const Schema& getType() const {
        return record.type;
    }
};

class TableFullScanDS : public DataSequence {
private:
    DataFile& data;
    DataFile::const_iterator iter;
public:
    TableFullScanDS(const Schema& type, DataFile& data);
    virtual void reset();
    virtual void advance();
    virtual bool hasEnded() const;
};

class TableIndexScanDS : public DataSequence {
private:
    DataFile& data;
    IndexFile& index;
    IndexFile::const_iterator iter;
    ValueArray from, to;
    bool incFrom, incTo;
public:
    TableIndexScanDS(const Schema& type, DataFile& data, IndexFile& index,
        ValueArray from, ValueArray to, bool incFrom, bool incTo);
    virtual void reset();
    virtual void advance();
    virtual bool hasEnded() const;
};

class ProjectorDS : public DataSequence {
private:
    DataSequence* source;
    struct CopyInfo {
        int from, nullFrom, to, nullTo, size;
    };
    vector<CopyInfo> copyInfo;
    vector<char> buffer;
    void update();
public:
    ProjectorDS(const Schema& type, 
        DataSequence* source, 
        vector<uint16_t> columns);
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
    FilterDS(const Schema& type, 
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
    UnionDS(const Schema& type, vector<DataSequence*> sources);
    virtual void reset();
    virtual void advance();
    virtual bool hasEnded() const;
};

class ConstTableDS : public DataSequence {
private:
    vector<vector<char>> data;
    int index;
    void update();
public:
    ConstTableDS(const Schema& type,
        vector<ValueArray> values);
    virtual void reset();
    virtual void advance();
    virtual bool hasEnded() const;
};

vector<ValueArray> selectData(DataSequence* source);

class Inserter {
private:
    DataFile dataFile;
    vector<IndexFile> indexFiles;
public:
    Inserter(const SystemInfoManager& sysMan, uint16_t tableId);
    bool insert(RecordPtr record);
    bool insert(DataSequence* source);
};