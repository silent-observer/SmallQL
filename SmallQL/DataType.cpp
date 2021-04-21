#include "DataType.h"
#include "Common.h"
#include <iostream>
#include <algorithm>
#include <assert.h>
#include <sstream>

Value::~Value() {
    if (type == ValueType::String)
        stringVal.~string();
}
Value::Value(const Value& v) {
    *this = v;
}
Value::Value(ValueType type) : type(type) {
    switch (type)
    {
    case ValueType::Null:
    case ValueType::MaxVal:
    case ValueType::MinVal:
        break;
    case ValueType::Integer:
        intVal = 0;
        break;
    case ValueType::Double:
        doubleVal = 0;
        break;
    case ValueType::String:
        new(&stringVal) string();
        break;
    default:
        break;
    }
}
Value::Value(int64_t intVal) : type(ValueType::Integer), intVal(intVal) {}
Value::Value(double doubleVal) : type(ValueType::Double), doubleVal(doubleVal) {}
Value::Value(string stringVal) : type(ValueType::String), stringVal(stringVal) {}
Value& Value::operator=(const Value& v) {
    if (this == &v) return *this;
    type = v.type;
    switch (type)
    {
    case ValueType::Null:
    case ValueType::MaxVal:
    case ValueType::MinVal:
        break;
    case ValueType::Integer:
        intVal = v.intVal;
        break;
    case ValueType::Double:
        doubleVal = v.doubleVal;
        break;
    case ValueType::String:
        new(&stringVal) string(v.stringVal);
        break;
    default:
        break;
    }
}
unique_ptr<DataType> Value::defaultType() const {
    switch (type)
    {
    case ValueType::Null:
    case ValueType::MaxVal:
    case ValueType::MinVal:
        return make_unique<NullType>();
    case ValueType::Integer:
        return make_unique<IntType>();
    case ValueType::Double:
        return make_unique<DoubleType>();
    case ValueType::String:
        return make_unique<VarCharType>(stringVal.size());
    default:
        break;
    }
}

void Value::convertToDouble() {
    switch (type)
    {
    case ValueType::Null:
    case ValueType::MaxVal:
    case ValueType::MinVal:
        return;
    case ValueType::String:
        doubleVal = NAN;
        break;
    case ValueType::Integer:
        doubleVal = (double)intVal;
        break;
    case ValueType::Double:
        break;
    }
    type = ValueType::Double;
}

ostream& operator<<(ostream& os, const Value& v) {
    switch (v.type)
    {
    case ValueType::Null:
        os << "NULL";
        break;
    case ValueType::MaxVal:
        os << "MAX";
        break;
    case ValueType::MinVal:
        os << "MIN";
        break;
    case ValueType::Integer:
        os << "Int{" << v.intVal << "}";
        break;
    case ValueType::Double:
        os << "Double{" << v.doubleVal << "}";
        break;
    case ValueType::String:
        os << "String{\"" << v.stringVal << "\"}";
        break;
    default:
        break;
    }
    return os;
}
string Value::toString() const {
    stringstream ss;
    ss << *this;
    return ss.str();
}
ostream& operator<<(ostream& os, const ValueArray& values) {
    os << "[";
    for (int i = 0; i < values.size(); i++) {
        os << " " << values[i];
    }
    os << " ]";
    return os;
}
ostream& operator<<(ostream& os, const DataType& t) {
    t.print(os);
    return os;
}

int compareValue(const Value& a, const Value& b) {
    if (a.type == ValueType::MinVal || b.type == ValueType::MinVal) {
        if (a.type == ValueType::MinVal && b.type == ValueType::MinVal)
            return 0;
        else if (a.type == ValueType::MinVal)
            return -1;
        else
            return 1;
    }
    if (a.type == ValueType::MaxVal || b.type == ValueType::MaxVal) {
        if (a.type == ValueType::MaxVal && b.type == ValueType::MaxVal)
            return 0;
        else if (a.type == ValueType::MaxVal)
            return 1;
        else
            return -1;
    }

    if (a.type == ValueType::Null || b.type == ValueType::Null) {
        if (a.type == ValueType::Null && b.type == ValueType::Null)
            return 0;
        else if (a.type == ValueType::Null)
            return -1;
        else
            return 1;
    }
    assert(a.type == b.type);
    switch (a.type)
    {
    case ValueType::Integer: 
        return cmp(a.intVal, b.intVal);
    case ValueType::Double: 
        return cmp(a.doubleVal, b.doubleVal);
    case ValueType::String: 
        return a.stringVal.compare(b.stringVal);
    }
}

bool NullType::checkVal(Value val) const {
    return val.type == ValueType::Null;
}
void NullType::encode(Value val, char* out) const {}
Value NullType::decode(const char* data) const {
    return Value(ValueType::Null);
}
void NullType::print(ostream& os) const {
    os << "NULL";
}

bool ByteType::checkVal(Value val) const {
    return val.type == ValueType::Integer && val.intVal >= INT8_MIN && val.intVal <= INT8_MAX;
}
void ByteType::encode(Value val, char* out) const {
    assert(checkVal(val));
    *(int8_t*)out = val.intVal;
}
Value ByteType::decode(const char* data) const {
    Value result(ValueType::Integer);
    result.intVal = *(int8_t*)data;
    return result;
}
void ByteType::print(ostream& os) const {
    os << "BYTE";
}

bool ShortIntType::checkVal(Value val) const {
    return val.type == ValueType::Integer && val.intVal >= INT16_MIN && val.intVal <= INT16_MAX;
}
void ShortIntType::encode(Value val, char* out) const {
    assert(checkVal(val));
    *(int16_t*)out = val.intVal;
}
Value ShortIntType::decode(const char* data) const {
    Value result(ValueType::Integer);
    result.intVal = *(int16_t*)data;
    return result;
}
void ShortIntType::print(ostream& os) const {
    os << "SMALLINT";
}

bool IntType::checkVal(Value val) const {
    return val.type == ValueType::Integer && val.intVal >= INT32_MIN && val.intVal <= INT32_MAX;
}
void IntType::encode(Value val, char* out) const {
    assert(checkVal(val));
    *(int32_t*)out = val.intVal;
}
Value IntType::decode(const char* data) const {
    Value result(ValueType::Integer);
    result.intVal = *(int32_t*)data;
    return result;
}
void IntType::print(ostream& os) const {
    os << "INTEGER";
}

bool DoubleType::checkVal(Value val) const {
    return val.type == ValueType::Double || val.type == ValueType::Integer;
}
void DoubleType::encode(Value val, char* out) const {
    assert(checkVal(val));
    if (val.type == ValueType::Double)
        *(double*)out = val.doubleVal;
    else
        *(double*)out = (double)val.intVal;
}
Value DoubleType::decode(const char* data) const {
    Value result(ValueType::Double);
    result.doubleVal = *(double*)data;
    return result;
}
void DoubleType::print(ostream& os) const {
    os << "DOUBLE";
}

bool VarCharType::checkVal(Value val) const {
    return val.type == ValueType::String && val.stringVal.size() <= maxSize;
}
void VarCharType::encode(Value val, char* out) const {
    assert(checkVal(val));
    *(uint16_t*)out = val.stringVal.size();
    memcpy(out + 2, val.stringVal.c_str(), val.stringVal.size());
    memset(out + 2 + val.stringVal.size(), 0, maxSize - val.stringVal.size());
}
Value VarCharType::decode(const char* data) const {
    Value result(ValueType::String);
    uint16_t size = *(uint16_t*)data;
    assert(size <= maxSize);
    result.stringVal = string(data + 2, size);
    return result;
}
void VarCharType::print(ostream& os) const {
    os << "VARCHAR(" << maxSize << ")";
}

int Schema::compare(const char* a, ValueArray b) const {
    uint32_t offset = 0;
    for (int i = 0; i < columns.size(); i++) {
        Value v1 = columns[i].type->decode(a + offset);
        Value v2 = b[i];
        int x = compareValue(v1, v2);
        if (x != 0) return x;
        offset += columns[i].type->getSize();
    }
    return 0;
}

int Schema::compare(const char* a, const char* b) const {
    uint32_t offset = 0;
    for (int i = 0; i < columns.size(); i++) {
        Value v1 = columns[i].type->decode(a + offset);
        Value v2 = columns[i].type->decode(b + offset);
        int x = compareValue(v1, v2);
        if (x != 0) return x;
        offset += columns[i].type->getSize();
    }
    return 0;
}


Schema::Schema(vector<SchemaEntry> columns)
    : columns(columns)
    , totalNullBits(0) {
    for (int i = 0; i < this->columns.size(); i++) {
        this->columns[i].id = i;
        if (this->columns[i].canBeNull)
            this->columns[i].nullBit = totalNullBits++;
    }
    totalNullBytes = (totalNullBits + 7) / 8;

    uint32_t offset = totalNullBytes;
    for (int i = 0; i < this->columns.size(); i++) {
        this->columns[i].offset = offset;
        offset += this->columns[i].type->getSize();
    }
    size = offset;
}

bool Schema::checkVal(ValueArray values) const {
    for (int i = 0; i < columns.size(); i++) {
        if (values[i].type == ValueType::Null) {
            if (!columns[i].canBeNull) return false;
        }
        else if (!columns[i].type->checkVal(values[i])) {
            return false;
        }
    }
    return true;
}
void Schema::encode(ValueArray values, char* out) const {
    assert(checkVal(values));
    memset(out, 0, totalNullBytes);
    for (int i = 0; i < columns.size(); i++) {
        if(FixedLengthType* t = convert<FixedLengthType>(columns[i].type)) {
            if (values[i].type == ValueType::Null) {
                int nullIndex = columns[i].nullBit;
                out[nullIndex / 8] |= 1 << (nullIndex % 8);
            }
            else {
                t->encode(values[i], out + columns[i].offset);
            }
        }
    }
}
ValueArray Schema::decode(const char* data) const {
    ValueArray result(columns.size());
    for (int i = 0; i < columns.size(); i++) {
        if(FixedLengthType* t = convert<FixedLengthType>(columns[i].type)) {
            bool isNull = false;
            if (columns[i].canBeNull) {
                int nullIndex = columns[i].nullBit;
                isNull = (data[nullIndex / 8] >> (nullIndex % 8)) & 1;
            }
            if (isNull)
                result[i] = Value(ValueType::Null);
            else {
                Value v = t->decode(data + columns[i].offset);
                result[i] = v;
            }
        }
    }
    return result;
}
void Schema::addColumn(SchemaEntry entry) {
    entry.id = columns.size();
    if (entry.canBeNull) {
        entry.nullBit = totalNullBits++;
        totalNullBytes = (totalNullBits + 7) / 8;
    }
    entry.offset = size;
    size += entry.type->getSize();
    columns.push_back(entry);
}

Schema Schema::primaryKeySubschema() const {
    vector<SchemaEntry> keys;
    for (const auto& column : columns) {
        if (column.isPrimary)
            keys.push_back(column);
    }
    return Schema(keys);
}
ValueArray Schema::narrow(const ValueArray& values) const {
    ValueArray result;
    for (const auto& column : columns) {
        result.push_back(values[column.id]);
    }
    return result;
}

vector<int> Schema::getNullBitOffsets() const {
    uint32_t nullIndex = 0;
    vector<int> result(totalNullBits);
    for (int i = 0; i < columns.size(); i++) {
        if(auto t = convert<FixedLengthType>(columns[i].type)) {
            if (columns[i].canBeNull) {
                result[nullIndex] = i;
                nullIndex++;
            }
        }
    }
    return result;
}

ostream& operator<<(ostream& os, const Schema& schema) {
    os << "[";
    for (int i = 0; i < schema.columns.size(); i++) {
        if (i != 0) os << ", ";
        os << schema.columns[i].name << " " << *schema.columns[i].type;
    }
    os << " ]";
    return os;
}

shared_ptr<DataType> parseDataType(string str) {    string s = str;
    for (auto& c: s) c = toupper(c);
    
    int lparenPos = s.find('(');
    string mainName = s;
    string params = "";
    if (lparenPos != s.npos) {
        mainName = s.substr(0, lparenPos);
        int rparenPos = s.find(')');
        params = s.substr(lparenPos + 1, rparenPos - lparenPos - 1);
    }
    if (mainName == "INTEGER" || 
        mainName == "INT" || 
        mainName == "NUMBER") 
        return make_shared<IntType>();
    else if (mainName == "SMALLINT") return make_shared<ShortIntType>();
    else if (mainName == "TINYINT" || mainName == "BYTE") return make_shared<ByteType>();
    else if (mainName == "DOUBLE") return make_shared<DoubleType>();
    else if (mainName == "VARCHAR") {
        if (params == "") return NULL;
        int val = stoi(params);
        return make_shared<VarCharType>(val);
    }
    else {
        return NULL;
    }
}

shared_ptr<DataType> typeCheckFunc(string name, vector<shared_ptr<DataType>> inputs) {
    if (name == "+" || name == "-" || name == "*" || name == "/") {
        bool hasDouble = false;
        for (auto& t : inputs)
            if (is<DoubleType>(t))
                hasDouble = true;
            else if (!is<IntType>(t) && !is<ShortIntType>(t) && !is<ByteType>(t))
                throw TypeException("Invalid type in " + name + " expression");
        if (hasDouble)
            return make_shared<DoubleType>();
        else
            return make_shared<IntType>();
    }
    else if (name == "CONCAT") {
        for (auto& t : inputs)
            if (!is<VarCharType>(t))
                throw TypeException("Invalid type in CONCAT expression");
        return make_shared<IntType>();
    }
    return nullptr;
}

IntermediateType::IntermediateType(const Schema& schema, string tableName) {
    for (int i = 0; i < schema.columns.size(); i++) {
        IntermediateTypeEntry entry;
        entry.columnName = schema.columns[i].name;
        entry.tableName = tableName;
        entry.type = schema.columns[i].type;
        entry.canBeNull = schema.columns[i].canBeNull;
        entry.id = schema.columns[i].id;
        entries.push_back(entry);
        isAmbiguous.push_back(false);
    }
}

int Schema::compare(const ValueArray& a, const ValueArray& b) const {
    for (int i = 0; i < a.size(); i++) {
        int x = compareValue(a[i], b[i]);
        if (x != 0) return x;
    }
    return 0;
}

int IntermediateType::compare(const ValueArray& a, const ValueArray& b) const {
    for (int i = 0; i < a.size(); i++) {
        int x = compareValue(a[i], b[i]);
        if (x != 0) return x;
    }
    return 0;
}

void IntermediateType::addEntry(const IntermediateTypeEntry& entry) {
    bool ambiguous = false;
    for (int i = 0; i < entries.size(); i++)
        if (entry.columnName == entries[i].columnName) {
            ambiguous = true;
            break;
        }
    entries.push_back(entry);
    entries.back().id = entries.size() - 1;
    isAmbiguous.push_back(ambiguous);
}

ostream& operator<<(ostream& os, const IntermediateType& type) {
    os << "[";
    for (int i = 0; i < type.entries.size(); i++) {
        if (i != 0) os << ", ";
        if (type.isAmbiguous[i])
            os << type.entries[i].tableName << ".";
        os << type.entries[i].columnName << " " << *type.entries[i].type;
    }
    os << " ]";
    return os;
}