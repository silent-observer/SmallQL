#pragma once

#include "DataSequence.h"

struct GroupRecordPtr {
    IntermediateType type;
    vector<ValueArray>* record;
    vector<bool> isGrouped;
    GroupRecordPtr() : record(NULL) {}
    GroupRecordPtr(const IntermediateType& type, vector<bool> isGrouped) : record(NULL), type(type), isGrouped(isGrouped) {}
};

class GroupDataSequence {
protected:
    GroupRecordPtr record;
    GroupDataSequence() {}
    GroupDataSequence(const IntermediateType& type, vector<bool> isGrouped): record(type, isGrouped) {}
public:
    virtual ~GroupDataSequence() {}
    virtual void reset() = 0;
    inline const GroupRecordPtr& get() const {
        return record;
    }
    virtual void advance() = 0;
    virtual bool hasEnded() const = 0;
    inline const IntermediateType& getType() const {
        return record.type;
    }
};

class GroupifierDS : public GroupDataSequence {
private:
    DataSequence* source;
    unique_ptr<vector<ValueArray>> data;
    bool ended;
    void update();
public:
    GroupifierDS(const IntermediateType& type, vector<bool> isGrouped, DataSequence* source);
    virtual void reset();
    virtual void advance();
    virtual bool hasEnded() const;
};

class DegroupifierDS : public DataSequence {
private:
    GroupDataSequence* source;
    unique_ptr<ValueArray> data;
    void update();
public:
    DegroupifierDS(const IntermediateType& type, GroupDataSequence* source);
    virtual void reset();
    virtual void advance();
    virtual bool hasEnded() const;
};