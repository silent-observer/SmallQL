#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <memory>

using namespace std;

enum class ValueType {
    Null,
    Integer,
    String,
    MinVal,
    MaxVal
};

struct Value {
    ValueType type;
    union {
        int64_t intVal;
        string stringVal;
    };

    Value() : type(ValueType::Null) {};
    Value(ValueType type);
    Value(const Value& v);
    ~Value();
    Value& operator=(const Value& other);

    Value(int64_t intVal);
    Value(string stringVal);
    friend ostream& operator<<(ostream& os, const Value& v);
    friend int compareValue(const Value& a, const Value& b);
};
using ValueArray = vector<Value>;

ostream& operator<<(ostream& os, const ValueArray& values);
int compareValue(const Value& a, const Value& b);

class DataType {
protected:
    uint32_t size;
    uint8_t priority;
    DataType(uint32_t size, uint8_t priority) : size(size), priority(priority) {}
public:
    inline uint32_t getSize() const { return size; };
    inline uint32_t getPriority() const { return priority; };
    virtual bool checkVal(Value val) const = 0;
    virtual void encode(Value val, char* out) const = 0;
    virtual Value decode(const char* data) const = 0;
    virtual void print(ostream& os) const = 0;
    friend ostream& operator<<(ostream& os, const DataType& t);
};

class FixedLengthType : public DataType {
protected:
    FixedLengthType(uint32_t size, uint8_t priority) : DataType(size, priority) {}
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
    ByteType(): FixedLengthType(1, 4) {}

    bool checkVal(Value val) const;
    void encode(Value val, char* out) const;
    Value decode(const char* data) const;
    void print(ostream& os) const;
};

class ShortIntType : public FixedLengthType {
public:
    ShortIntType(): FixedLengthType(2, 3) {}

    bool checkVal(Value val) const;
    void encode(Value val, char* out) const;
    Value decode(const char* data) const;
    void print(ostream& os) const;
};

class IntType : public FixedLengthType {
public:
    IntType(): FixedLengthType(4, 2) {}

    bool checkVal(Value val) const;
    void encode(Value val, char* out) const;
    Value decode(const char* data) const;
    void print(ostream& os) const;
};

class VarCharType : public FixedLengthType {
private:
    uint16_t maxSize;
public:
    VarCharType(uint16_t maxSize) : maxSize(maxSize), FixedLengthType(maxSize + 2, 10) {}

    bool checkVal(Value val) const;
    void encode(Value val, char* out) const;
    Value decode(const char* data) const;
    void print(ostream& os) const;
};

static inline int cmp(uint64_t x, uint64_t y) {
    return x > y ? 1 : x < y ? -1 : 0;
}

shared_ptr<DataType> parseDataType(string str);