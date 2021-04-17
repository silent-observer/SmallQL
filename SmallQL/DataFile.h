#pragma once
#include "Pager.h"
#include "Common.h"
#include "SystemInfoManager.h"
#include <cstdint>
#include <vector>

class DataFile : public Pager {
    uint32_t recordSize;
    uint32_t trueRecordSize;
    uint64_t* activeRecordCount;
    uint64_t* totalRecordCount;
    RecordId* frlStart;
    char* page0Data;

    uint32_t recordsPerPage;
    uint32_t recordsPerZeroPage;

    mutable PageId lastReadPage;

    void initFile(int tableId, uint32_t recordSize, Page headerPage);
    void initPointers(Page headerPage);
    char* readRecordInternal(RecordId id) const;
public:
    DataFile(int tableId);
    DataFile(int tableId, uint32_t recordSize);
    DataFile(const SystemInfoManager& sysMan, int tableId);
    DataFile(const SystemInfoManager& sysMan, string tableName);
    const char* readRecord(RecordId id) const;
    inline vector<char> readRecordVector(RecordId id) const {
        const char* record = readRecord(id);
        return vector<char>(record, record + recordSize);
    }
    bool writeRecord(RecordId id, const char* data);
    inline bool writeRecord(RecordId id, const vector<char>& data) {
        return writeRecord(id, data.data());
    };
    RecordId addRecord(const char* data);
    inline RecordId addRecord(const vector<char>& data) {
        return addRecord(data.data());
    }
    bool deleteRecord(RecordId id);
    inline uint32_t getRecordSize() const {
        return recordSize;
    }

    template <typename T>
    class CustomIterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = T;
        using difference_type = ptrdiff_t;
        using pointer = const value_type*;
        using reference = const value_type&;
    private:
        RecordId recordId;
        value_type value;
        const DataFile* dataFile;
        void updateValue() {
            if (recordId == *dataFile->totalRecordCount) return;
            value = const_cast<value_type>(dataFile->readRecord(recordId));
            assert(value != NULL);
        }
    public:
        CustomIterator(RecordId recordId, value_type value, const DataFile* dataFile)
            : recordId(recordId)
            , value(value)
            , dataFile(dataFile){}
        CustomIterator(RecordId recordId, const DataFile* dataFile)
            : recordId(recordId)
            , value(NULL)
            , dataFile(dataFile){
            if (*dataFile->totalRecordCount != 0)
                updateValue();
        }

        reference operator*() const { return value; }
        pointer operator->() { return &value; }

        // Prefix increment
        CustomIterator& operator++() {
            do {
                recordId++; 
            } while (recordId < *dataFile->totalRecordCount && dataFile->readRecord(recordId) == NULL);
            if (recordId < *dataFile->totalRecordCount)
                updateValue();
            return *this; 
        }  

        // Postfix increment
        CustomIterator operator++(int) { 
            const_iterator tmp = *this; 
            ++(*this); 
            return tmp; 
        }

        friend bool operator== (const CustomIterator& a, const CustomIterator& b) { 
            return a.recordId == b.recordId; 
        };
        friend bool operator!= (const CustomIterator& a, const CustomIterator& b) { 
            return a.recordId != b.recordId; 
        };
    };
    using const_iterator = CustomIterator<const char*>;
    using iterator = CustomIterator<char*>;

    iterator begin() {
        return iterator(0, this);
    }
    iterator end() {
        return iterator(*totalRecordCount, NULL, this);
    }
    const_iterator begin() const {
        return const_iterator(0, this);
    }
    const_iterator end() const {
        return const_iterator(*totalRecordCount, NULL, this);
    }
};