#pragma once

#include "DataType.h"
#include "Common.h"
#include <map>

struct TableInfo {
    uint16_t id;
    string name;
    Schema schema;
    Schema primaryKeys;
    vector<uint16_t> indexes;
};
struct IndexInfo {
    uint16_t id;
    string name;
    Schema schema;
};

class SystemInfoManager {
private:
    map<uint16_t, TableInfo> tables;
    map<string, uint16_t> tableNames;
    map<pair<uint16_t, uint16_t>, IndexInfo> indexes;
    map<pair<uint16_t, string>, uint16_t> indexNames;
public:
    SystemInfoManager(): tables() {};
    void load();
    uint16_t addTable(string name);
    uint16_t addColumn(uint16_t tableId, string name, string type, bool isPrimary, bool canBeNull, string defaultValue="");
    void dropTable(string name);
    void createPrimaryIndex(uint16_t tableId);

    inline bool tableExists(string name) const {
        return tableNames.count(name) > 0;
    }
    inline uint16_t _getTableId(string name) const {
        return tableNames.at(name);
    }
    inline uint16_t getTableId(string name) const {
        makeUpper(name);
        return _getTableId(name);
    }
    inline const Schema& getTableSchema(uint16_t id) const {
        return tables.at(id).schema;
    }
    inline const Schema& _getTableSchema(string name) const {
        return getTableSchema(tableNames.at(name));
    }
    inline const Schema& getTableSchema(string name) const {
        makeUpper(name);
        return _getTableSchema(name);
    }
    inline const TableInfo& getTableInfo(uint16_t id) const {
        return tables.at(id);
    }
    inline uint16_t _getIndexId(string tableName, string indexName = "PRIMARY") const {
        uint16_t tableId = tableNames.at(tableName);
        return indexNames.at(make_pair(tableId, indexName));
    }
    inline uint16_t getIndexId(string tableName, string indexName = "PRIMARY") const {
        makeUpper(tableName);
        makeUpper(indexName);
        return _getIndexId(tableName, indexName);
    }
    inline const Schema& _getIndexSchema(string tableName, string indexName = "PRIMARY") const {
        uint16_t tableId = tableNames.at(tableName);
        uint16_t indexId = indexNames.at(make_pair(tableId, indexName));
        return indexes.at(make_pair(tableId, indexId)).schema;
    }
    inline const Schema& getIndexSchema(string tableName, string indexName = "PRIMARY") const {
        makeUpper(tableName);
        makeUpper(indexName);
        return _getIndexSchema(tableName, indexName);
    }
    inline const Schema& getIndexSchema(uint16_t tableId, uint16_t indexId) const {
        return indexes.at(make_pair(tableId, indexId)).schema;
    }
};

class InternalTablesException : public std::exception
{
private:
    string message;
public:
    InternalTablesException(string cause)
        : message("Internal Tables Exception: " + cause) {}
    const char* what() const throw ()
    {
        return message.c_str();
    }
};
