#include "DDLExecutor.h"
#include <iostream>

void executeCreateTable(const CreateTableNode* n, SystemInfoManager& sysMan) {
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

bool tryDDL(const unique_ptr<StatementNode>& n, SystemInfoManager& sysMan) {
    auto crTab = convert<CreateTableNode>(n);
    if (crTab) {
        executeCreateTable(crTab, sysMan);
        return true;
    }
    return false;
}