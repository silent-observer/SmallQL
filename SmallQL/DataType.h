#pragma once

#include <cstdint>
#include <vector>
#include <set>
#include <map>
#include <string>
#include <memory>
#include "Common.h"

using namespace std;

enum class ValueType {
    Null,
    Integer,
    Double,
    String,
    MinVal,
    MaxVal
};

class DataType;
struct Value {
    ValueType type;
    union {
        int64_t intVal;
        double doubleVal;
        string stringVal;
    };

    Value() : type(ValueType::Null) {};
    Value(ValueType type);
    Value(const Value& v);
    ~Value();
    Value& operator=(const Value& other);

    Value(int64_t intVal);
    inline Value(uint64_t v) : Value((int64_t)v) {}
    inline Value(int32_t v) : Value((int64_t)v) {}
    inline Value(uint32_t v) : Value((int64_t)v) {}
    inline Value(uint16_t v) : Value((int64_t)v) {}
    inline Value(int16_t v) : Value((int64_t)v) {}
    inline Value(uint8_t v) : Value((int64_t)v) {}
    inline Value(int8_t v) : Value((int64_t)v) {}
    Value(double doubleVal);
    Value(string stringVal);
    friend ostream& operator<<(ostream& os, const Value& v);
    friend int compareValue(const Value& a, const Value& b);
    unique_ptr<DataType> defaultType() const;
    virtual string toString() const;

    void convertToDouble();
};
using ValueArray = vector<Value>;

ostream& operator<<(ostream& os, const ValueArray& values);
int compareValue(const Value& a, const Value& b);

class DataType {
protected:
    uint32_t size;
    DataType(uint32_t size) : size(size) {}
public:
    inline uint32_t getSize() const { return size; };
    virtual bool checkVal(Value val) const = 0;
    virtual void encode(Value val, char* out) const = 0;
    virtual Value decode(const char* data) const = 0;
    virtual void print(ostream& os) const = 0;
    friend ostream& operator<<(ostream& os, const DataType& t);
};

class NullType : public DataType {
public:
    NullType() : DataType(0) {}
    virtual bool checkVal(Value val) const;
    virtual void encode(Value val, char* out) const;
    virtual Value decode(const char* data) const;
    virtual void print(ostream& os) const;
};

class FixedLengthType : public DataType {
protected:
    FixedLengthType(uint32_t size) : DataType(size) {}
};

struct SchemaEntry {
    shared_ptr<DataType> type;
    string name;
    int id;
    int offset;
    bool isPrimary;
    bool canBeNull;
    int nullBit;
};

static inline SchemaEntry schemaEntryNormal(string name, shared_ptr<DataType> type, bool canBeNull = true) {
    return SchemaEntry{move(type), name, 0, 0, false, canBeNull, -1};
}
static inline SchemaEntry schemaEntryPrimary(string name, shared_ptr<DataType> type) {
    return SchemaEntry{move(type), name, 0, 0, true, false, -1};
}

struct Schema {
    vector<SchemaEntry> columns;
    int totalNullBits;
    int totalNullBytes;
    int size;
    Schema() : columns(), totalNullBits(0), totalNullBytes(0), size(0) {}
    Schema(vector<SchemaEntry> columns);
    void updateData();
    inline uint32_t getSize() const {
        return size;
    };
    bool checkVal(ValueArray values) const;
    void encode(ValueArray values, char* out) const;
    inline vector<char> encode(ValueArray values) const {
        vector<char> result(getSize());
        encode(values, result.data());
        return result;
    }
    ValueArray decode(const char* data) const;
    inline ValueArray decode(const vector<char>& data) const {
        return decode(data.data());
    }
    int compare(const ValueArray& a, const ValueArray& b) const;
    int compare(const char* a, ValueArray b) const;
    inline int compare(ValueArray a, const char* b) const {
        return -compare(b, a);
    }
    int compare(const char* a, const char* b) const;
    inline int compare(const vector<char>& a, const vector<char>& b) const {
        return compare(a.data(), b.data());
    };
    void addColumn(SchemaEntry entry);
    Schema primaryKeySubschema() const;
    ValueArray narrow(const ValueArray& values) const;
    vector<int> getNullBitOffsets() const;
    friend ostream& operator<<(ostream& os, const Schema& schema);
};

class ByteType : public FixedLengthType {
public:
    ByteType(): FixedLengthType(1) {}

    bool checkVal(Value val) const;
    void encode(Value val, char* out) const;
    Value decode(const char* data) const;
    void print(ostream& os) const;
};

class ShortIntType : public FixedLengthType {
public:
    ShortIntType(): FixedLengthType(2) {}

    bool checkVal(Value val) const;
    void encode(Value val, char* out) const;
    Value decode(const char* data) const;
    void print(ostream& os) const;
};

class IntType : public FixedLengthType {
public:
    IntType(): FixedLengthType(4) {}

    bool checkVal(Value val) const;
    void encode(Value val, char* out) const;
    Value decode(const char* data) const;
    void print(ostream& os) const;
};

class DoubleType : public FixedLengthType {
public:
    DoubleType(): FixedLengthType(8) {}

    bool checkVal(Value val) const;
    void encode(Value val, char* out) const;
    Value decode(const char* data) const;
    void print(ostream& os) const;
};

class VarCharType : public FixedLengthType {
private:
    uint16_t maxSize;
public:
    VarCharType(uint16_t maxSize) : maxSize(maxSize), FixedLengthType(maxSize + 2) {}

    bool checkVal(Value val) const;
    void encode(Value val, char* out) const;
    Value decode(const char* data) const;
    void print(ostream& os) const;
};

template<typename T>
static inline int cmp(T x, T y) {
    return x > y ? 1 : x < y ? -1 : 0;
}

class TypeException : public SQLException
{
private:
    string message;
public:
    TypeException(string message)
        : message("Type Exception: " + message) {}
    const char* what() const throw ()
    {
        return message.c_str();
    }
};

shared_ptr<DataType> parseDataType(string str);
shared_ptr<DataType> typeCheckFunc(string name, vector<shared_ptr<DataType>> inputs);
shared_ptr<DataType> typeCheckAggrFunc(string name, shared_ptr<DataType> input);

struct IntermediateTypeEntry {
    string columnName, tableName;
    shared_ptr<DataType> type;
    int id;
    bool canBeNull;
    bool isAggregate;
    bool isConst;
    IntermediateTypeEntry() {}
    IntermediateTypeEntry(string columnName, string tableName,
        shared_ptr<DataType> type, bool canBeNull, bool isAggregate, bool isConst)
        : columnName(columnName)
        , tableName(tableName)
        , type(type)
        , id(-1)
        , canBeNull(canBeNull)
        , isAggregate(isAggregate)
        , isConst(isConst) {}
    int compare(const Value& a, const Value& b) const;
};

struct IntermediateType {
    vector<IntermediateTypeEntry> entries;
    set<string> isAmbiguous;
    map<string, string> tableAliases;
    IntermediateType() : entries(), isAmbiguous(), tableAliases() {}
    IntermediateType(const Schema& schema, string tableName = "");
    int compare(const ValueArray& a, const ValueArray& b) const;
    void addEntry(const IntermediateTypeEntry& entry);
    friend ostream& operator<<(ostream& os, const IntermediateType& type);
};