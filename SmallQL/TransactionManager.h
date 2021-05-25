#pragma once

#include "PageManager.h"

class TransactionManager {
private:
    using PagePair = pair<VirtualFileID, PageId>;
    PageManager pageManager;
    mutable map<PagePair, pair<Page, bool>> transactionPages;
public:
    TransactionManager(string filename, string metaFilename)
        : pageManager(filename, metaFilename) {}

    Page retrieveRead(VirtualFileID fileId, PageId id) const;
    Page retrieveWrite(VirtualFileID fileId, PageId id) const;
    void update(VirtualFileID fileId, PageId id);
    inline void deleteFile(VirtualFileID fileId) { pageManager.deleteFile(fileId); }
    inline bool flushOne() { return pageManager.flushOne(); }
    inline void flushMetadata() { pageManager.flushMetadata(); }
    inline void flushAll() { pageManager.flushAll(); }

    void commit();
    void rollback();
};

class Pager {
private:
    TransactionManager& trMan;
    VirtualFileID fileId;
public:
    Pager(TransactionManager& trMan, VirtualFileID fileId)
        : trMan(trMan), fileId(fileId) {}

    inline Page retrieveRead(PageId id) const {
        return trMan.retrieveRead(fileId, id);
    }
    inline Page retrieveWrite(PageId id) const {
        return trMan.retrieveWrite(fileId, id);
    }
    inline void update(PageId id) {
        trMan.update(fileId, id);
    }
};