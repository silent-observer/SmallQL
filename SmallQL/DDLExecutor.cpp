#include "DDLExecutor.h"
#include <iostream>
#include "DataFile.h"
#include "IndexFile.h"

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

static void executeCreateIndex(const CreateIndexNode* n, PageManager& pageManager, SystemInfoManager& sysMan) {
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
    IndexFile indexFile(pageManager, sysMan, n->tableName, n->name);
    DataFile dataFile(pageManager, sysMan, n->tableName);
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

bool tryDDL(const unique_ptr<StatementNode>& n, PageManager& pageManager, SystemInfoManager& sysMan) {
    if (auto crTab = convert<CreateTableNode>(n)) {
        executeCreateTable(crTab, sysMan);
        return true;
    }
    if (auto drTab = convert<DropTableNode>(n)) {
        executeDropTable(drTab, sysMan);
        return true;
    }
    if (auto crInd = convert<CreateIndexNode>(n)) {
        executeCreateIndex(crInd, pageManager, sysMan);
        return true;
    }
    if (auto drInd = convert<DropIndexNode>(n)) {
        executeDropIndex(drInd, sysMan);
        return true;
    }
    return false;
}