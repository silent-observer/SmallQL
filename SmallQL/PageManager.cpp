#include "PageManager.h"
#include <fstream>
#include <iostream>

static const VirtualFileID DELETED_FILE_ID = 0xFFFFFFF0;

PageManager::PageManager(string filename, string metaFilename)
    : f(nullptr), pageCount(0), metaFilename(metaFilename), metadataUpdated(false) {
    fstream file(metaFilename, ios::binary | ios::out | ios::app);
    file.close();
    file.open(metaFilename, ios::binary | ios::in | ios::ate);
    streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<uint32_t> buffer(size / 4);
    if (!file.read((char*)buffer.data(), size)) {
        cout << "Can't open metadata file!" << endl;
        exit(1);
    }

    if (size > 2) {
        if (buffer[0] != 0x4D446D53) {
            cout << "Metadata file is in invalid format!" << endl;
            exit(1);
        }
        pageCount = buffer[1];

        for (int i = 0; i < pageCount; i++) {
            VirtualFileID id = buffer[i + 2];
            if (id == DELETED_FILE_ID) {
                deletedPages.insert(i);
            }
            else if (metadata.count(id) == 0) {
                metadata.emplace(id, PageMap(1, i));
            }
            else {
                metadata.at(id).push_back(i);
            }
        }
    }
    else {
        metadataUpdated = false;
    }


    int err = fopen_s(&f, filename.c_str(), "rb+");
    if (err) {
        fopen_s(&f, filename.c_str(), "wb");
        fclose(f);
        err = fopen_s(&f, filename.c_str(), "rb+");
        if (err) {
            cout << "Cannot open the Big File!" << endl;
            exit(1);
        }
    }
}

PageManager::~PageManager() {
    flushAll();
    fclose(f);
}

PageId PageManager::getFreePage(bool hasMinPage, PageId minPage) const {
    if (deletedPages.size() == 0) return pageCount++;
    auto iter = hasMinPage? deletedPages.upper_bound(minPage) : deletedPages.begin();
    PageId p = *iter;
    deletedPages.erase(iter);
    return p;
}

Page PageManager::retrieve(VirtualFileID fileId, PageId id) const {
    if (metadata.count(fileId) == 0) {
        metadata.emplace(fileId, PageMap());
    }
    auto& pageMap = metadata.at(fileId);
    while (pageMap.size() <= id) {
        PageId minPage = pageMap.empty() ? 0 : pageMap.back();
        pageMap.push_back(getFreePage(!pageMap.empty(), minPage));
        metadataUpdated = true;
    }
    PageId trueId = pageMap[id];

    auto i = cache.find(trueId);
    if (i != cache.end()) {
        return i->second.first;
    }

    size_t offset = trueId * PAGE_SIZE;
    fseek(f, offset, SEEK_SET);
    Page p = new char[PAGE_SIZE];

    bool dirty = false;
    if (!fread(p, PAGE_SIZE, 1, f)) {
        if (feof(f)) {
            memset(p, 0, PAGE_SIZE);
            dirty = true;
            dirtyQueue.push(trueId);
        }
        else {
            cout << "ERROR!" << endl;
            exit(1);
        }
    }
    cache[trueId] = make_pair(p, dirty);
    return p;
}

void PageManager::update(VirtualFileID fileId, PageId id) {
    PageId trueId = metadata[fileId][id];

    auto i = cache.find(trueId);
    bool newDirty = true;
    if (i == cache.end()) {
        cache[trueId] = make_pair(new char[PAGE_SIZE](), true);
    }
    else {
        if (i->second.second)
            newDirty = false;
        i->second.second = true;
    }

    if (newDirty)
        dirtyQueue.push(trueId);
}

bool PageManager::flushOne() {
    if (dirtyQueue.empty()) return false;
    PageId trueId = dirtyQueue.front();
    dirtyQueue.pop();

    fseek(f, trueId * PAGE_SIZE, SEEK_SET);
    fwrite(cache[trueId].first, 1, PAGE_SIZE, f);
    cache[trueId].second = false;
}

void PageManager::flushMetadata() {
    if (!metadataUpdated) return;

    fstream file(metaFilename, ios::binary | ios::in | ios::out | ios::ate);
    streamsize size = file.tellg();
    if (size < 2) {
        file.seekp(0, ios::beg);
        uint32_t val = 0x4D446D53;
        file.write((const char*)&val, 4);
        file.write((const char*)&pageCount, 4);
    }
    else {
        file.seekp(4, ios::beg);
        file.write((const char*)&pageCount, 4);
    }
    vector<uint32_t> buffer(pageCount);
    for (const auto& m : metadata) {
        for (PageId id : m.second)
                buffer[id] = m.first;
    }
    for (PageId id : deletedPages)
            buffer[id] = DELETED_FILE_ID;
    file.seekp(8, ios::beg);
    file.write((const char*)buffer.data(), 4 * buffer.size());
    metadataUpdated = false;
}

void PageManager::deleteFile(VirtualFileID fileId) {
    auto& pageMap = metadata.at(fileId);
    for (int i = 0; i < pageMap.size(); i++) {
        Page p = retrieve(fileId, i);
        memset(p, 0, PAGE_SIZE);
        update(fileId, i);
        deletedPages.insert(pageMap[i]);
    }
    metadata.erase(fileId);
    metadataUpdated = true;
}