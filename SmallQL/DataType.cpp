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
    case ValueType::Datetime:
        datetimeVal = Datetime();
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
Value::Value(Datetime datetimeVal) : type(ValueType::Datetime), datetimeVal(datetimeVal) {}
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
    case ValueType::Datetime:
        datetimeVal = v.datetimeVal;
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
    case ValueType::Datetime:
        return make_unique<DatetimeType>();
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
    case ValueType::Datetime:
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
    case ValueType::Datetime:
        os << "Datetime{\"" << v.datetimeVal << "\"}";
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
string DataType::toString() const {
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
    if (a.type != b.type) {
        if (a.type == ValueType::Integer && b.type == ValueType::Double)
            return cmp((double)a.intVal, b.doubleVal);
        else if (a.type == ValueType::Double && b.type == ValueType::Integer)
            return cmp(a.doubleVal, (double)b.intVal);
        else
            assert(false);
    }
    switch (a.type)
    {
    case ValueType::Integer: 
        return cmp(a.intVal, b.intVal);
    case ValueType::Double: 
        return cmp(a.doubleVal, b.doubleVal);
    case ValueType::String: 
        return a.stringVal.compare(b.stringVal);
    case ValueType::Datetime: 
        return a.datetimeVal.compare(b.datetimeVal);
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

bool DatetimeType::checkVal(Value val) const {
    return val.type == ValueType::Datetime;
}
void DatetimeType::encode(Value val, char* out) const {
    assert(checkVal(val));
    *(uint16_t*)out = val.datetimeVal.year;
    *(uint8_t*)(out + 2) = val.datetimeVal.month;
    *(uint8_t*)(out + 3) = val.datetimeVal.day;
    *(uint8_t*)(out + 4) = val.datetimeVal.hour;
    *(uint8_t*)(out + 5) = val.datetimeVal.minute;
    *(uint8_t*)(out + 6) = val.datetimeVal.second;
    *(uint8_t*)(out + 7) = 0;
}
Value DatetimeType::decode(const char* data) const {
    Value result(ValueType::Datetime);
    result.datetimeVal.year = *(uint16_t*)data;
    result.datetimeVal.month = *(uint8_t*)(data + 2);
    result.datetimeVal.day = *(uint8_t*)(data + 3);
    result.datetimeVal.hour = *(uint8_t*)(data + 4);
    result.datetimeVal.minute = *(uint8_t*)(data + 5);
    result.datetimeVal.second = *(uint8_t*)(data + 6);
    return result;
}
void DatetimeType::print(ostream& os) const {
    os << "DATETIME";
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

bool TextType::checkVal(Value val) const {
    return val.type == ValueType::String;
}
vector<char> TextType::encodePtr(Value val) const {
    assert(checkVal(val));
    vector<char> result(val.stringVal.size() + 4);
    *(uint32_t*)result.data() = val.stringVal.size();
    memcpy(result.data() + 4, val.stringVal.c_str(), val.stringVal.size());
    return result;
}
pair<uint32_t, Value> TextType::decodePtr(const char* data) const {
    Value result(ValueType::String);
    uint32_t size = *(uint32_t*)data;
    result.stringVal = string(data + 4, size);
    return make_pair(size + 4, result);
}
void TextType::print(ostream& os) const {
    os << "TEXT";
}

int Schema::compare(const char* a, ValueArray b) const {
    for (int i = 0; i < columns.size(); i++) {
        Value v1 = columns[i].type->decode(a + columns[i].offset);
        Value v2 = b[i];
        int x = compareValue(v1, v2);
        if (x != 0) return x;
    }
    return 0;
}

int Schema::compare(const char* a, const char* b) const {
    for (int i = 0; i < columns.size(); i++) {
        Value v1 = columns[i].type->decode(a + columns[i].offset);
        Value v2 = columns[i].type->decode(b + columns[i].offset);
        int x = compareValue(v1, v2);
        if (x != 0) return x;
    }
    return 0;
}


Schema::Schema(vector<SchemaEntry> columns)
    : columns(columns)
    , totalNullBits(0) {
    updateData(false);
}

void Schema::updateData(bool preserveIds) {
    totalNullBits = 0;
    hasVarLenData = false;

    for (int i = 0; i < this->columns.size(); i++) {
        if (!preserveIds)
            this->columns[i].id = i;
        if (this->columns[i].canBeNull)
            this->columns[i].nullBit = totalNullBits++;
        if (is<VariableLengthType>(this->columns[i].type)) {
            hasVarLenData = true;
            if (this->columns[i].isPrimary)
                throw TypeException("Cannot have primary keys of variable size!");
        }
    }
    totalNullBytes = (totalNullBits + 7) / 8;

    uint32_t offset = totalNullBytes;
    for (int i = 0; i < this->columns.size(); i++) {
        if (is<VariableLengthType>(this->columns[i].type)) continue;
        this->columns[i].offset = offset;
        offset += this->columns[i].type->getSize();
    }
    size = offset;
    if (hasVarLenData) {
        varLenOffset = offset;
        size += 4;
    }
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
vector<char> Schema::encode(ValueArray values, char* out) const {
    vector<char> varData;
    assert(checkVal(values));
    memset(out, 0, totalNullBytes);
    for (int i = 0; i < columns.size(); i++) {
        if (auto t = convert<FixedLengthType>(columns[i].type)) {
            if (values[i].type == ValueType::Null) {
                int nullIndex = columns[i].nullBit;
                out[nullIndex / 8] |= 1 << (nullIndex % 8);
            }
            else {
                t->encode(values[i], out + columns[i].offset);
            }
        }
        else if (auto t = convert<VariableLengthType>(columns[i].type)){
            if (values[i].type == ValueType::Null) {
                int nullIndex = columns[i].nullBit;
                out[nullIndex / 8] |= 1 << (nullIndex % 8);
            }
            else {
                auto vec = t->encodePtr(values[i]);
                varData.insert(varData.end(), vec.begin(), vec.end());
            }
        }
    }
    return varData;
}
ValueArray Schema::decode(const char* data, const char* varData) const {
    ValueArray result(columns.size());
    uint32_t varDataOffset = 0;
    for (int i = 0; i < columns.size(); i++) {
        if (auto t = convert<FixedLengthType>(columns[i].type)) {
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
        else if (auto t = convert<VariableLengthType>(columns[i].type)) {
            bool isNull = false;
            if (columns[i].canBeNull) {
                int nullIndex = columns[i].nullBit;
                isNull = (data[nullIndex / 8] >> (nullIndex % 8)) & 1;
            }
            if (isNull || varData == nullptr)
                result[i] = Value(ValueType::Null);
            else {
                auto p = t->decodePtr(varData + varDataOffset);
                varDataOffset += p.first;
                result[i] = p.second;
            }
        }
    }
    return result;
}
uint32_t Schema::decodeBlobId(const char* data) const {
    auto ptr = data + varLenOffset;
    return *(uint32_t*)ptr;
}
void Schema::setBlobId(const char* data, uint32_t blobId) const {
    auto ptr = data + varLenOffset;
    *(uint32_t*)ptr = blobId;
}


void Schema::addColumn(SchemaEntry entry) {
    columns.push_back(entry);
    if (is<VariableLengthType>(entry.type))
        hasVarLenData = true;
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
        if (columns[i].canBeNull) {
            result[nullIndex] = i;
            nullIndex++;
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
    else if (mainName == "DATETIME") return make_shared<DatetimeType>();
    else if (mainName == "VARCHAR") {
        if (params == "") return NULL;
        int val = stoi(params);
        return make_shared<VarCharType>(val);
    }
    else if (mainName == "TEXT") return make_shared<TextType>();
    else {
        return NULL;
    }
}

bool typeCheckComparable(shared_ptr<DataType> a, shared_ptr<DataType> b) {
    if (is<NullType>(a) || is<NullType>(b)) 
        return true;
    if (is<VarCharType>(a) || is<TextType>(a))
        return is<VarCharType>(b) || is<TextType>(b);
    if (is<ByteType>(a) ||is<ShortIntType>(a) || is<IntType>(a) || is<DoubleType>(a))
        return is<ByteType>(b) ||is<ShortIntType>(b) || is<IntType>(b) || is<DoubleType>(b);
    if (is<DatetimeType>(a))
        return is<DatetimeType>(b);
    return false;
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
            if (!is<VarCharType>(t) && !is<TextType>(t))
                throw TypeException("Invalid type in CONCAT expression");
        return inputs[0];
    }
    else if (name == "TO_DATETIME") {
        if (inputs.size() == 0 || inputs.size() > 2)
            throw TypeException("TO_DATETIME supports only 1 or 2 arguments");
        for (auto& t : inputs)
            if (!is<VarCharType>(t) && !is<TextType>(t))
                throw TypeException("Invalid type in TO_DATETIME expression");
        return make_shared<DatetimeType>();
    }
    else if (name == "UCASE" || name == "LCASE") {
        if (inputs.size() != 1)
            throw TypeException(name + " uses only 1 argument");
        if (!is<VarCharType>(inputs[0]) && !is<TextType>(inputs[0]))
            throw TypeException(name + " uses 1 string argument");
        return inputs[0];
    }
    else if (name == "LENGTH") {
        if (inputs.size() != 1)
            throw TypeException(name + " uses only 1 argument");
        if (!is<VarCharType>(inputs[0]) && !is<TextType>(inputs[0]))
            throw TypeException(name + " uses 1 string argument");
        return make_shared<IntType>();
    }
    else if (name == "SUBSTR") {
        if (inputs.size() != 2 && inputs.size() != 3)
            throw TypeException(name + " supports only 2 or 3 arguments");
        if (!is<VarCharType>(inputs[0]) && !is<TextType>(inputs[0]))
            throw TypeException("First argument of SUBSTR must be a string");
        if (!is<IntType>(inputs[1]) && !is<ShortIntType>(inputs[1]) && !is<ByteType>(inputs[1]))
            throw TypeException("Second argument of SUBSTR must be an integer");
        if (inputs.size() == 3 && !is<IntType>(inputs[2]) && !is<ShortIntType>(inputs[2]) && !is<ByteType>(inputs[2]))
            throw TypeException("Third argument of SUBSTR must be an integer");
        return inputs[0];
    }
    else if (name == "ROUND" || name == "CEIL" || name == "FLOOR") {
        if (inputs.size() != 1)
            throw TypeException(name + " uses only 1 argument");
        if (!is<DoubleType>(inputs[0]))
            throw TypeException("The argument of " + name + " must be a double");
        return make_shared<IntType>();
    }
    else if (name == "ABS") {
        if (inputs.size() != 1)
            throw TypeException(name + " uses only 1 argument");
        if (!is<DoubleType>(inputs[0]) && !is<IntType>(inputs[0]) && !is<ShortIntType>(inputs[0]) && !is<ByteType>(inputs[0]))
            throw TypeException("The argument of " + name + " must be a number");
        return inputs[0];
    }
    else if (name == "SQRT") {
        if (inputs.size() != 1)
            throw TypeException(name + " uses only 1 argument");
        if (!is<DoubleType>(inputs[0]) && !is<IntType>(inputs[0]) && !is<ShortIntType>(inputs[0]) && !is<ByteType>(inputs[0]))
            throw TypeException("The argument of " + name + " must be a number");
        return make_shared<DoubleType>();
    }
    else if (name == "POW") {
        if (inputs.size() != 2)
            throw TypeException(name + " uses 2 arguments");
        for (auto& t : inputs)
            if (!is<DoubleType>(t) && !is<IntType>(t) && !is<ShortIntType>(t) && !is<ByteType>(t))
                throw TypeException("The arguments of " + name + " must be numbers");
        return make_shared<DoubleType>();
    }
    else if (name == "RAND") {
        if (inputs.size() != 0)
            throw TypeException(name + " takes no arguments");
        return make_shared<DoubleType>();
    }
    else if (name == "NOW") {
        if (inputs.size() != 0)
            throw TypeException(name + " takes no arguments");
        return make_shared<DatetimeType>();
    }
    else if (name == "YEAR" || name == "MONTH" || name == "DAY" || name == "HOUR" || name == "MINUTE" || name == "SECOND") {
        if (inputs.size() != 1)
            throw TypeException(name + " takes 1 argument");
        if (!is<DatetimeType>(inputs[0]))
            throw TypeException("The argument of " + name + " must be a datetime");
        return make_shared<IntType>();
    }
    return nullptr;
}

shared_ptr<DataType> typeCheckAggrFunc(string name, shared_ptr<DataType> input) {
    if (name == "SUM" || name == "AVG") {
        if (is<DoubleType>(input))
            return make_shared<DoubleType>();
        else if (is<IntType>(input) || is<ShortIntType>(input) || is<ByteType>(input))
            return make_shared<IntType>();
        else
            throw TypeException("Invalid type in " + name + " expression");
    }
    else if (name == "MIN" || name == "MAX") {
        return input;
    }
    else if (name == "COUNT") {
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
        entry.isAggregate = false;
        entry.isConst = false;
        entries.push_back(entry);
    }
}

int Schema::compare(const ValueArray& a, const ValueArray& b) const {
    for (int i = 0; i < a.size(); i++) {
        int x = compareValue(a[i], b[i]);
        if (x != 0) return x;
    }
    return 0;
}

int IntermediateTypeEntry::compare(const Value& a, const Value& b) const {
    return compareValue(a, b);
}

int IntermediateType::compare(const ValueArray& a, const ValueArray& b) const {
    for (int i = 0; i < a.size(); i++) {
        int x = entries[i].compare(a[i], b[i]);
        if (x != 0) return x;
    }
    return 0;
}

void IntermediateType::addEntry(const IntermediateTypeEntry& entry) {
    for (int i = 0; i < entries.size(); i++)
        if (entry.columnName == entries[i].columnName) {
            isAmbiguous.insert(entry.columnName);
            break;
        }
    entries.push_back(entry);
    entries.back().id = entries.size() - 1;
}

ostream& operator<<(ostream& os, const IntermediateType& type) {
    os << "[";
    for (int i = 0; i < type.entries.size(); i++) {
        if (i != 0) os << ", ";
        if (type.isAmbiguous.count(type.entries[i].columnName) != 0)
            os << type.entries[i].tableName << ".";
        os << type.entries[i].columnName << " " << *type.entries[i].type;
    }
    os << " ]";
    return os;
}