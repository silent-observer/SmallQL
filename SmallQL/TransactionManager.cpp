#include "TransactionManager.h"

Page TransactionManager::retrieveRead(VirtualFileID fileId, PageId id) const {
    PagePair p = make_pair(fileId, id);
    if (transactionPages.count(p) != 0)
        return transactionPages[p].first;
    else
        return pageManager.retrieve(fileId, id);
}

Page TransactionManager::retrieveWrite(VirtualFileID fileId, PageId id) const {
    PagePair p = make_pair(fileId, id);
    if (transactionPages.count(p) != 0)
        return transactionPages[p].first;
    
    Page copy = new char[PAGE_SIZE];
    memcpy(copy, pageManager.retrieve(fileId, id), PAGE_SIZE);
    transactionPages[p] = make_pair(copy, false);
    return copy;
}

void TransactionManager::update(VirtualFileID fileId, PageId id) {
    transactionPages[make_pair(fileId, id)].second = true;
}

void TransactionManager::commit() {
    for (const auto& p : transactionPages) {
        if (!p.second.second) continue;
        Page actualPage = pageManager.retrieve(p.first.first, p.first.second);
        memcpy(actualPage, p.second.first, PAGE_SIZE);
        pageManager.update(p.first.first, p.first.second);
    }
    transactionPages.clear();
}

void TransactionManager::rollback() {
    transactionPages.clear();
}