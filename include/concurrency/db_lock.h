#ifndef DB_LOCK_H
#define DB_LOCK_H

#include <mutex>

class DBLock
{
public:
    std::mutex mtx;
};

#endif
