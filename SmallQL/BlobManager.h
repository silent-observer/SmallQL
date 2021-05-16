#pragma once

#include "PageManager.h"
#include <vector>
#include <set>

using BlobId = uint32_t;

class BlobManager : public Pager {
private:
    struct BlobData {
        uint32_t size;
        PageId page;
        uint16_t offset;
    };

    uint32_t* handleCount;
    uint32_t* pageCount;
    BlobId* fhlStart;

    set<BlobData> freeBlobList;
    vector<PageId> indexPageList;

    mutable vector<char> buffer;

    void initFile(Page headerPage);
    void initPointers(Page headerPage);
    void loadLists(Page headerPage);
    void pushBackIndex(PageId indexPageId);
    friend bool operator<(const BlobData& l, const BlobData& r);

    set<BlobData>::iterator findFreeBlobOfSize(uint32_t size);
    PageId getOverflowPage();
public:
    BlobManager(PageManager& pageManager);
    ~BlobManager();
    void flushLists();
    pair<uint32_t, const char*> readBlob(BlobId id) const;
    inline vector<char> readBlobVector(BlobId id) const {
        auto blob = readBlob(id);
        return vector<char>(blob.second, blob.second + blob.first);
    }

    bool writeBlob(BlobId id, const char* data, uint32_t size);
    inline bool writeBlob(BlobId id, const vector<char>& data) {
        return writeBlob(id, data.data(), data.size());
    };

    BlobId addBlob(const char* data, uint32_t size);
    inline BlobId addBlob(const vector<char>& data) {
        return addBlob(data.data(), data.size());
    }
    bool deleteBlob(BlobId id);
};