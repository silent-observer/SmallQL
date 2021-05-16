#pragma once

#include "Common.h"
#include <cstdint>
#include <cstdio>
#include <vector>
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
    FILE* f;
    mutable map<PageId, pair<Page, bool>> cache;
    mutable queue<PageId> dirtyQueue;
    mutable uint32_t pageCount;
    string metaFilename;

public:
    PageManager(string filename, string metaFilename);
    ~PageManager();

    Page retrieve(VirtualFileID fileId, PageId id) const;
    void update(VirtualFileID fileId, PageId id);
    bool flushOne();
    void flushMetadata();
    inline void flushAll() {
        while (flushOne()) {}
        flushMetadata();
    }
};

class Pager {
private:
    PageManager& pageManager;
    VirtualFileID fileId;
public:
    Pager(PageManager& pageManager, VirtualFileID fileId)
        : pageManager(pageManager), fileId(fileId) {}

    inline Page retrieve(PageId id) const {
        return pageManager.retrieve(fileId, id);
    }
    inline void update(PageId id) {
        pageManager.update(fileId, id);
    }
};