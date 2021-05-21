#pragma once

#include <cstdint>
#include <vector>
#include <set>
#include <map>
#include <string>
#include <memory>
#include "Common.h"
#include "Datetime.h"

using namespace std;

enum class ValueType {
    Null,
    Integer,
    Double,
    String,
    Datetime,
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
        Datetime datetimeVal;
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
    Value(Datetime datetimeVal);
    friend ostream& operator<<(ostream& os, const Value& v);
    friend int compareValue(const Value& a, const Value& b);
    unique_ptr<DataType> defaultType() const;
    string toString() const;

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
    string toString() const;
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

class VariableLengthType : public DataType {
protected:
    VariableLengthType() : DataType(0) {}
public:
    virtual vector<char> encodePtr(Value val) const = 0;
    virtual pair<uint32_t, Value> decodePtr(const char* data) const = 0;
    Value decode(const char* data) const { return decodePtr(data).second; }
    void encode(Value val, char* out) const { 
        auto data = encodePtr(val);
        memcpy(out, data.data(), data.size());
    };
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
    bool hasVarLenData;
    int varLenOffset;
    Schema() : columns(), totalNullBits(0), totalNullBytes(0), size(0), hasVarLenData(false), varLenOffset(0) {}
    Schema(vector<SchemaEntry> columns);
    void updateData(bool preserveIds);
    inline uint32_t getSize() const {
        return size;
    };
    bool checkVal(ValueArray values) const;
    vector<char> encode(ValueArray values, char* out) const;
    inline pair<vector<char>, vector<char>> encode(ValueArray values) const {
        vector<char> result(getSize());
        auto varData = encode(values, result.data());
        return make_pair(result, varData);
    }
    ValueArray decode(const char* data, const char* varData) const;
    inline ValueArray decode(const vector<char>& data, const vector<char>& varData) const {
        return decode(data.data(), varData.data());
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
    uint32_t decodeBlobId(const char* data) const;
    void setBlobId(const char* data, uint32_t blobId) const;
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

class DatetimeType : public FixedLengthType {
public:
    DatetimeType(): FixedLengthType(8) {}

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

class TextType : public VariableLengthType {
public:
    TextType() : VariableLengthType() {}

    bool checkVal(Value val) const;
    vector<char> encodePtr(Value val) const;
    pair<uint32_t, Value> decodePtr(const char* data) const;
    void print(ostream& os) const;
};

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
bool typeCheckComparable(shared_ptr<DataType> a, shared_ptr<DataType> b);
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