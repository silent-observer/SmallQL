#pragma once

#include "SqlAst.h"
#include "SystemInfoManager.h"

bool tryDDL(const unique_ptr<StatementNode>& n, PageManager& pageManager, SystemInfoManager& sysMan);