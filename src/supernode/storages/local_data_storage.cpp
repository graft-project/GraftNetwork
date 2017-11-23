#include "local_data_storage.h"

LocalDataStorage::LocalDataStorage(const std::string &level)
    : DataStorage(level)
{

}

std::string LocalDataStorage::getData(const std::string &key, const std::string &defaultData) const
{
    if (exists(key))
    {
        return m_storage.at(key);
    }
    else
    {
        return defaultData;
    }
}

void LocalDataStorage::storeData(const std::string &key, const std::string &data)
{
    m_storage[key] = data;
}

void LocalDataStorage::deleteData(const std::string &key)
{
    m_storage.erase(key);
}

bool LocalDataStorage::exists(const std::string &key) const
{
    return (m_storage.find(key) != m_storage.end());
}
