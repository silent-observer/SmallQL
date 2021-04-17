#pragma once

#include "SystemInfoManager.h"
#include "QueryTree.h"

void optimizeConditions(QTablePtr& tree);
void optimizeTableReads(QTablePtr& tree, const SystemInfoManager& sysMan);

static inline void optimize(QTablePtr& tree, const SystemInfoManager& sysMan) {
    optimizeConditions(tree);
    optimizeTableReads(tree, sysMan);
}