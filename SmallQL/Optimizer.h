#pragma once

#include "SystemInfoManager.h"
#include "QueryTree.h"

void optimizeConstants(QTablePtr& tree);
void optimizeConditions(QTablePtr& tree);
void optimizeTableReads(QTablePtr& tree, const SystemInfoManager& sysMan);

static inline void optimize(QTablePtr& tree, const SystemInfoManager& sysMan) {
    optimizeConstants(tree);
    optimizeConditions(tree);
    optimizeTableReads(tree, sysMan);
}