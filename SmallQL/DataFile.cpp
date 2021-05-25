#include "DataFile.h"

#include <iostream>

const uint64_t FRL_MASK = 0x00FFFFFFFFFFFFFFul;

#define DATAFILE_ID(tableId) ((tableId & 0xFFFF) << 16 | 0xFFFF)

DataFile::DataFile(TransactionManager& trMan, int tableId)
    : Pager(trMan, DATAFILE_ID(tableId)) {
    Page headerPage = retrieveWrite(0);
    if (memcmp(headerPage, "SmDD", 4) != 0) {
        cout << "ERROR! Table not found" << endl;
        exit(1);
    }
    initPointers(headerPage);
}

DataFile::DataFile(TransactionManager& trMan, int tableId, uint32_t recordSize)
    : Pager(trMan, DATAFILE_ID(tableId)) {
    Page headerPage = retrieveWrite(0);
    if (memcmp(headerPage, "SmDD", 4) != 0) {
        initFile(tableId, recordSize, headerPage);
    }
    initPointers(headerPage);
}

DataFile::DataFile(TransactionManager& trMan, const SystemInfoManager& sysMan, int tableId)
    : DataFile(trMan, tableId, 
        sysMan.getTableSchema(tableId).getSize()) {}

DataFile::DataFile(TransactionManager& trMan, const SystemInfoManager& sysMan, string tableName)
    : DataFile(trMan, sysMan.getTableId(tableName), 
        sysMan.getTableSchema(tableName).getSize()) {}

void DataFile::deleteFile(TransactionManager& trMan, int tableId) {
    trMan.deleteFile(DATAFILE_ID(tableId));
}

void DataFile::initPointers(Page headerPage) {
    recordSize = *(uint32_t*)(headerPage + 0x08);
    assert(recordSize > 0);
    trueRecordSize = recordSize + 1;
    if (trueRecordSize < 8) trueRecordSize = 8;
    activeRecordCount = (uint64_t*)(headerPage + 0x10);
    totalRecordCount = (uint64_t*)(headerPage + 0x18);
    frlStart = (RecordId*)(headerPage + 0x20);
    page0Data = headerPage + 0x40;

    recordsPerPage = PAGE_SIZE / trueRecordSize;
    recordsPerZeroPage = (PAGE_SIZE - 0x40) / trueRecordSize;
}

void DataFile::initFile(int tableId, uint32_t recordSize, Page headerPage) {
    memcpy(headerPage, "SmDD", 4);
    headerPage[0x04] = 0x01;
    headerPage[0x05] = 0x01;
    *(uint16_t*)(headerPage + 0x06) = tableId;
    *(uint32_t*)(headerPage + 0x08) = recordSize;
    *(uint64_t*)(headerPage + 0x10) = 0;
    *(uint64_t*)(headerPage + 0x18) = 0;
    *(RecordId*)(headerPage + 0x20) = NULL64;
    update(0);
}

char* DataFile::readRecordInternal(RecordId id, bool isRead) const {
    if (id < recordsPerZeroPage) {
        uint32_t offset = id * trueRecordSize;
        lastReadPage = 0;
        return page0Data + offset;
    }

    RecordId newId = id - recordsPerZeroPage;
    PageId pageId = newId / recordsPerPage + 1;
    uint32_t offset = (newId % recordsPerPage) * trueRecordSize;
    Page p = isRead? retrieveRead(pageId) : retrieveWrite(pageId);
    lastReadPage = pageId;
    return p + offset;
}

const char* DataFile::readRecord(RecordId id) const {
    char* record = readRecordInternal(id, true);
    if (record[0] != 0x01) return NULL;
    return record + 1;
}

bool DataFile::writeRecord(RecordId id, const char* data) {
    if (id >= *totalRecordCount)
        return false;
    char* record = readRecordInternal(id, false);
    if (record[0] != 0x01) return false;

    memcpy(record + 1, data, recordSize);
    update(lastReadPage);
    return true;
}

RecordId DataFile::addRecord(const char* data) {
    RecordId newId;
    if (*frlStart != NULL64) {
        newId = *frlStart;
        char* frl = readRecordInternal(newId, false);
        if (frl[0] != 0x00 && frl[0] != 0xFF) {
            cout << "File is in incorrect format!" << endl;
            return NULL64;
        }
        PageId frlPage = lastReadPage;

        memcpy(frlStart, frl, 8);
        *frlStart &= FRL_MASK;
        memcpy(frl + 1, data, recordSize);
        frl[0] = 0x01;
        (*activeRecordCount)++;
        update(0);
        update(frlPage);
    }
    else {
        newId = *totalRecordCount;
        (*totalRecordCount)++;
        (*activeRecordCount)++;
        update(0);
        char* record = readRecordInternal(newId, false);
        record[0] = 0x01;
        memcpy(record + 1, data, recordSize);
        update(lastReadPage);
    }
    return newId;
}

bool DataFile::deleteRecord(RecordId id) {
    char* record = readRecordInternal(id, false);
    if (record[0] != 0x01) return false;
    RecordId newFrl = *frlStart;
    memcpy(record, frlStart, 8);
    if (record[0] != 0xFF) record[0] = 0x00;
    *frlStart = id;
    (*activeRecordCount)--;
    update(0);
    update(lastReadPage);
    return true;
}