#ifndef INCLUDED_HASH_TABLE_H
#define INCLUDED_HASH_TABLE_H

#include "unity/array.h"

template<class Key, class Value, class Base> class HashTableBase : public Base
{
public:
    HashTableBase() : _n(0)
    {
        _table.allocate(1);
    }
    bool hasKey(const Key& key)
    {
        return _table[row(key)].hasKey(key);
    }
    Value operator[](const Key& key) const
    {
        return _table[row(key)].value(key);
    }
    void add(const Key& key, const Value& value)
    {
        if (_n == _table.count()) {
            Array<TableEntry> table;
            table.allocate(_table.count() * 2);
            table.swap(_table);
            _n = 0;
            for (int i = 0; i < table.count(); ++i)
                table[i].addAllTo(this);
        }
        _table[row(key)].add(key, value);
        ++_n;
    }
    int count() const { return _n; }
private:
    int row(const Key& key) const { return hash(key) & (_table.count() - 1); }

    class TableEntry
    {
    public:
        TableEntry() : _next(0) { }
        ~TableEntry()
        {
            while (_next != 0 && _next != this) {
                TableEntry* t = _next->_next;
                _next->_next = 0;
                delete _next;
                _next = t;
            }
        }
        bool hasKey(const Key& key)
        {
            if (_next == 0)
                return false;
            TableEntry* t = this;
            do {
                if (t->_key == key)
                    return true;
                t = t->_next;
            } while (t != this);
            return false;
        }
        Value value(const Key& key) const
        {
            if (_next == 0)
                return Value();
            const TableEntry* t = this;
            do {
                if (t->_key == key)
                    return t->_value;
                t = t->_next;
            } while (t != this);
            return Value();
        }
        void add(const Key& key, const Value& value)
        {
            if (_next == 0) {
                _key = key;
                _value = value;
                _next = this;
                return;
            }
            TableEntry* t = this;
            while (t->_next != this)
                t = t->_next;
            t->_next = new TableEntry();
            t->_next->_key = key;
            t->_next->_value = value;
            t->_next->_next = this;
        }
        void addAllTo(HashTableBase* table)
        {
            if (_next == 0)
                return;
            TableEntry* t = this;
            do {
                table->add(t->_key, t->_value);
                t = t->_next;
            } while (t != this);
        }
    private:
        Key _key;
        Value _value;
        TableEntry* _next;
    };
    Array<TableEntry> _table;
    int _n;
};

template<class Key, class Value> class HashTableRow : Uncopyable
{
protected:
    int hash(const Key& key) const { return key.hash(); }
};

template<class Value> class HashTableRow<int, Value> : Uncopyable
{
protected:
    int hash(int key) const { return key; }
};

template<class Key, class Value> class HashTable : public HashTableBase<Key, Value, HashTableRow<Key, Value> >
{
};

#endif // INCLUDED_HASH_TABLE_H
