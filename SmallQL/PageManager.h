#pragma once

#include "Common.h"
#include <cstdint>
#include <cstdio>
#include <vector>
#include <set>
#include <queue>
#include <map>
#include <string>

using VirtualFileID = uint32_t;
const int PAGE_SIZE = 8192;
using Page = char*;

class PageManager {
private:
    using PageMap = vector<PageId>;
    mutable map<VirtualFileID, PageMap> metadata;
    mutable set<PageId> deletedPages;
    FILE* f;
    mutable map<PageId, pair<Page, bool>> cache;
    mutable queue<PageId> dirtyQueue;
    mutable uint32_t pageCount;
    mutable bool metadataUpdated;
    string metaFilename;

    PageId getFreePage(bool hasMinPage, PageId minPage) const;
public:
    PageManager(string filename, string metaFilename);
    ~PageManager();

    Page retrieve(VirtualFileID fileId, PageId id) const;
    void update(VirtualFileID fileId, PageId id);
    void deleteFile(VirtualFileID fileId);
    bool flushOne();
    void flushMetadata();
    inline void flushAll() {
        while (flushOne()) {}
        flushMetadata();
    }
};