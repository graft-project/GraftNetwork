#ifndef DATASTORAGE_H
#define DATASTORAGE_H

#include <string>

class DataStorage
{
public:
    DataStorage(const std::string &level) { m_level = level; }
    virtual ~DataStorage() {}

    std::string storageLevel() const { return m_level; }

    virtual std::string getData(const std::string &key,
                                const std::string &defaultData = std::string()) const = 0;
    virtual void storeData(const std::string &key, const std::string &data) = 0;
    virtual void deleteData(const std::string &key) = 0;
    virtual bool exists(const std::string &key) const = 0;

protected:
    std::string m_level;
};

#endif // DATASTORAGE_H
