#include "PageManager.h"
#include <fstream>
#include <iostream>

PageManager::PageManager(string filename, string metaFilename)
    : f(nullptr), pageCount(0), metaFilename(metaFilename) {
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
            if (metadata.count(id) == 0) {
                metadata.emplace(id, PageMap(1, i));
            }
            else {
                metadata.at(id).push_back(i);
            }
        }
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

Page PageManager::retrieve(VirtualFileID fileId, PageId id) const {
    if (metadata.count(fileId) == 0)
        metadata.emplace(fileId, PageMap());
    auto& pageMap = metadata.at(fileId);
    while (pageMap.size() <= id) {
        pageMap.push_back(pageCount++);
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
    fstream file(metaFilename, ios::binary | ios::in | ios::out | ios::ate);
    streamsize size = file.tellg();
    uint32_t oldPageCount = 0;
    if (size < 2) {
        file.seekp(0, ios::beg);
        uint32_t val = 0x4D446D53;
        file.write((const char*)&val, 4);
        file.write((const char*)&pageCount, 4);
    }
    else {
        file.seekg(4, ios::beg);
        file.read((char*)&oldPageCount, 4);
        file.seekp(4, ios::beg);
        file.write((const char*)&pageCount, 4);
    }
    if (oldPageCount == pageCount) return;
    file.seekp(8 + 4 * oldPageCount, ios::beg);
    vector<uint32_t> buffer(pageCount - oldPageCount);
    for (const auto& m : metadata) {
        for (PageId id : m.second)
            if (id >= oldPageCount)
                buffer[id - oldPageCount] = m.first;
    }
    file.write((const char*)buffer.data(), 4 * buffer.size());
}