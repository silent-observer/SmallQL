#include "SystemInfoManager.h"

#include "DataFile.h"
#include <iostream>
#include <set>

const Schema tablesSchema(
    vector<SchemaEntry> {
        schemaEntryPrimary("table_id", make_shared<ShortIntType>()),
        schemaEntryNormal("table_name", make_shared<VarCharType>(40), false)
    }
);
const Schema columnsSchema(
    vector<SchemaEntry> {
        schemaEntryPrimary("table_id", make_shared<ShortIntType>()),
        schemaEntryPrimary("column_id", make_shared<ShortIntType>()),
        schemaEntryNormal("column_name", make_shared<VarCharType>(40), false),
        schemaEntryNormal("column_type", make_shared<VarCharType>(20), false),
        schemaEntryNormal("flags", make_shared<ByteType>(), false),
        schemaEntryNormal("default_value", make_shared<VarCharType>(40), false)
    }
);
const Schema indexesSchema(
    vector<SchemaEntry> {
        schemaEntryPrimary("table_id", make_shared<ShortIntType>()),
        schemaEntryPrimary("index_id", make_shared<ShortIntType>()),
        schemaEntryNormal("flags", make_shared<ByteType>(), false),
        schemaEntryNormal("index_name", make_shared<VarCharType>(40), false)
    }
);
const Schema indexColumnsSchema(
    vector<SchemaEntry> {
        schemaEntryPrimary("table_id", make_shared<ShortIntType>()),
        schemaEntryPrimary("index_id", make_shared<ShortIntType>()),
        schemaEntryPrimary("column_id", make_shared<ShortIntType>())
    }
);

void SystemInfoManager::load() {
    DataFile tablesFile(-1, tablesSchema.getSize());
    DataFile columnsFile(-2, columnsSchema.getSize());
    DataFile indexesFile(-3, indexesSchema.getSize());
    DataFile indexColumnsFile(-4, indexColumnsSchema.getSize());

    struct ColumnData {
        string columnName;
        string type;
        uint8_t flags;
        string default_value;
    };
    map<uint16_t, map<uint16_t, ColumnData>> columnData;
    for (const char* record : makeConst(tablesFile)) {
        ValueArray decoded = tablesSchema.decode(record);
        uint16_t tableId = decoded[0].intVal;
        string tableName = decoded[1].stringVal;
        tableNames[tableName] = tableId;
        columnData[tableId] = map<uint16_t, ColumnData>();
    }

    for (const char* record : makeConst(columnsFile)) {
        ValueArray decoded = columnsSchema.decode(record);
        uint16_t tableId = decoded[0].intVal;
        uint16_t columnId = decoded[1].intVal;
        string columnName = decoded[2].stringVal;
        string columnType = decoded[3].stringVal;
        uint8_t columnFlags = decoded[4].intVal;
        string columnDefaultVal = decoded[5].stringVal;
        columnData[tableId][columnId] = ColumnData{
            columnName, columnType, columnFlags, columnDefaultVal
        };
    }

    for (const auto& p : tableNames) {
        vector<SchemaEntry> entries;
        map<uint16_t, ColumnData>& columnMap = columnData[p.second];
        for (int i = 0; i < columnMap.size(); i++) {
            if (columnMap.count(i) == 0) {
                throw InternalTablesException("ERROR: in table " + p.first +
                    "(" + to_string(p.second) + ") there is no column " + to_string(i));
            }
            ColumnData data = columnMap[i];
            bool isPrimary = data.flags & 0x1;
            bool canBeNull = data.flags & 0x2;
            shared_ptr<DataType> type = parseDataType(data.type);
            if (type == NULL) {
                throw InternalTablesException("ERROR: in table " + p.first +
                    "(" + to_string(p.second) + ") column " + to_string(i) + 
                    "has unknown type \"" + data.type + "\"");
            }
            if (isPrimary) {
                entries.push_back(schemaEntryPrimary(data.columnName, type));
            }
            else {
                entries.push_back(schemaEntryNormal(data.columnName, type, canBeNull));
            }
        }
        tables[p.second] = TableInfo{p.second, p.first, Schema(entries)};
    }

    for (const char* record : makeConst(indexesFile)) {
        ValueArray decoded = indexesSchema.decode(record);
        uint16_t tableId = decoded[0].intVal;
        uint16_t indexId = decoded[1].intVal;
        uint8_t flags = decoded[2].intVal;
        string indexName = decoded[3].stringVal;
        indexNames[make_pair(tableId, indexName)] = indexId;
        indexes[make_pair(tableId, indexId)] = IndexInfo{indexId, indexName, Schema()};
        tables[tableId].indexes.push_back(indexId);
    }

    for (const char* record : makeConst(indexColumnsFile)) {
        ValueArray decoded = indexColumnsSchema.decode(record);
        uint16_t tableId = decoded[0].intVal;
        uint16_t indexId = decoded[1].intVal;
        uint16_t columnId = decoded[2].intVal;
        SchemaEntry& entry = tables[tableId].schema.columns[columnId];
        indexes[make_pair(tableId, indexId)].schema.addColumn(entry);
    }
}

uint16_t SystemInfoManager::addTable(string name) {
    makeUpper(name);
    if (tableNames.count(name) != 0)
        throw InternalTablesException("Table " + name + " already exists!");
    DataFile tablesFile(-1, tablesSchema.getSize());
    uint16_t tableId = 0;
    while (tables.count(tableId) != 0) tableId++;

    auto encoded = tablesSchema.encode({
        Value(tableId), Value(name)
    });
    tablesFile.addRecord(encoded);
    tables[tableId] = { tableId, name, Schema() };
    tableNames[name] = tableId;

    return tableId;
}

uint16_t SystemInfoManager::addColumn(uint16_t tableId, string name, 
    string type, bool isPrimary, bool canBeNull, string defaultValue) {
    makeUpper(name);

    DataFile columnsFile(-2, columnsSchema.getSize());
    uint16_t columnId = tables[tableId].schema.columns.size();
    if (isPrimary) canBeNull = false;
    uint8_t flags = (int)isPrimary | ((int)canBeNull << 1);

    auto encoded = columnsSchema.encode({
        Value(tableId), Value(columnId), 
        Value(name), Value(type), 
        Value(flags), Value(defaultValue)
    });
    columnsFile.addRecord(encoded);
    tables[tableId].schema.addColumn(SchemaEntry{
        parseDataType(type), name,
        0, 0, isPrimary, canBeNull
    });

    return columnId;
}

void SystemInfoManager::createPrimaryIndex(uint16_t tableId) {
    DataFile indexesFile(-3, indexesSchema.getSize());
    DataFile indexColumnsFile(-4, indexColumnsSchema.getSize());

    auto encoded = indexesSchema.encode({
        Value(tableId), Value(0), Value((int8_t)0x80), Value("PRIMARY")
    });
    indexesFile.addRecord(encoded);
    
    Schema keySchema = tables[tableId].schema.primaryKeySubschema();
    tables[tableId].primaryKeys = keySchema;
    for (const auto& entry : keySchema.columns) {
        auto encoded = indexColumnsSchema.encode({
            Value(tableId), Value(0), Value(entry.id)
        });
        indexColumnsFile.addRecord(encoded);
    }
    indexes[make_pair(tableId, 0)] = IndexInfo{ 0, "PRIMARY", keySchema };
    indexNames[make_pair(tableId, "PRIMARY")] = 0;
    tables[tableId].indexes.push_back(0);
}