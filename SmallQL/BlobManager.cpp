#include "BlobManager.h"

bool operator<(const BlobManager::BlobData& l, const BlobManager::BlobData& r) {
    if (l.size < r.size) return true;
    if (l.size > r.size) return false;

    if (l.page < r.page) return true;
    if (l.page > r.page) return false;

    if (l.offset < r.offset) return true;
    return false;
}

BlobManager::BlobManager(PageManager& pageManager) : Pager(pageManager, 0xFFFF0000) {
    Page headerPage = retrieve(0);
    if (memcmp(headerPage, "SmDB", 4) != 0) {
        initFile(headerPage);
    }
    initPointers(headerPage);
    loadLists(headerPage);
}

void BlobManager::initPointers(Page headerPage) {
    fhlStart = (BlobId*)(headerPage + 0x08);
    handleCount = (uint32_t*)(headerPage + 0x0C);
    pageCount = (uint32_t*)(headerPage + 0x10);
}

static const int handlesPerIndex = 744;
static const int indexesPerMasterIndex = 2047;
static const int overflowSize = 8188;

void BlobManager::loadLists(Page headerPage) {
    Page fblData = headerPage + 0x1A;
    bool isFirst = true;
    while (true) {
        uint16_t entryCount = *(uint16_t*)fblData;
        for (int i = 0; i < entryCount; i++) {
            const char* entry = fblData + 2 + i * 10;
            BlobData data;
            data.size = *(uint32_t*)entry;
            data.page = *(uint32_t*)(entry + 0x4);
            data.offset = *(uint16_t*)(entry + 0x8);
            freeBlobList.insert(data);
        }
        PageId nextPage = *(uint32_t*)(isFirst ? headerPage + 0x1FFC : fblData + 0x1FFC);
        if (nextPage == NULL32) break;
        fblData = retrieve(nextPage);
        isFirst = false;
    }

    int indexPages = (*handleCount + handlesPerIndex - 1) / handlesPerIndex;
    Page masterIndexPage = retrieve(1);
    int indexPagesToBeRead = indexPages;
    while (true) {
        int count = min(indexPagesToBeRead, indexesPerMasterIndex);
        for (int i = 0; i < count; i++) {
            PageId indexPageId = *(PageId*)(masterIndexPage + i * 4);
            indexPageList.push_back(indexPageId);
        }
        PageId nextPageId = *(PageId*)(masterIndexPage + 2047 * 4);
        if (nextPageId == NULL32) break;
        masterIndexPage = retrieve(nextPageId);
    }
}

void BlobManager::initFile(Page headerPage) {
    memcpy(headerPage, "SmDB", 4);
    headerPage[0x04] = 0x01;
    headerPage[0x05] = 0x00;
    *(uint32_t*)(headerPage + 0x08) = NULL32;
    *(uint64_t*)(headerPage + 0x0C) = 0;
    *(uint32_t*)(headerPage + 0x10) = 2;
    *(uint16_t*)(headerPage + 0x1A) = 0;
    *(uint32_t*)(headerPage + 0x1FFC) = NULL32;
    update(0);

    Page masterIndexPage = retrieve(1);
    memset(masterIndexPage, 0xFF, PAGE_SIZE);
    update(1);
}

BlobManager::~BlobManager() {
    flushLists();
}

void BlobManager::flushLists() {
    PageId currentPageId = 0;
    Page fblData = retrieve(0) + 0x1A;
    bool isFirst = true;
    int fblSize = freeBlobList.size();
    auto iter = freeBlobList.begin();
    int i = 0;
    while (true) {
        int entriesLeft = fblSize - i;
        int maxEntryCount = isFirst ? 816 : 818;
        int actualEntryCount = min(maxEntryCount, entriesLeft);
        *(uint16_t*)fblData = actualEntryCount;
        for (int j = 0; j < actualEntryCount; j++, iter++) {
            const char* entry = fblData + 2 + j * 10;
            *(uint32_t*)entry = iter->size;
            *(uint32_t*)(entry + 0x4) = iter->page;
            *(uint16_t*)(entry + 0x8) = iter->offset;
        }
        i += actualEntryCount;
        if (i == fblSize) {
            *(uint32_t*)(isFirst ? fblData - 0x1A + 8188 : fblData + 8188) = NULL32;
            update(currentPageId);
            break;
        }
        PageId nextPage = *(uint32_t*)(isFirst ? fblData - 0x1A + 8188 : fblData + 8188);
        if (nextPage == NULL32) {
            nextPage = (*pageCount)++;
            *(uint32_t*)(isFirst ? fblData - 0x1A + 8188 : fblData + 8188) = nextPage;
        }
        update(currentPageId);

        currentPageId = nextPage;
        fblData = retrieve(nextPage);
        isFirst = false;
    }
}

pair<uint32_t, const char*> BlobManager::readBlob(BlobId id) const {
    if (id >= *handleCount)
        return make_pair(0, nullptr);
    int indexId = id / handlesPerIndex;
    int idInIndex = id % handlesPerIndex;
    Page indexPage = retrieve(indexPageList[indexId]);
    const char* entry = indexPage + idInIndex * 11;
    char status = *entry;
    if (status != 1) 
        return make_pair(0, nullptr);

    uint32_t size = *(uint32_t*)(entry + 0x1);
    uint32_t pageId = *(uint32_t*)(entry + 0x5);
    uint16_t offset = *(uint16_t*)(entry + 0x9);
    if (size < overflowSize) {
        Page page = retrieve(pageId);
        return make_pair(size, page + offset + 2);
    }
    else {
        uint32_t remainderSize = size % overflowSize;
        buffer.resize(size);
        Page page = retrieve(pageId);
        memcpy(buffer.data(), page + offset + 2, remainderSize);
        PageId overflowId = *(PageId*)(page + offset + 2 + remainderSize);
        int i = 0;
        while (true) {
            Page overflowPage = retrieve(overflowId);
            memcpy(buffer.data() + remainderSize + i * overflowSize, overflowPage, overflowSize);
            overflowId = *(PageId*)(overflowPage + overflowSize);
            if (overflowId == NULL32) break;
            i++;
        }
        return make_pair(size, buffer.data());
    }
}

bool BlobManager::writeBlob(BlobId id, const char* data, uint32_t size) {
    // TODO: make updating blobs more efficient than this
    if(!deleteBlob(id)) return false;
    addBlob(data, size);
    return true;
}

void BlobManager::pushBackIndex(PageId indexPageId) {
    int indexId = indexPageList.size();
    int masterIndexId = indexId / indexesPerMasterIndex;
    int idInMasterIndex = indexId % indexesPerMasterIndex;
    
    Page masterIndexPage = retrieve(1);
    PageId masterIndexPageId = 1;
    int desiredMasterIndexId = idInMasterIndex == 0 ? masterIndexId - 1 : masterIndexId;
    for (int i = 0; i < desiredMasterIndexId; i++) {
        masterIndexPageId = *(PageId*)(masterIndexPage + 2047 * 4);
        masterIndexPage = retrieve(masterIndexPageId);
    }
    if (idInMasterIndex == 0 && masterIndexId != 0) {
        PageId newMasterIndexPageId = (*pageCount)++;
        *(PageId*)(masterIndexPage + 2047 * 4) = newMasterIndexPageId;
        masterIndexPage = retrieve(newMasterIndexPageId);
        memset(masterIndexPage, 0xFF, PAGE_SIZE);
        update(masterIndexPageId);
        masterIndexPageId = newMasterIndexPageId;
    }

    *(PageId*)(masterIndexPage + idInMasterIndex * 4) = indexPageId;
    indexPageList.push_back(indexPageId);
    update(masterIndexPageId);
}

set<BlobManager::BlobData>::iterator BlobManager::findFreeBlobOfSize(uint32_t size) {
    if (freeBlobList.empty()) return freeBlobList.end();

    BlobData testBlob = {size, 0, 0};
    return freeBlobList.lower_bound(testBlob);
}

PageId BlobManager::getOverflowPage() {
    BlobData testBlob = {overflowSize, 0, 0};
    auto iter = freeBlobList.lower_bound(testBlob);
    if (iter == freeBlobList.end())
        return (*pageCount)++;
    else {
        PageId pageId = iter->page;
        freeBlobList.erase(iter);
        return pageId;
    }
}

const int minFreeBytes = 8;

BlobId BlobManager::addBlob(const char* data, uint32_t size) {
    BlobId handle;
    char* handleInfo;
    PageId indexPageId;
    if (*fhlStart != NULL32) {
        handle = *fhlStart;
        int indexId = handle / handlesPerIndex;
        int idInIndex = handle % handlesPerIndex;

        indexPageId = indexPageList[indexId];
        Page indexPage = retrieve(indexPageId);
        handleInfo = indexPage + idInIndex * 11;
        char status = *handleInfo;
        assert(status == 0);
        *fhlStart = *(BlobId*)(handleInfo + 0x1);
        update(0);
    }
    else {
        handle = (*handleCount)++;
        int indexId = handle / handlesPerIndex;
        int idInIndex = handle % handlesPerIndex;
        if (idInIndex == 0) {
            indexPageId = (*pageCount)++;
            pushBackIndex(indexPageId);
        }
        else {
            indexPageId = indexPageList[indexId];
        }
        Page indexPage = retrieve(indexPageId);
        handleInfo = indexPage + idInIndex * 11;
    }

    *handleInfo = 1;

    uint32_t remainderSize = size % overflowSize;
    uint32_t overflowPageCount = size / overflowSize;
    uint32_t firstBlobSize = remainderSize;
    if (overflowPageCount > 0)
        firstBlobSize += 4;

    auto freeBlobIter = findFreeBlobOfSize(firstBlobSize);
    BlobData initialBlobData;
    if (freeBlobIter == freeBlobList.end()) {
        initialBlobData.page = (*pageCount)++;
        initialBlobData.offset = 0;
        initialBlobData.size = PAGE_SIZE - 4;
    }
    else {
        initialBlobData = *freeBlobIter;
        freeBlobList.erase(freeBlobIter);
    }

    uint32_t freeBlobSize = initialBlobData.size - firstBlobSize;
    if (freeBlobSize < 4 + minFreeBytes)
        freeBlobSize = 0;

    BlobData activeBlobData;
    activeBlobData.size = initialBlobData.size - freeBlobSize;
    activeBlobData.page = initialBlobData.page;
    activeBlobData.offset = initialBlobData.offset;
    *(uint32_t*)(handleInfo + 0x1) = activeBlobData.size + overflowPageCount * overflowSize - (overflowPageCount > 0? 4 : 0);
    *(uint32_t*)(handleInfo + 0x5) = activeBlobData.page;
    *(uint16_t*)(handleInfo + 0x9) = activeBlobData.offset;

    Page dataPage = retrieve(initialBlobData.page);
    char* activeBlobStart = dataPage + activeBlobData.offset;
    *(uint16_t*)activeBlobStart = 0xFFFF;
    *(uint16_t*)(activeBlobStart + activeBlobData.size + 2) = 0xFFFF;
    memcpy(activeBlobStart + 2, data, remainderSize);

    if (overflowPageCount > 0) {
        PageId firstOverflowPageId = getOverflowPage();
        *(PageId*)(activeBlobStart + 2 + remainderSize) = firstOverflowPageId;
        Page firstOverflowPage = retrieve(firstOverflowPageId);
        *(PageId*)(firstOverflowPage + overflowSize) = NULL32;
        memcpy(firstOverflowPage, data + remainderSize, overflowSize);
        update(firstOverflowPageId);

        PageId previousOverflowPageId = firstOverflowPageId;
        Page previousOverflowPage = firstOverflowPage;
        for (int i = 1; i < overflowPageCount; i++) {
            PageId overflowPageId = getOverflowPage();
            Page overflowPage = retrieve(overflowPageId);
            *(PageId*)(previousOverflowPage + overflowSize) = overflowPageId;
            update(previousOverflowPageId);
            
            memcpy(overflowPage, data + remainderSize + i * overflowSize, overflowSize);
            *(PageId*)(overflowPage + overflowSize) = NULL32;
            previousOverflowPageId = overflowPageId;
            previousOverflowPage = overflowPage;
            update(overflowPageId);
        }
    }
    
    if (freeBlobSize != 0) {
        freeBlobSize -= 4;
        BlobData freeBlobData;
        freeBlobData.size = freeBlobSize;
        freeBlobData.page = initialBlobData.page;
        freeBlobData.offset = initialBlobData.offset + activeBlobData.size + 4;
        freeBlobList.insert(freeBlobData);

        char* freeBlobStart = dataPage + freeBlobData.offset;
        *(uint16_t*)freeBlobStart = freeBlobData.size;
        *(uint16_t*)(freeBlobStart + freeBlobData.size + 2) = freeBlobData.size;
    }
    update(initialBlobData.page);
    update(indexPageId);
    return handle;
}

bool BlobManager::deleteBlob(BlobId id) {
    if (id >= *handleCount) 
        return false;
    int indexId = id / handlesPerIndex;
    int idInIndex = id % handlesPerIndex;
    Page indexPage = retrieve(indexPageList[indexId]);
    char* entry = indexPage + idInIndex * 11;
    char status = *entry;
    if (status != 1) 
        return false;

    uint32_t size = *(uint32_t*)(entry + 0x1);
    uint32_t pageId = *(uint32_t*)(entry + 0x5);
    uint16_t offset = *(uint16_t*)(entry + 0x9);

    uint32_t remainderSize = size % overflowSize;
    uint32_t overflowPageCount = size / overflowSize;
    uint32_t firstBlobSize = remainderSize;
    if (overflowPageCount > 0)
        firstBlobSize += 4;

    *entry = 0;
    *(uint32_t*)(entry + 0x1) = *fhlStart;
    *fhlStart = id;
    update(0);
    update(indexPageList[indexId]);

    Page dataPage = retrieve(pageId);
    char* blobData = dataPage + offset;

    if (overflowPageCount > 0) {
        PageId overflowPageId = *(PageId*)(blobData + 2 + remainderSize);
        while (overflowPageId != NULL32) {
            Page overflowPage = retrieve(overflowPageId);
            PageId nextOverflowPageId = *(PageId*)(overflowPage + overflowSize);
            *(uint16_t*)overflowPage = overflowSize;
            *(uint16_t*)(overflowPage + overflowSize + 2) = overflowSize;
            BlobData overflowBlob;
            overflowBlob.page = overflowPageId;
            overflowBlob.offset = 0;
            overflowBlob.size = overflowSize;
            freeBlobList.insert(overflowBlob);
            update(overflowPageId);
            overflowPageId = nextOverflowPageId;
        }
    }

    if (offset != 0 && *(uint16_t*)(blobData - 2) != 0xFFFF) {
        uint16_t addSize = *(uint16_t*)(blobData - 2);
        
        BlobData leftBlobData;
        leftBlobData.size = addSize;
        leftBlobData.page = pageId;
        leftBlobData.offset = offset - addSize - 4;
        freeBlobList.erase(leftBlobData);

        firstBlobSize += addSize + 4;
        offset -= addSize + 4;
        blobData = dataPage + offset;
    }
    if (offset + firstBlobSize + 4 < PAGE_SIZE && *(uint16_t*)(blobData + firstBlobSize + 4) != 0xFFFF) {
        uint16_t addSize = *(uint16_t*)(blobData + firstBlobSize + 4);
        
        BlobData rightBlobData;
        rightBlobData.size = addSize;
        rightBlobData.page = pageId;
        rightBlobData.offset = offset + firstBlobSize + 4;
        freeBlobList.erase(rightBlobData);
        
        firstBlobSize += addSize + 4;
    }
    *(uint16_t*)blobData = firstBlobSize;
    *(uint16_t*)(blobData + firstBlobSize + 2) = firstBlobSize;
    freeBlobList.insert({ firstBlobSize, pageId, offset });
    update(pageId);
    return true;
}