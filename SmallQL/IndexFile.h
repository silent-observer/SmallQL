#pragma once
#include "PageManager.h"
#include "DataType.h"
#include "Common.h"
#include "SystemInfoManager.h"
#include <memory>
#include <vector>
#include "DataFile.h"

using namespace std;

const int PARENTING_ZERO_SIZE = 2040;
const int PARENTING_NORMAL_SIZE = 2048;

class IndexFile : public Pager {
    uint16_t keySize;
    uint32_t* totalPageCount;
    NodeId* fplStart;
    NodeId* rootPageId;
    char* page0Data;
    
    uint16_t leafCellSize;
    uint16_t internalCellSize;
    int overflowMode;

    const Schema& keySchema;

    void initFile(int tableId, int indexId, uint16_t keySize, Page headerPage);
    void initPointers(Page headerPage);

    NodeId lastParentingPageId;
public:
    uint16_t cellsPerLeafPage;
    uint16_t cellsPerInternalPage;
    bool isUnique;

    IndexFile(PageManager& pageManager, int tableId, int indexId, const Schema& keySchema, bool isUnique);
    IndexFile(PageManager& pageManager, const SystemInfoManager& sysMan, string tableName, string indexName = "primary");
    NodeId allocateFreePage();
    void deallocatePage(NodeId id);
    void updateParent(NodeId id, NodeId parent);
    NodeId& nodeParent(NodeId id);
    void fullConsistencyCheck(NodeId id);
    inline void fullConsistencyCheck() {
        fullConsistencyCheck(*rootPageId);
    }

    bool addKey(const char* key, RecordId val);
    inline bool addKey(const vector<char>& key, RecordId val) {
        return addKey(key.data(), val);
    }
    RecordId findKey(const char* key);
    inline RecordId findKey(const vector<char>& key) {
        return findKey(key.data());
    }
    bool deleteKey(const char* key, RecordId val);
    inline bool deleteKey(const vector<char>& key, RecordId val) {
        return deleteKey(key.data(), val);
    }

    inline uint32_t getKeySize() {
        return keySize;
    }
    inline const Schema& getKeySchema() {
        return keySchema;
    }
    inline void setNewRoot(NodeId id) {
        *rootPageId = id;
        update(0);
    }

    class CustomIterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = RecordId;
        using difference_type = ptrdiff_t;
        using pointer = const value_type*;
        using reference = const value_type&;
    private:
        RecordId recordId;
        vector<pair<NodeId, SlotId>> ptrStack;
        const IndexFile* indexFile;
        void updateValue();
        void advance();
    public:
        CustomIterator(RecordId recordId, vector<pair<NodeId, SlotId>> ptrStack, const IndexFile* indexFile)
            : recordId(recordId)
            , ptrStack(ptrStack)
            , indexFile(indexFile){}
        CustomIterator(vector<pair<NodeId, SlotId>> ptrStack, const IndexFile* indexFile)
            : recordId(NULL64)
            , ptrStack(ptrStack)
            , indexFile(indexFile){
            updateValue();
        }

        reference operator*() const { return recordId; }
        pointer operator->() { return &recordId; }

        // Prefix increment
        inline CustomIterator& operator++() {
            advance();
            return *this;
        }

        // Postfix increment
        inline CustomIterator operator++(int) { 
            CustomIterator tmp = *this; 
            ++(*this); 
            return tmp; 
        }

        friend inline bool operator== (const CustomIterator& a, const CustomIterator& b) { 
            return a.ptrStack == b.ptrStack; 
        };
        friend inline bool operator!= (const CustomIterator& a, const CustomIterator& b) { 
            return a.ptrStack != b.ptrStack; 
        };
    };
    using const_iterator = CustomIterator;

    const_iterator begin() const {
        return const_iterator(vector<pair<NodeId, SlotId>>{make_pair(*rootPageId, 0)}, this);
    }
    const_iterator end() const {
        return const_iterator(NULL64, vector<pair<NodeId, SlotId>>{}, this);
    }
    const_iterator getIterator(const char* key);
    const_iterator getIterator(ValueArray key);
};