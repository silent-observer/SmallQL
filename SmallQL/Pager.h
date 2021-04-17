#pragma once

#include "Common.h"
#include <map>
#include <cstdint>
#include <cstdio>
#include <string>

using namespace std;
const int PAGE_SIZE = 8192;
using Page = char*;

class Pager {
private:
    FILE* f;
    mutable map<PageId, pair<Page, bool>> cache;
public:
    Pager(string filename);
    ~Pager();

    Page retrieve(PageId id) const;
    void update(PageId id);
    void flush();
};