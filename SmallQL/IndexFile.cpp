#define CHECKS_ENABLED
#include "IndexFilePrivate.h"
#include "DataType.h"
#include <iostream>

#define INDEXFILE_ID(tableId, indexId) ((tableId & 0xFFFF) << 16 | (indexId & 0xFFFF))

IndexFile::IndexFile(PageManager& pageManager, int tableId, int indexId, const Schema& keySchema, bool isUnique)
    : Pager(pageManager, INDEXFILE_ID(tableId, indexId))
    , keySchema(keySchema)
    , isUnique(isUnique) {
    Page headerPage = retrieve(0);
    if (memcmp(headerPage, "SmDI", 4) != 0) {
        initFile(tableId, indexId, keySchema.getSize(), headerPage);
    }
    initPointers(headerPage);
}

IndexFile::IndexFile(PageManager& pageManager, const SystemInfoManager& sysMan, string tableName, string indexName)
    : IndexFile(pageManager, sysMan.getTableId(tableName), 
        sysMan.getIndexId(tableName, indexName), 
        sysMan.getIndexSchema(tableName, indexName),
        sysMan.getIndexInfo(tableName, indexName).isUnique) {}

void IndexFile::initPointers(Page headerPage) {
    keySize = *(uint16_t*)(headerPage + 0x0A);
    totalPageCount = (uint32_t*)(headerPage + 0x10);
    fplStart = (NodeId*)(headerPage + 0x14);
    rootPageId = (NodeId*)(headerPage + 0x18);
    page0Data = headerPage + 0x20;

    overflowMode = keySize <= 2030 ? 0 : keySize <= 4077 ? 1 : 2;
    leafCellSize = keySize + 8;
    internalCellSize = overflowMode ? 2029 : keySize + 12;

    cellsPerLeafPage = (PAGE_SIZE - 0x06) / (leafCellSize + 2);
    cellsPerInternalPage = overflowMode ? 4 : (PAGE_SIZE - 0x0A) / (internalCellSize + 2);

    if (*rootPageId == NULL32) {
        LeafNode newRoot = LeafNode::create(*this, NULL32, keySize);
        *rootPageId = newRoot.getId();
        (*totalPageCount)++;
        update(0);
    }
}

void IndexFile::initFile(int tableId, int indexId, uint16_t keySize, Page headerPage) {
    memcpy(headerPage, "SmDI", 4);
    headerPage[0x04] = 0x01;
    const int ZERO_PAGE_KEY_LIMIT = 2022;
    headerPage[0x05] = keySize > ZERO_PAGE_KEY_LIMIT? 0x01 : 0x00;
    *(uint16_t*)(headerPage + 0x06) = tableId;
    *(uint16_t*)(headerPage + 0x08) = indexId;
    *(uint16_t*)(headerPage + 0x0A) = keySize;
    *(uint32_t*)(headerPage + 0x10) = 1;
    *(NodeId*)(headerPage + 0x14) = NULL32;
    *(NodeId*)(headerPage + 0x18) = NULL32;
}

NodeId IndexFile::allocateFreePage() {
    if (*fplStart != NULL32) {
        NodeId result = *fplStart;
        Page p = retrieve(result);
        if (result == 0) p += 0x20;
        if (p[0] != 0x00) {
            cout << "File is in incorrect format!" << endl;
            return NULL32;
        }
        memcpy(fplStart, p + 2, 4);
        update(0);
        //if (result == 3)
        //    cout << "P+" << endl;
        return result;
    }

    if (*totalPageCount % PARENTING_NORMAL_SIZE == PARENTING_ZERO_SIZE)
        (*totalPageCount)++;

    return (*totalPageCount)++;
}
void IndexFile::deallocatePage(NodeId id) {
    //if (id == 24)
    //    cout << "P-" << endl;
    Page p = retrieve(id);
    if (p[0] == 0x00) {
        cout << "File is in incorrect format!" << endl;
        return;
    }
    p[0] = 0x00;
    p[1] = 0x00;
    memcpy(p + 2, fplStart, 4);
    *fplStart = id;
    update(0);
    update(id);
}

NodeId& IndexFile::nodeParent(NodeId id) {
    Page parentingPage;
    uint16_t idInPage;
    if (id < PARENTING_ZERO_SIZE) {
        parentingPage = retrieve(0) + 0x20;
        idInPage = id - 1;
        lastParentingPageId = 0;
    }
    else {
        uint32_t parentingNumber = (id - PARENTING_ZERO_SIZE) / PARENTING_NORMAL_SIZE;
        NodeId parentingId = parentingNumber * PARENTING_NORMAL_SIZE + PARENTING_ZERO_SIZE;
        parentingPage = retrieve(parentingId);
        idInPage = (id - PARENTING_ZERO_SIZE) % PARENTING_NORMAL_SIZE - 1;
        lastParentingPageId = parentingId;
    }
    return ((NodeId*)(parentingPage + 0x04))[idInPage];
}

void IndexFile::updateParent(NodeId id, NodeId parent) {
    nodeParent(id) = parent;
    update(lastParentingPageId);
}

bool IndexFile::addKey(const char* key, RecordId val) {
    NodeId currentId = *rootPageId;
    while (true) {
        Page page = retrieve(currentId);
        if (page[0] == 0x01) { // Internal
            InternalNode n(*this, currentId, keySize);
            n.consistencyCheck();
            n.internalConsistencyCheck();
            pair<bool, SlotId> p = isUnique? n.binarySearch(key) : n.binarySearch(key, val);
            if (p.first) return false;
            if (p.second == n.cellCount())
                currentId = n.rightPtr();
            else
                currentId = n.getCellLeftPtr(p.second);
        }
        else { // Leaf
            LeafNode n(*this, currentId, keySize);
            n.consistencyCheck();
            pair<bool, SlotId> p = isUnique? n.binarySearch(key) : n.binarySearch(key, val);
            if (p.first) return false;
            n.insertIntoNode(key, val);
            return true;
        }
    }
}

RecordId IndexFile::findKey(const char* key) {
    NodeId currentId = *rootPageId;
    while (true) {
        Page page = retrieve(currentId);
        if (page[0] == 0x01) { // Internal
            InternalNode n(*this, currentId, keySize);
            pair<bool, SlotId> p = n.binarySearch(key);
            if (p.first) return n.getCellRecord(p.second);
            if (p.second == n.cellCount())
                currentId = n.rightPtr();
            else
                currentId = n.getCellLeftPtr(p.second);
        }
        else { // Leaf
            LeafNode n(*this, currentId, keySize);
            pair<bool, SlotId> p = n.binarySearch(key);
            if (p.first) return n.getCellRecord(p.second);
            return NULL64;
        }
    }
}

bool IndexFile::deleteKey(const char* key, RecordId val) {
    NodeId currentId = *rootPageId;
    Page page = retrieve(currentId);
    if (page[0] == 0x01) { // Internal
        InternalNode n(*this, currentId, keySize);
        return n.deleteFromNode(key, val);
    }
    else { // Leaf
        LeafNode n(*this, currentId, keySize);
        return n.deleteFromNode(key, val);
    }
}

void IndexFile::fullConsistencyCheck(NodeId id) {
#ifdef CHECKS_ENABLED
    Page page = retrieve(id);
    if (page[0] == 0x01) { // Internal
        InternalNode n(*this, id, keySize);
        n.consistencyCheck();
        n.internalConsistencyCheck();
        for (int i = 0; i < n.cellCount(); i++)
            fullConsistencyCheck(n.getCellLeftPtr(i));
        fullConsistencyCheck(n.rightPtr());
    }
    else { // Leaf
        LeafNode n(*this, id, keySize);
        n.consistencyCheck();
    }
#endif
}

bool IndexFile::fillFrom(const DataFile& dataFile, const Schema& tableSchema) {
    for (auto iter = dataFile.begin(); iter != dataFile.end(); iter++) {
        auto decoded = tableSchema.decode(*iter, nullptr);
        auto keys = keySchema.narrow(decoded);
        auto keysEncoded = keySchema.encode(keys).first;
        bool result = addKey(keysEncoded, iter.getRecordId());
        if (!result)
            return false;
    }
    return true;
}

void IndexFile::CustomIterator::updateValue() {
    if (ptrStack.size() == 0) return;
    NodeId nodeId = ptrStack.back().first;
    SlotId slotId = ptrStack.back().second;
    Page p = indexFile->retrieve(nodeId);
    if (p[0] == 0x01) { // Internal
        InternalNode n(const_cast<IndexFile&>(*indexFile), nodeId, indexFile->keySize);
        recordId = n.getCellRecord(slotId);
    }
    else { // Leaf
        LeafNode n(const_cast<IndexFile&>(*indexFile), nodeId, indexFile->keySize);
        recordId = n.getCellRecord(slotId);
    }
}

void IndexFile::CustomIterator::advance() {
    while (true) {
        if (ptrStack.size() == 0) return;
        NodeId& nodeId = ptrStack.back().first;
        SlotId& slotId = ptrStack.back().second;
        Page p = indexFile->retrieve(nodeId);
        if (p[0] == 0x01) { // Internal
            InternalNode n(const_cast<IndexFile&>(*indexFile), nodeId, indexFile->keySize);
            if (slotId == n.cellCount()) {
                ptrStack.pop_back();
                continue;
            }

            slotId++;
            if (slotId == n.cellCount()) {
                ptrStack.push_back(make_pair(n.rightPtr(), 0));
                continue;
            }
            else {
                ptrStack.push_back(make_pair(n.getCellLeftPtr(slotId), 0));
                continue;
            }
        }
        else { // Leaf
            LeafNode n(const_cast<IndexFile&>(*indexFile), nodeId, indexFile->keySize);
            
            slotId++;
            if (slotId == n.cellCount()) {
                ptrStack.pop_back();
                continue;
            }
            break;
        }
    }
    updateValue();
}

IndexFile::const_iterator IndexFile::getIterator(const char* key) {
    vector<pair<NodeId, SlotId>> ptrStack;
    NodeId currentId = *rootPageId;
    while (true) {
        Page page = retrieve(currentId);
        if (page[0] == 0x01) { // Internal
            InternalNode n(*this, currentId, keySize);
            pair<bool, SlotId> p = n.binarySearch(key);
            ptrStack.emplace_back(currentId, p.second);
            if (p.first)
                return const_iterator(n.getCellRecord(p.second), ptrStack, this);
            if (p.second == n.cellCount()) {
                currentId = n.rightPtr();
            } 
            else
                currentId = n.getCellLeftPtr(p.second);
        }
        else { // Leaf
            LeafNode n(*this, currentId, keySize);
            pair<bool, SlotId> p = n.binarySearch(key);
            ptrStack.emplace_back(currentId, p.second);
            return const_iterator(n.getCellRecord(p.second), ptrStack, this);
        }
    }
}

IndexFile::const_iterator IndexFile::getIterator(ValueArray key) {
    vector<pair<NodeId, SlotId>> ptrStack;
    NodeId currentId = *rootPageId;
    while (true) {
        Page page = retrieve(currentId);
        if (page[0] == 0x01) { // Internal
            InternalNode n(*this, currentId, keySize);
            pair<bool, SlotId> p = n.binarySearch(key);
            ptrStack.emplace_back(currentId, p.second);
            if (p.first)
                return const_iterator(n.getCellRecord(p.second), ptrStack, this);
            if (p.second == n.cellCount()) {
                currentId = n.rightPtr();
            } 
            else
                currentId = n.getCellLeftPtr(p.second);
        }
        else { // Leaf
            LeafNode n(*this, currentId, keySize);
            pair<bool, SlotId> p = n.binarySearch(key);
            ptrStack.emplace_back(currentId, p.second);
            return const_iterator(n.getCellRecord(p.second), ptrStack, this);
        }
    }
}