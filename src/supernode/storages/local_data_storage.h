#ifndef LOCAL_DATA_STORAGE_H
#define LOCAL_DATA_STORAGE_H

#include <map>

#include "data_storage.h"

class LocalDataStorage : public DataStorage
{
public:
    LocalDataStorage(const std::string &level);

    std::string getData(const std::string &key, const std::string &defaultData) const override;
    void storeData(const std::string &key, const std::string &data) override;
    void deleteData(const std::string &key) override;
    bool exists(const std::string &key) const override;

private:
    std::map<std::string, std::string> m_storage;
};

#endif // LOCAL_DATA_STORAGE_H
