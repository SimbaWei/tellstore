#pragma once

#include "PageManager.hpp"
#include <cstddef>
#include <atomic>
#include <assert.h>

namespace tell {
namespace store {

struct LogPage;

struct LogEntry {
private:
    std::atomic<uint32_t> mOffset;
public:
    uint32_t size;

    LogEntry(uint32_t offset, uint32_t size)
        : mOffset(offset + 1), size(size) {
    }

    char* data() {
        return reinterpret_cast<char*>(this) + sizeof(LogEntry);
    }

    const char* data() const {
        return reinterpret_cast<const char*>(this) + sizeof(LogEntry);
    }

    bool sealed() const {
        return mOffset.load() % 2 == 0;
    }

    uint32_t offset() const {
        auto offset = mOffset.load();
        return offset - (offset % 2);
    }

    void seal() {
        auto offset = mOffset.load();
        while (offset % 2 != 0) {
            mOffset.compare_exchange_strong(offset, offset - 1);
            offset = mOffset.load();
        }
    }

    std::pair<LogPage*, LogEntry*> nextP(LogPage* page);

    bool last() const;
    LogEntry* next();

    char* page() {
        return reinterpret_cast<char*>(this) - offset();
    }
};

/**
* A Log-Page has the following form:
*
* -------------------------------------------------------------------------------
* | next (8 bytes) | offset (4 bytes) | padding (4 bytes) | entry | entry | ... |
* -------------------------------------------------------------------------------
*
* This class, which only holds a pointer to the page, is mainly used
* for memory management.
*
* The last entry in the log is set to 0 (size and offset). We make
* use of the fact, that the PageManager only returns nulled pages.
*/
struct LogPage {
    static constexpr size_t LOG_HEADER_SIZE = 16;
    char* page;

    LogPage(char* page)
        : page(page) {
    }

    LogPage(const LogPage&) = delete;

    std::atomic<LogPage*>& next() {
        return *reinterpret_cast<std::atomic<LogPage*>*>(page);
    }

    std::atomic<uint32_t>& offset() {
        return *reinterpret_cast<std::atomic<uint32_t>*>(page + sizeof(LogPage*));
    }

    LogEntry* begin() {
        return reinterpret_cast<LogEntry*>(page);
    }

    constexpr static size_t DATA_OFFSET = 16;
};

class Log {
    constexpr static uint32_t MAX_SIZE = TELL_PAGE_SIZE - LogPage::DATA_OFFSET - sizeof(LogPage);
    PageManager& mPageManager;
    std::atomic<LogPage*> mHead;
    std::atomic<LogEntry*> mSealHead;
    std::pair<LogPage*, LogEntry*> mTail;
public:
    Log(PageManager& pageManager);

    LogEntry* append(uint32_t size);

    void seal(LogEntry* entry);

    LogEntry* tail();

    /**
    * Not thread safe
    */
    void setTail(LogEntry* nTail);
};

} // namespace store
} // namespace tell
