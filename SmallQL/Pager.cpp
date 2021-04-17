#include "Pager.h"

#include <iostream>

Pager::Pager(string filename) {
    int err = fopen_s(&f, filename.c_str(), "rb+");
    if (err) {
        fopen_s(&f, filename.c_str(), "wb");
        fclose(f);
        err = fopen_s(&f, filename.c_str(), "rb+");
        if (err) {
            cout << "ERROR!" << endl;
            exit(1);
        }
    }
}

Pager::~Pager() {
    flush();
    fclose(f);
}

Page Pager::retrieve(PageId id) const {
    auto i = cache.find(id);
    if (i != cache.end()) {
        return i->second.first;
    }

    size_t offset = id * PAGE_SIZE;
    fseek(f, offset, SEEK_SET);
    Page p = new char[PAGE_SIZE];

    bool dirty = false;
    if (!fread(p, PAGE_SIZE, 1, f)) {
        if (feof(f)) {
            memset(p, 0, PAGE_SIZE);
            dirty = true;
        }
        else {
            cout << "ERROR!" << endl;
            exit(1);
        }
    }
    cache[id] = make_pair(p, dirty);
    return p;
}

void Pager::update(PageId id) {
    auto i = cache.find(id);
    if (i == cache.end()) {
        cache[id] = make_pair(new char[PAGE_SIZE](), true);
    }
    else {
        i->second.second = true;
    }
}

void Pager::flush() {
    for (auto i = cache.begin(); i != cache.end(); i++) {
        if (!i->second.second) 
            continue;

        fseek(f, i->first * PAGE_SIZE, SEEK_SET);
        fwrite(i->second.first, 1, PAGE_SIZE, f);
        i->second.second = false;
    }
}