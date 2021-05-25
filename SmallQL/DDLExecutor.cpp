#include "DDLExecutor.h"
#include <iostream>
#include "DataFile.h"
#include "IndexFile.h"
#include "PrettyTablePrinter.h"

static void executeCreateTable(const CreateTableNode* n, SystemInfoManager& sysMan) {
    if (sysMan.tableExists(n->name)) {
        cout << "Table " << n->name << " already exists!" << endl;
        return;
    }
    
    uint16_t tableId = sysMan.addTable(n->name);
    for (auto& col : n->columns) {
        sysMan.addColumn(tableId, col->name, col->typeStr, col->isPrimary, col->canBeNull);
    }
    sysMan.createPrimaryIndex(tableId);
    cout << "Table " << n->name << " successfully created!" << endl;
}

static void executeDropTable(const DropTableNode* n, SystemInfoManager& sysMan) {
    if (!sysMan.tableExists(n->name)) {
        cout << "Table " << n->name << " doesn't exist!" << endl;
        return;
    }
    
    sysMan.dropTable(n->name);
    cout << "Table " << n->name << " successfully dropped!" << endl;
}

static void executeCreateIndex(const CreateIndexNode* n, TransactionManager& trMan, SystemInfoManager& sysMan) {
    if (!sysMan.tableExists(n->tableName)) {
        cout << "Table " << n->tableName << " doesn't exist!" << endl;
        return;
    }
    uint16_t tableId = sysMan.getTableId(n->tableName);
    if (sysMan.indexExists(tableId, n->name)) {
        cout << "Index " << n->name << " for table " << n->tableName << " already exists!" << endl;
        return;
    }
    sysMan.createIndex(tableId, n->name, n->columns, n->isUnique);
    IndexFile indexFile(trMan, sysMan, n->tableName, n->name);
    DataFile dataFile(trMan, sysMan, n->tableName);
    bool result = indexFile.fillFrom(dataFile, sysMan.getTableSchema(n->tableName));
    if (result)
        cout << "Index " << n->name << " successfully created!" << endl;
    else {
        cout << "Index " << n->name << " couldn't be created since there are repeating keys in the table " << n->tableName << "!" << endl;
        sysMan.dropIndex(tableId, n->name);
    }
}

static void executeDropIndex(const DropIndexNode* n, SystemInfoManager& sysMan) {
    if (!sysMan.tableExists(n->tableName)) {
        cout << "Table " << n->tableName << " doesn't exist!" << endl;
        return;
    }
    uint16_t tableId = sysMan.getTableId(n->tableName);
    if (!sysMan.indexExists(tableId, n->name)) {
        cout << "Index " << n->name << " on table " << n->tableName << " doesn't exist!" << endl;
        return;
    }
    
    sysMan.dropIndex(tableId, n->name);
    cout << "Index " << n->name << " successfully dropped!" << endl;
}

static void executeShow(const ShowNode* n, SystemInfoManager& sysMan) {
    if (n->what == "TABLES") {
        IntermediateType type;
        type.addEntry(IntermediateTypeEntry("Table name", "", make_shared<TextType>(), false, false, false));
        vector<ValueArray> data;
        for (auto& table : sysMan.tables) {
            data.push_back({Value(table.second.name)});
        }
        cout << prettyPrintTable(data, type);
    } else if (n->what == "COLUMNS") {
        if (!sysMan.tableExists(n->fromWhere)) {
            cout << "Table " << n->fromWhere << " doesn't exist!" << endl;
            return;
        }

        IntermediateType type;
        type.addEntry(IntermediateTypeEntry("Column", "", make_shared<TextType>(), false, false, false));
        type.addEntry(IntermediateTypeEntry("Type", "", make_shared<TextType>(), false, false, false));
        type.addEntry(IntermediateTypeEntry("Null?", "", make_shared<TextType>(), false, false, false));
        type.addEntry(IntermediateTypeEntry("Is primary?", "", make_shared<TextType>(), false, false, false));

        vector<ValueArray> data;
        for (auto& column : sysMan.tables.at(sysMan.getTableId(n->fromWhere)).schema.columns) {
            string name = column.name;
            string type = column.type->toString();
            string null = column.canBeNull ? "YES" : "NO";
            string primary = column.isPrimary ? "YES" : "NO";
            data.push_back({Value(name), Value(type), Value(null), Value(primary)});
        }
        cout << prettyPrintTable(data, type);
    } else if (n->what == "INDEXES") {
        if (!sysMan.tableExists(n->fromWhere)) {
            cout << "Table " << n->fromWhere << " doesn't exist!" << endl;
            return;
        }

        IntermediateType type;
        type.addEntry(IntermediateTypeEntry("Index name", "", make_shared<TextType>(), false, false, false));
        type.addEntry(IntermediateTypeEntry("Column", "", make_shared<TextType>(), false, false, false));

        vector<ValueArray> data;
        uint16_t tableId = sysMan.getTableId(n->fromWhere);
        for (auto& indexId : sysMan.tables[tableId].indexes) {
            string indexName = sysMan.getIndexInfo(tableId, indexId).name;
            for (auto& column : sysMan.getIndexInfo(tableId, indexId).schema.columns)
                data.push_back({Value(indexName), Value(column.name)});
        }
        cout << prettyPrintTable(data, type);
    }
}

bool tryDDL(const unique_ptr<StatementNode>& n, TransactionManager& trMan, SystemInfoManager& sysMan) {
    if (auto crTab = convert<CreateTableNode>(n)) {
        executeCreateTable(crTab, sysMan);
        return true;
    }
    if (auto drTab = convert<DropTableNode>(n)) {
        executeDropTable(drTab, sysMan);
        return true;
    }
    if (auto crInd = convert<CreateIndexNode>(n)) {
        executeCreateIndex(crInd, trMan, sysMan);
        return true;
    }
    if (auto drInd = convert<DropIndexNode>(n)) {
        executeDropIndex(drInd, sysMan);
        return true;
    }
    if (auto show = convert<ShowNode>(n)) {
        executeShow(show, sysMan);
        return true;
    }
    return false;
}