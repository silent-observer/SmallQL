#pragma once
#include "Common.h"
#include "IndexFile.h"
#include <set>
#include <iostream>

class TreeNode {
protected:
    NodeId id;
    Page page;
    uint16_t cellSize;
    uint16_t cellPtrOffset;
    uint16_t cellHeader;
    uint16_t fclStartOffset;
    IndexFile& index;
public:
    uint16_t maxCellCount;
    uint16_t minCellCount;
    TreeNode(IndexFile& index, uint16_t keySize, uint16_t cellPtrOffset, 
        uint16_t cellHeader, uint16_t fclStartOffset)
    : TreeNode(index, index.allocateFreePage(), keySize, 
        cellPtrOffset, cellHeader, 
        fclStartOffset) {}
    TreeNode(IndexFile& index, NodeId id, uint16_t keySize, uint16_t cellPtrOffset, 
        uint16_t cellHeader, uint16_t fclStartOffset)
        : index(index)
        , id(id)
        , page(index.retrieve(id))
        , cellSize(keySize + cellHeader)
        , cellHeader(cellHeader)
        , cellPtrOffset(cellPtrOffset)
        , fclStartOffset(fclStartOffset)
        , maxCellCount(0)
        , minCellCount(0) {}
    inline uint16_t& cellCount() {
        return *(uint16_t*)(page + 0x02);
    }
    inline NodeId getId() {
        return id;
    }
    inline NodeId& parent() {
        return index.nodeParent(id);
    }
    inline char* getCellRaw(CellId id) {
        return page + PAGE_SIZE - (id + 1) * cellSize;
    }
    inline CellId& cellId(SlotId id) {
        return ((CellId*)(page + cellPtrOffset))[id];
    }
    inline char* getCell(SlotId id) {
        return getCellRaw(cellId(id));
    }
    inline char* getCellData(SlotId id) {
        return getCell(id) + cellHeader;
    }
    inline RecordId& getCellRecord(SlotId id) {
        return *(RecordId*)getCell(id);
    }
    inline char* getCellDataRaw(CellId id) {
        return getCellRaw(id) + cellHeader;
    }
    inline RecordId& getCellRecordRaw(CellId id) {
        return *(RecordId*)getCellRaw(id);
    }
    inline CellId& fclStart() {
        return *(CellId*)(page + fclStartOffset);
    }
#ifdef CHECKS_ENABLED
    inline void consistencyCheck() {
        for (int i = 0; i < cellCount() - 1; i++) {
            assert(*(uint32_t*)getCellData(i) <= *(uint32_t*)getCellData(i + 1));
        }
    }
#else
    inline void consistencyCheck() {}
#endif // CHECKS_ENABLED

    

    CellId allocateCell() {
        assert(cellCount() < maxCellCount);
        if (fclStart() != NULL16) {
            CellId result = fclStart();
            fclStart() = *(CellId*)getCellRaw(result);
            index.update(id);
            return result;
        }
        return cellCount();
    }
    inline void deallocateCell(CellId cell) {
        *(CellId*)getCellRaw(cell) = fclStart();
        fclStart() = cell;
        index.update(id);
    }
    
    inline int compareKey(const char* key, SlotId slotId) {
        char* otherKey = getCellData(slotId);
        return index.getKeySchema().compare(key, otherKey);
    }
    inline int compareKey(const char* key, SlotId slotId, RecordId record) {
        int r = compareKey(key, slotId);
        if (r != 0) return r;
        return cmp(record, getCellRecord(slotId));
    }

    inline int compareKey(ValueArray key, SlotId slotId) {
        char* otherKey = getCellData(slotId);
        return index.getKeySchema().compare(key, otherKey);
    }
    inline int compareKey(ValueArray key, SlotId slotId, RecordId record) {
        int r = compareKey(key, slotId);
        if (r != 0) return r;
        return cmp(record, getCellRecord(slotId));
    }

    pair<bool, SlotId> binarySearch(const char* key) {
        if (cellCount() == 0) return make_pair(false, 0);
        SlotId l = 0;
        SlotId r = cellCount();
        while (l < r) {
            SlotId mid = l + (r - l) / 2;
            int result = compareKey(key, mid);
            if (result == 0)
                return make_pair(true, mid);
            if (result < 0)
                r = mid;
            else
                l = mid + 1;
        }
        return make_pair(false, r);
    }

    pair<bool, SlotId> binarySearch(const char* key, RecordId record) {
        if (cellCount() == 0) return make_pair(false, 0);
        SlotId l = 0;
        SlotId r = cellCount();
        while (l < r) {
            SlotId mid = l + (r - l) / 2;
            int result = compareKey(key, mid, record);
            if (result == 0)
                return make_pair(true, mid);
            if (result < 0)
                r = mid;
            else
                l = mid + 1;
        }
        return make_pair(false, r);
    }

    pair<bool, SlotId> binarySearch(ValueArray key) {
        if (cellCount() == 0) return make_pair(false, 0);
        SlotId l = 0;
        SlotId r = cellCount();
        while (l < r) {
            SlotId mid = l + (r - l) / 2;
            int result = compareKey(key, mid);
            if (result == 0)
                return make_pair(true, mid);
            if (result < 0)
                r = mid;
            else
                l = mid + 1;
        }
        return make_pair(false, r);
    }

    pair<bool, SlotId> binarySearch(ValueArray key, RecordId record) {
        if (cellCount() == 0) return make_pair(false, 0);
        SlotId l = 0;
        SlotId r = cellCount();
        while (l < r) {
            SlotId mid = l + (r - l) / 2;
            int result = compareKey(key, mid, record);
            if (result == 0)
                return make_pair(true, mid);
            if (result < 0)
                r = mid;
            else
                l = mid + 1;
        }
        return make_pair(false, r);
    }

    inline void fillNextSlotData(const char* data) {
        CellId id = allocateCell();
        cellId(cellCount()) = id;
        memcpy(getCellRaw(id), data, cellSize);
        cellCount()++;
        index.update(this->id);
    }

    struct SiblingResult {
        NodeId left, right;
        char* keyLeft, *keyRight;
        RecordId recordLeft, recordRight;
    };

    inline SiblingResult getSiblings();
};

class InternalNode : public TreeNode {
protected:
    uint16_t keySize;
public:
    static InternalNode create(IndexFile& index, NodeId parent, uint16_t keySize) {
        InternalNode result(index, index.allocateFreePage(), keySize);
        Page p = result.page;
        p[0] = 0x01;
        p[1] = 0x00;
        *(uint16_t*)(p + 0x02) = 0;
        index.updateParent(result.id, parent);
        *(uint32_t*)(p + 0x04) = NULL32;
        *(uint16_t*)(p + 0x08) = NULL16;
        index.update(result.id);
        return result;
    }
    InternalNode(IndexFile& index, NodeId id, uint16_t keySize)
        : TreeNode(index, id, keySize, 0x0A, 12, 0x08)
        , keySize(keySize) {
        maxCellCount = index.cellsPerInternalPage;
        minCellCount = maxCellCount / 2;
    }

    inline NodeId& rightPtr() {
        return *(NodeId*)(page + 0x04);
    }
    inline NodeId& getCellLeftPtr(SlotId id) {
        return *(NodeId*)(getCell(id) + 8);
    }
    inline NodeId& getCellLeftPtrRaw(CellId id) {
        return *(NodeId*)(getCellRaw(id) + 8);
    }

    inline void fillNextSlot(const char* key, RecordId record, NodeId childId) {
        CellId id = allocateCell();
        cellId(cellCount()) = id;
        memcpy(getCellRaw(id), &record, 8);
        memcpy(getCellRaw(id) + 8, &childId, 4);
        memcpy(getCellRaw(id) + cellHeader, key, keySize);
        cellCount()++;
        index.update(this->id);
    }

    struct SplitResult {
        NodeId left, right;
        char* key;
        RecordId record;
    };

    SplitResult splitNode(char* key, RecordId record, NodeId childId, SlotId position) {
        SlotId median = cellCount() / 2;
        InternalNode left = InternalNode::create(index, parent(), keySize);
        InternalNode right = InternalNode::create(index, parent(), keySize);
        for (int i = 0; i < median; i++) {
            if (i == position) {
                left.fillNextSlot(key, record, childId);
                index.updateParent(childId, left.id);
            }
            left.fillNextSlotData(getCell(i));
            index.updateParent(getCellLeftPtr(i), left.id);
        }
        left.rightPtr() = getCellLeftPtr(median);
        index.updateParent(left.rightPtr(), left.id);

        if (median == position) {
            left.fillNextSlot(key, record, childId);
            index.updateParent(childId, left.id);
        }
        for (int i = median + 1; i < cellCount(); i++) {
            if (i == position) {
                right.fillNextSlot(key, record, childId);
                index.updateParent(childId, right.id);
            }
            right.fillNextSlotData(getCell(i));
            index.updateParent(getCellLeftPtr(i), right.id);
        }
        if (position == cellCount()) {
            right.fillNextSlot(key, record, childId);
            index.updateParent(childId, right.id);
        }
        right.rightPtr() = rightPtr();
        index.updateParent(right.rightPtr(), right.id);

        SplitResult result;
        result.left = left.id;
        result.right = right.id;
        
        memcpy(key, getCellData(median), keySize);
        result.key = key;
        result.record = getCellRecord(median);
        
        index.deallocatePage(id);
        index.update(id);
        index.update(left.id);
        index.update(right.id);
        left.consistencyCheck();
        right.consistencyCheck();
        return result;
    }

    void updateChild(const char* key, RecordId record, NodeId newChild) {
        pair<bool, SlotId> p = binarySearch(key, record);
        if (p.second == cellCount())
            rightPtr() = newChild;
        else
            getCellLeftPtr(p.second) = newChild;
        index.update(id);
        index.updateParent(newChild, id);

        internalConsistencyCheck();
        consistencyCheck();
    }

    void updateEntry(const char* key, RecordId record, const char* newKey, RecordId newRecord) {
        internalConsistencyCheck();
        consistencyCheck();

        pair<bool, SlotId> p = binarySearch(key, record);
        CellId cell = cellId(p.second);
        memcpy(getCellDataRaw(cell), newKey, keySize);
        getCellRecordRaw(cell) = newRecord;
        index.update(id);
        
        internalConsistencyCheck();
        consistencyCheck();
    }

#ifdef CHECKS_ENABLED
    inline void internalConsistencyCheck() {
        for (int i = 0; i < cellCount(); i++) {
            assert(index.nodeParent(getCellLeftPtr(i)) == this->id);
        }
        assert(index.nodeParent(rightPtr()) == this->id);
    }
#else
    inline void internalConsistencyCheck() {}
#endif // CHECKS_ENABLED

    

    void insertIntoChild(char* key, RecordId record);
    void insertIntoChild(char* key, RecordId record, NodeId newChild) {
        pair<bool, SlotId> p = binarySearch(key, record);
        NodeId currentId = rightPtr();
        if (p.second != cellCount())
            currentId = getCellLeftPtr(p.second);
        InternalNode n(index, currentId, keySize);
        n.insertIntoNode(key, record, newChild);
        consistencyCheck();
        internalConsistencyCheck();
        n.consistencyCheck();
        n.internalConsistencyCheck();
    }

    void insertIntoNode(char* key, RecordId record, NodeId newChild) {
        internalConsistencyCheck();
        consistencyCheck();
        pair<bool, SlotId> p = binarySearch(key, record);
        if (p.first) return;
        if (cellCount() == maxCellCount) 
            if (tryRebalance(p.second)) {
                internalConsistencyCheck();
                consistencyCheck();
                InternalNode parentNode(index, parent(), keySize);
                parentNode.consistencyCheck();
                parentNode.internalConsistencyCheck();
                parentNode.insertIntoChild(key, record, newChild);
                return;
            }
        if (cellCount() < maxCellCount) {
            CellId* insertionPlace = &cellId(p.second);
            if (p.second != cellCount())
                memmove(insertionPlace + 1, insertionPlace, (cellCount() - p.second) * 2);
            CellId cell = allocateCell();
            *insertionPlace = cell;
            getCellRecordRaw(cell) = record;
            getCellLeftPtrRaw(cell) = newChild;
            memcpy(getCellDataRaw(cell), key, keySize);
            cellCount()++;
            index.update(id);
            index.updateParent(newChild, id);
            internalConsistencyCheck();
            consistencyCheck();
            return;
        }

        NodeId parentId = parent();
        SplitResult split = splitNode(key, record, newChild, p.second);
        if (parentId == NULL32) {
            InternalNode newRoot = InternalNode::create(index, NULL32, keySize);
            newRoot.fillNextSlot(split.key, split.record, split.left);
            newRoot.rightPtr() = split.right;
            index.setNewRoot(newRoot.getId());
            index.update(newRoot.getId());
            index.updateParent(split.left, newRoot.getId());
            index.updateParent(split.right, newRoot.getId());
            internalConsistencyCheck();
            consistencyCheck();
            newRoot.internalConsistencyCheck();
            newRoot.consistencyCheck();
            return;
        }

        InternalNode parentNode(index, parentId, keySize);
        
        parentNode.updateChild(split.key, split.record, split.right);
        parentNode.insertIntoNode(split.key, split.record, split.left);
    }

    void rebalanceLeft(InternalNode& leftNode, InternalNode& parentNode) {
        uint16_t leftCount = leftNode.cellCount();
        uint16_t thisCount = this->cellCount();
        uint16_t totalCount = leftCount + thisCount + 1;
        uint16_t newMedianPosition = totalCount / 2;
        uint16_t targetLeftCount = newMedianPosition;
        uint16_t targetThisCount = totalCount - 1 - targetLeftCount;
        uint16_t cellsToMove = targetLeftCount - leftCount - 1;
        uint16_t newMedianPositionInThisNode = newMedianPosition - leftCount - 1;

        // find the current median
        CellId zeroCell = this->cellId(0);
        char* leastKey = this->getCellDataRaw(zeroCell);
        RecordId leastRecord = this->getCellRecordRaw(zeroCell);
        pair<bool, SlotId> p = parentNode.binarySearch(leastKey, leastRecord); // the left pointer of this should be this node
        SlotId oldMedianSlot = p.second - 1;
        CellId oldMedianCell = parentNode.cellId(oldMedianSlot);
        char* oldMedianKey = parentNode.getCellDataRaw(oldMedianCell);
        RecordId& oldMedianRecord = parentNode.getCellRecordRaw(oldMedianCell);

        // copy the current median to the end of left node
        leftNode.fillNextSlot(oldMedianKey, oldMedianRecord, leftNode.rightPtr());
        // copy all the needed entries from this node
        for (int i = 0; i < cellsToMove; i++) {
            CellId sourceCell = this->cellId(i);
            leftNode.fillNextSlotData(this->getCellRaw(sourceCell));
            index.updateParent(this->getCellLeftPtrRaw(sourceCell), leftNode.id);
            this->cellCount()--;
            this->deallocateCell(sourceCell);
        }
        // copy new median
        CellId newMedianCell = this->cellId(newMedianPositionInThisNode);
        leftNode.rightPtr() = this->getCellLeftPtrRaw(newMedianCell);
        index.updateParent(leftNode.rightPtr(), leftNode.id);
        memcpy(oldMedianKey, this->getCellDataRaw(newMedianCell), keySize);
        oldMedianRecord = this->getCellRecordRaw(newMedianCell);
        
        this->cellCount()--;
        this->deallocateCell(newMedianCell);
        // move the rest of the keys in this node
        memmove(&this->cellId(0), &this->cellId(cellsToMove + 1), sizeof(CellId) * targetThisCount);
        consistencyCheck();
        internalConsistencyCheck();
        leftNode.consistencyCheck();
        leftNode.internalConsistencyCheck();
        parentNode.consistencyCheck();
        parentNode.internalConsistencyCheck();
    }

    void rebalanceRight(InternalNode& rightNode, InternalNode& parentNode) {
        uint16_t rightCount = rightNode.cellCount();
        uint16_t thisCount = this->cellCount();
        uint16_t totalCount = rightCount + thisCount + 1;
        uint16_t newMedianPosition = (totalCount - 1) / 2;
        uint16_t targetThisCount = newMedianPosition;
        uint16_t targetRightCount = totalCount - 1 - targetThisCount;
        uint16_t cellsToMove = targetRightCount - rightCount - 1;

        // find the current median
        CellId zeroCell = this->cellId(0);
        char* leastKey = this->getCellDataRaw(zeroCell);
        RecordId leastRecord = this->getCellRecordRaw(zeroCell);
        pair<bool, SlotId> p = parentNode.binarySearch(leastKey, leastRecord); // the left pointer of this should be this node
        SlotId oldMedianSlot = p.second;
        CellId oldMedianCell = parentNode.cellId(oldMedianSlot);
        char* oldMedianKey = parentNode.getCellDataRaw(oldMedianCell);
        RecordId& oldMedianRecord = parentNode.getCellRecordRaw(oldMedianCell);

        // free space for the slots in the right node
        memmove(&rightNode.cellId(cellsToMove + 1), &rightNode.cellId(0), sizeof(CellId) * rightCount);
        
        // copy the old median to the right node
        CellId oldMedianNewCell = rightNode.allocateCell();
        rightNode.cellId(cellsToMove) = oldMedianNewCell;
        rightNode.getCellRecordRaw(oldMedianNewCell) = oldMedianRecord;
        memcpy(rightNode.getCellDataRaw(oldMedianNewCell), oldMedianKey, keySize);
        rightNode.getCellLeftPtrRaw(oldMedianNewCell) = this->rightPtr();
        index.updateParent(rightNode.getCellLeftPtrRaw(oldMedianNewCell), rightNode.id);
        rightNode.cellCount()++;

        // copy the other part of the cells from this node to the right
        for (int i = cellsToMove - 1; i >= 0; i--) {
            CellId newCellId = rightNode.allocateCell();
            rightNode.cellId(i) = newCellId;
            CellId sourceCell = this->cellId(thisCount - cellsToMove + i);
            memcpy(rightNode.getCellRaw(newCellId), this->getCellRaw(sourceCell), cellSize);
            index.updateParent(rightNode.getCellLeftPtrRaw(newCellId), rightNode.id);
            rightNode.cellCount()++;
            this->cellCount()--;
            this->deallocateCell(sourceCell);
        }

        // copy the new median into place
        CellId newMedianCell = this->cellId(newMedianPosition);
        this->rightPtr() = this->getCellLeftPtrRaw(newMedianCell);
        memcpy(oldMedianKey, this->getCellDataRaw(newMedianCell), keySize);
        oldMedianRecord = this->getCellRecordRaw(newMedianCell);
        this->cellCount()--;
        this->deallocateCell(newMedianCell);
        consistencyCheck();
        internalConsistencyCheck();
        rightNode.consistencyCheck();
        rightNode.internalConsistencyCheck();
        parentNode.consistencyCheck();
        parentNode.internalConsistencyCheck();
    }

    bool tryRebalance(SlotId insertionPlace) {
        SiblingResult siblings = getSiblings();
        if (siblings.left != NULL32) {
            InternalNode leftNode(index, siblings.left, keySize);
            if (leftNode.cellCount() >= leftNode.maxCellCount - 1) return false;
            InternalNode parentNode(index, parent(), keySize);
            consistencyCheck();
            internalConsistencyCheck();
            leftNode.consistencyCheck();
            leftNode.internalConsistencyCheck();
            parentNode.consistencyCheck();
            parentNode.internalConsistencyCheck();

            rebalanceLeft(leftNode, parentNode);
            index.update(this->id);
            index.update(this->parent());
            index.update(leftNode.id);
            
            consistencyCheck();
            internalConsistencyCheck();
            leftNode.consistencyCheck();
            leftNode.internalConsistencyCheck();
            parentNode.consistencyCheck();
            parentNode.internalConsistencyCheck();
            return true;
        }
        if (siblings.right != NULL32) {
            InternalNode rightNode(index, siblings.right, keySize);
            if (rightNode.cellCount() >= rightNode.maxCellCount - 1) return false;
            InternalNode parentNode(index, parent(), keySize);
            consistencyCheck();
            internalConsistencyCheck();
            rightNode.consistencyCheck();
            rightNode.internalConsistencyCheck();
            parentNode.consistencyCheck();
            parentNode.internalConsistencyCheck();

            rebalanceRight(rightNode, parentNode);
            index.update(this->id);
            index.update(this->parent());
            index.update(rightNode.id);

            consistencyCheck();
            internalConsistencyCheck();
            rightNode.consistencyCheck();
            rightNode.internalConsistencyCheck();
            parentNode.consistencyCheck();
            parentNode.internalConsistencyCheck();
            return true;
        }
        return false;
    }

    NodeId findRightmostNode() {
        NodeId currentId = rightPtr();
        while (true) {
            Page page = index.retrieve(currentId);
            if (page[0] == 0x01) { // Internal
                InternalNode n(index, currentId, keySize);
                currentId = n.rightPtr();
            }
            else
                return currentId;
        }
    }

    bool deleteFromNode(const char* key, RecordId record);
};

class LeafNode : public TreeNode {
protected:
    uint16_t keySize;
public:
    static LeafNode create(IndexFile& index, NodeId parent, uint16_t keySize) {
        LeafNode result(index, index.allocateFreePage(), keySize);
        Page p = result.page;
        p[0] = 0x03;
        p[1] = 0x00;
        *(uint16_t*)(p + 0x02) = 0;
        index.updateParent(result.id, parent);
        *(uint16_t*)(p + 0x04) = NULL16;
        index.update(result.id);
        return result;
    }
    LeafNode(IndexFile& index, NodeId id, uint16_t keySize)
        : TreeNode(index, id, keySize, 0x06, 8, 0x04)
        , keySize(keySize) {
        maxCellCount = index.cellsPerLeafPage;
        minCellCount = maxCellCount / 2;
    }

    inline void fillNextSlot(const char* key, RecordId record) {
        CellId id = allocateCell();
        cellId(cellCount()) = id;
        memcpy(getCellRaw(id), &record, 8);
        memcpy(getCellRaw(id) + cellHeader, key, keySize);
        cellCount()++;
        index.update(this->id);
    }

    struct SplitResult {
        NodeId left, right;
        char* key;
        RecordId record;
    };

    SplitResult splitNode(char* key, RecordId record, SlotId position) {
        SlotId median = cellCount() / 2;
        LeafNode left = LeafNode::create(index, parent(), keySize);
        LeafNode right = LeafNode::create(index, parent(), keySize);
        for (int i = 0; i < median; i++) {
            if (i == position) left.fillNextSlot(key, record);
            left.fillNextSlotData(getCell(i));
        }
        if (median == position) left.fillNextSlot(key, record);
        for (int i = median + 1; i < cellCount(); i++) {
            if (i == position) right.fillNextSlot(key, record);
            right.fillNextSlotData(getCell(i));
        }
        if (position == cellCount())
            right.fillNextSlot(key, record);

        SplitResult result;
        result.left = left.id;
        result.right = right.id;

        memcpy(key, getCellData(median), keySize);
        result.key = key;
        result.record = getCellRecord(median);

        index.deallocatePage(id);
        index.update(id);
        index.update(left.id);
        index.update(right.id);
        left.consistencyCheck();
        right.consistencyCheck();
        return result;
    }
    void insertIntoNode(const char* key, RecordId record) {
        pair<bool, SlotId> p = binarySearch(key, record);
        if (p.first) return;
        if (cellCount() == maxCellCount) 
            if (tryRebalance(p.second)) {
                consistencyCheck();
                InternalNode parentNode(index, parent(), keySize);
                parentNode.consistencyCheck();
                parentNode.internalConsistencyCheck();
                char* keyCopy = new char[keySize];
                memcpy(keyCopy, key, keySize);
                parentNode.insertIntoChild(keyCopy, record);
                delete[] keyCopy;
                return;
            }
        if (cellCount() < maxCellCount) {
            CellId* insertionPlace = &cellId(p.second);
            if (p.second != cellCount())
                memmove(insertionPlace + 1, insertionPlace, (cellCount() - p.second) * 2);
            CellId cell = allocateCell();
            *insertionPlace = cell;
            *(RecordId*)getCellRaw(cell) = record;
            memcpy(getCellRaw(cell) + 8, key, keySize);
            cellCount()++;
            index.update(id);
            consistencyCheck();
            return;
        }

        char* keyCopy = new char[keySize];
        memcpy(keyCopy, key, keySize);

        NodeId parentId = parent();
        SplitResult split = splitNode(keyCopy, record, p.second);
        if (parentId == NULL32) {
            InternalNode newRoot = InternalNode::create(index, NULL32, keySize);
            LeafNode leftNode(index, split.left, keySize);
            LeafNode rightNode(index, split.right, keySize);
            newRoot.fillNextSlot(split.key, split.record, split.left);
            newRoot.rightPtr() = split.right;
            index.updateParent(split.left, newRoot.getId());
            index.updateParent(split.right, newRoot.getId());
            index.setNewRoot(newRoot.getId());
            index.update(newRoot.getId());
            consistencyCheck();
            newRoot.consistencyCheck();
            newRoot.internalConsistencyCheck();
            leftNode.consistencyCheck();
            rightNode.consistencyCheck();
            delete[] keyCopy;
            return;
        }

        InternalNode parentNode(index, parentId, keySize);
        
        parentNode.updateChild(split.key, split.record, split.right);
        parentNode.insertIntoNode(split.key, split.record, split.left);
        delete[] keyCopy;
    }

    void rebalanceLeft(LeafNode& leftNode, InternalNode& parentNode) {
        uint16_t leftCount = leftNode.cellCount();
        uint16_t thisCount = this->cellCount();
        uint16_t totalCount = leftCount + thisCount + 1;
        uint16_t newMedianPosition = totalCount / 2;
        uint16_t targetLeftCount = newMedianPosition;
        uint16_t targetThisCount = totalCount - 1 - targetLeftCount;
        uint16_t cellsToMove = targetLeftCount - leftCount - 1;
        uint16_t newMedianPositionInThisNode = newMedianPosition - leftCount - 1;

        // find the current median
        CellId zeroCell = this->cellId(0);
        char* leastKey = this->getCellDataRaw(zeroCell);
        RecordId leastRecord = this->getCellRecordRaw(zeroCell);
        pair<bool, SlotId> p = parentNode.binarySearch(leastKey, leastRecord); // the left pointer of this should be this node
        SlotId oldMedianSlot = p.second - 1;
        CellId oldMedianCell = parentNode.cellId(oldMedianSlot);
        char* oldMedianKey = parentNode.getCellDataRaw(oldMedianCell);
        RecordId& oldMedianRecord = parentNode.getCellRecordRaw(oldMedianCell);

        // copy the current median to the end of left node
        leftNode.fillNextSlot(oldMedianKey, oldMedianRecord);
        // copy all the needed entries from this node
        for (int i = 0; i < cellsToMove; i++) {
            CellId sourceCell = this->cellId(i);
            leftNode.fillNextSlotData(this->getCellRaw(sourceCell));
            this->cellCount()--;
            this->deallocateCell(sourceCell);
        }
        // copy new median
        CellId newMedianCell = this->cellId(newMedianPositionInThisNode);
        memcpy(oldMedianKey, this->getCellDataRaw(newMedianCell), keySize);
        oldMedianRecord = this->getCellRecordRaw(newMedianCell);
        this->cellCount()--;
        this->deallocateCell(newMedianCell);
        // move the rest of the keys in this node
        memmove(&this->cellId(0), &this->cellId(cellsToMove + 1), sizeof(CellId) * targetThisCount);
        parentNode.consistencyCheck();
        parentNode.internalConsistencyCheck();
    }

    void rebalanceRight(LeafNode& rightNode, InternalNode& parentNode) {
        uint16_t rightCount = rightNode.cellCount();
        uint16_t thisCount = this->cellCount();
        uint16_t totalCount = rightCount + thisCount + 1;
        uint16_t newMedianPosition = (totalCount - 1) / 2;
        uint16_t targetThisCount = newMedianPosition;
        uint16_t targetRightCount = totalCount - 1 - targetThisCount;
        uint16_t cellsToMove = targetRightCount - rightCount - 1;

        // find the current median
        CellId zeroCell = this->cellId(0);
        char* leastKey = this->getCellDataRaw(zeroCell);
        RecordId leastRecord = this->getCellRecordRaw(zeroCell);
        pair<bool, SlotId> p = parentNode.binarySearch(leastKey, leastRecord); // the left pointer of this should be this node
        SlotId oldMedianSlot = p.second;
        CellId oldMedianCell = parentNode.cellId(oldMedianSlot);
        char* oldMedianKey = parentNode.getCellDataRaw(oldMedianCell);
        RecordId& oldMedianRecord = parentNode.getCellRecordRaw(oldMedianCell);

        // free space for the slots in the right node
        memmove(&rightNode.cellId(cellsToMove + 1), &rightNode.cellId(0), sizeof(CellId) * rightCount);
        
        // copy the old median to the right node
        CellId oldMedianNewCell = rightNode.allocateCell();
        rightNode.cellId(cellsToMove) = oldMedianNewCell;
        rightNode.getCellRecordRaw(oldMedianNewCell) = oldMedianRecord;
        memcpy(rightNode.getCellDataRaw(oldMedianNewCell), oldMedianKey, keySize);
        rightNode.cellCount()++;

        // copy the other part of the cells from this node to the right
        for (int i = cellsToMove - 1; i >= 0; i--) {
            CellId newCellId = rightNode.allocateCell();
            rightNode.cellId(i) = newCellId;
            CellId sourceCell = this->cellId(thisCount - cellsToMove + i);
            memcpy(rightNode.getCellRaw(newCellId), this->getCellRaw(sourceCell), cellSize);
            rightNode.cellCount()++;
            this->cellCount()--;
            this->deallocateCell(sourceCell);
        }

        // copy the new median into place
        CellId newMedianCell = this->cellId(newMedianPosition);
        memcpy(oldMedianKey, this->getCellDataRaw(newMedianCell), keySize);
        oldMedianRecord = this->getCellRecordRaw(newMedianCell);
        this->cellCount()--;
        this->deallocateCell(newMedianCell);
        parentNode.consistencyCheck();
        parentNode.internalConsistencyCheck();
    }

    bool tryRebalance(SlotId insertionPlace) {
        SiblingResult siblings = getSiblings();
        if (siblings.left != NULL32) {
            LeafNode leftNode(index, siblings.left, keySize);
            if (leftNode.cellCount() >= leftNode.maxCellCount - 1) return false;
            InternalNode parentNode(index, parent(), keySize);
            consistencyCheck();
            leftNode.consistencyCheck();
            parentNode.consistencyCheck();
            parentNode.internalConsistencyCheck();

            rebalanceLeft(leftNode, parentNode);
            index.update(this->id);
            index.update(this->parent());
            index.update(leftNode.id);
            return true;
        }
        if (siblings.right != NULL32) {
            LeafNode rightNode(index, siblings.right, keySize);
            if (rightNode.cellCount() >= rightNode.maxCellCount - 1) return false;
            InternalNode parentNode(index, parent(), keySize);
            consistencyCheck();
            rightNode.consistencyCheck();
            parentNode.consistencyCheck();
            parentNode.internalConsistencyCheck();

            rebalanceRight(rightNode, parentNode);
            index.update(this->id);
            index.update(this->parent());
            index.update(rightNode.id);
            return true;
        }
        return false;
    }

    bool deleteFromNode(const char* key, RecordId record) {
        pair<bool, SlotId> p = binarySearch(key, record);
        if (!p.first) return false;

        CellId* deletionPlace = &cellId(p.second);
        if (p.second != cellCount() - 1)
            memmove(deletionPlace, deletionPlace + 1, (cellCount() - p.second - 1) * 2);
        deallocateCell(*deletionPlace);
        cellCount()--;
        index.update(id);
        consistencyCheck();
        return true;
    }
};

inline TreeNode::SiblingResult TreeNode::getSiblings() {
    SiblingResult result;
    result.left = NULL32;
    result.right = NULL32;
    result.keyLeft = NULL;
    result.keyRight = NULL;
    result.recordLeft = NULL64;
    result.recordRight = NULL64;
    if (parent() == NULL32) return result;
    InternalNode parentNode(index, parent(), cellSize - cellHeader);
    SlotId rightSlot = cellCount() - 1;
    char* key = getCellData(rightSlot);
    RecordId record = getCellRecord(rightSlot);
    pair<bool, SlotId> p = parentNode.binarySearch(key, record);
    if (p.second == 0) {
        result.right = parentNode.cellCount() == 1? parentNode.rightPtr() : parentNode.getCellLeftPtr(1);
        CellId cell = parentNode.cellId(0);
        result.keyRight = parentNode.getCellDataRaw(cell);
        result.recordRight = parentNode.getCellRecordRaw(cell);
    } 
    else if (p.second == parentNode.cellCount()) {
        CellId cell = parentNode.cellId(parentNode.cellCount() - 1);
        result.left = parentNode.getCellLeftPtrRaw(cell);
        result.keyLeft = parentNode.getCellDataRaw(cell);
        result.recordLeft = parentNode.getCellRecordRaw(cell);
    }
    else if (p.second == parentNode.cellCount() - 1) {
        CellId cellLeft = parentNode.cellId(parentNode.cellCount() - 2);
        result.left = parentNode.getCellLeftPtrRaw(cellLeft);
        result.keyLeft = parentNode.getCellDataRaw(cellLeft);
        result.recordLeft = parentNode.getCellRecordRaw(cellLeft);
        CellId cellRight = parentNode.cellId(parentNode.cellCount() - 1);
        result.right = parentNode.rightPtr();
        result.keyRight = parentNode.getCellDataRaw(cellRight);
        result.recordRight = parentNode.getCellRecordRaw(cellRight);
    }
    else {
        CellId cellLeft = parentNode.cellId(p.second - 1);
        result.left = parentNode.getCellLeftPtrRaw(cellLeft);
        result.keyLeft = parentNode.getCellDataRaw(cellLeft);
        result.recordLeft = parentNode.getCellRecordRaw(cellLeft);
        CellId cellRight = parentNode.cellId(p.second);
        result.right = parentNode.getCellLeftPtr(p.second + 1);
        result.keyRight = parentNode.getCellDataRaw(cellRight);
        result.recordRight = parentNode.getCellRecordRaw(cellRight);
    }
    return result;
};

void InternalNode::insertIntoChild(char* key, RecordId record) {
        pair<bool, SlotId> p = binarySearch(key, record);
        NodeId currentId = rightPtr();
        if (p.second != cellCount())
            currentId = getCellLeftPtr(p.second);
        LeafNode n(index, currentId, keySize);
        n.insertIntoNode(key, record);
        consistencyCheck();
        internalConsistencyCheck();
        n.consistencyCheck();
}

bool InternalNode::deleteFromNode(const char* key, RecordId record) {
        pair<bool, SlotId> p = binarySearch(key, record);
        if (p.first) {
            NodeId childId = p.second == cellCount() ? rightPtr() : getCellLeftPtr(p.second);
            Page childPage = index.retrieve(childId);
            NodeId rightmostId = childId;
            bool isChildEmpty;
            if (childPage[0] == 0x01) { // Internal
                InternalNode n(index, childId, keySize);
                rightmostId = n.findRightmostNode();
                isChildEmpty = n.cellCount() == 0;
            }
            else {
                LeafNode n(index, childId, keySize);
                isChildEmpty = n.cellCount() == 0;
            }

            CellId* deletionPlace = &cellId(p.second);
            if (!isChildEmpty) {
                LeafNode rightmost(index, rightmostId, keySize);
                CellId rightmostCell = rightmost.cellId(rightmost.cellCount() - 1);
                memcpy(getCellDataRaw(*deletionPlace), rightmost.getCellDataRaw(rightmostCell), keySize);
                getCellRecordRaw(*deletionPlace) = rightmost.getCellRecordRaw(rightmostCell);
                rightmost.deallocateCell(rightmostCell);
                rightmost.cellCount()--;
                index.update(rightmostId);
            }
            else {
                index.deallocatePage(childId);
                if (p.second != cellCount() - 1)
                    memmove(deletionPlace, deletionPlace + 1, (cellCount() - p.second - 1) * 2);
                deallocateCell(*deletionPlace);
                cellCount()--;
            }
            index.update(id);
            consistencyCheck();
            return true;
        }
        else {
            NodeId childId = p.second == cellCount() ? rightPtr() : getCellLeftPtr(p.second);
            Page childPage = index.retrieve(childId);
            if (page[0] == 0x01) { // Internal
                InternalNode n(index, childId, keySize);
                return n.deleteFromNode(key, record);
            }
            else { // Leaf
                LeafNode n(index, childId, keySize);
                return n.deleteFromNode(key, record);
            }
        }
    }