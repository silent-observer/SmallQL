// SmallQL.cpp : Этот файл содержит функцию "main". Здесь начинается и заканчивается выполнение программы.
//

#include <iostream>
#include <cstdio>
#include "DataFile.h"
#include "IndexFile.h"
#include "DataType.h"
#include "SystemInfoManager.h"
#include "SqlParser.h"
#include "Executor.h"
#include "Optimizer.h"
#include "DDLExecutor.h"

#include <set>
#include <ctime>

const Schema mainSchema(
    vector<SchemaEntry>{
        schemaEntryPrimary("id", make_shared<IntType>()),
        schemaEntryNormal("number", make_shared<IntType>()),
        schemaEntryNormal("name", make_shared<VarCharType>(20))
    }
);

const Schema keySchema = {
    vector<SchemaEntry>{
        schemaEntryPrimary("id", make_shared<IntType>())
    }
};

uint32_t betterRand() {
    return rand() * RAND_MAX + rand();
}

int main()
{
    srand(0);
    /*const uint32_t recordSize = mainSchema.getSize();
    char* buf = new char[recordSize];
    DataFile dataFile(0, recordSize);
    
    mainSchema.encode(vector<Value>{
        Value(1), Value(15), Value("Hello")
    }, buf);
    dataFile.addRecord(buf);

    mainSchema.encode(vector<Value>{
        Value(2), Value(17), Value("World")
    }, buf);
    RecordId id = dataFile.addRecord(buf);

    mainSchema.encode(vector<Value>{
        Value(3), Value(28), Value("Testing")
    }, buf);
    dataFile.addRecord(buf);

    cout << mainSchema.decode(dataFile.readRecord(id)) << endl;*/

    /*const uint32_t keySize = keySchema.getSize();
    char* buf = new char[keySize];
    IndexFile index(0, 3, make_shared<Schema>(keySchema));

    map<uint32_t, set<uint64_t>> tests;
    for (int i = 0; i < 1000000; i++) {
        int key = betterRand() % 100000000;
        uint64_t record = betterRand() % 100000000;
        keySchema.encode(vector<Value>{Value(key)}, buf);
        //index.addKey(buf, record);
        if (tests.count(key) == 0)
            tests.insert(make_pair(key, set<RecordId>()));
        tests[key].insert(record);

        //index.fullConsistencyCheck();
    }
    for (int i = 0; i < 1000; i++) {
        //cout << i << endl;
        int key = betterRand() % 100000000;
        uint64_t record = betterRand() % 100000000;
        keySchema.encode(vector<Value>{Value(key)}, buf);
        //index.addKey(buf, record);
        if (tests.count(key) == 0)
            tests.insert(make_pair(key, set<RecordId>()));
        tests[key].insert(record);

        //index.fullConsistencyCheck();
    }

    for (auto i = tests.begin(); i != tests.end(); i++) {
        keySchema.encode(vector<Value>{Value(i->first)}, buf);
        RecordId record = index.findKey(buf);
        if (i->second.count(record) == 0) {
            cout << i->first << endl;
            return 0;
        }
    }
    cout << "Success!";*/

    //keySchema.encode(vector<Value>{Value(key)}, buf);
    //cout << index.findKey(buf);

    SystemInfoManager sysMan;
    sysMan.load();
    /*uint16_t tableId = sysMan.addTable("test_table");
    sysMan.addColumn(tableId, "id", "INTEGER", true, false);
    sysMan.addColumn(tableId, "name", "VARCHAR(20)", false, false);
    sysMan.addColumn(tableId, "some_number", "INTEGER", false, false);
    sysMan.createPrimaryIndex(tableId);*/

    /*DataFile dataFile(sysMan, "test_table");
    IndexFile indexFile(sysMan, "test_table");

    ValueArray data = {
        Value(1), Value("Some random name"), Value(42)
    };
    auto encoded = sysMan.getTableSchema("test_table").encode(data);
    RecordId record = dataFile.addRecord(encoded);
    ValueArray keyData = sysMan.getIndexSchema("test_table").narrow(data);
    encoded = sysMan.getIndexSchema("test_table").encode(keyData);
    indexFile.addKey(encoded, record);
    

    data = {
        Value(2), Value("Some other name"), Value(10)
    };
    encoded = sysMan.getTableSchema("test_table").encode(data);
    record = dataFile.addRecord(encoded);
    keyData = sysMan.getIndexSchema("test_table").narrow(data);
    encoded = sysMan.getIndexSchema("test_table").encode(keyData);
    indexFile.addKey(encoded, record);

    data = {
        Value(3), Value("Test"), Value(100)
    };
    encoded = sysMan.getTableSchema("test_table").encode(data);
    record = dataFile.addRecord(encoded);
    keyData = sysMan.getIndexSchema("test_table").narrow(data);
    encoded = sysMan.getIndexSchema("test_table").encode(keyData);
    indexFile.addKey(encoded, record);*/

    /*ValueArray keyData = {Value(2)};
    auto encoded = sysMan.getIndexSchema("test_table").encode(keyData);
    RecordId record = indexFile.findKey(encoded);
    auto recordData = dataFile.readRecordVector(record);
    ValueArray decoded = sysMan.getTableSchema("test_table").decode(recordData);
    cout << decoded << endl;*/

    while (true) {
        cout << "> ";
        stringstream ss;
        while (true) {
            string s;
            getline(cin, s);
            ss << s << " ";
            if (!s.empty() && s.back() == ';') break;
            cout << ". ";
        }
        cout << endl;
        string text = ss.str();
        if (text.substr(0, 4) == "exit" || text.substr(0, 4) == "EXIT")
            break;
        /*string text = "INSERT INTO test_table VALUES "
            "(1, \"Hello\", 1543), "
            "(2, \"World\", 1543), "
            "(3, \"Testing\", 53), "
            "(4, \"Hello\", 6543), "
            "(5, \"Random string\", 6);";*/
        try {
            Parser parser(text);
            auto parsed = parser.parse();
            cout << parsed->prettyPrint();

            if (!tryDDL(parsed, sysMan)) {
                auto qtree = parsed->algebrize(sysMan);
                cout << endl << "Before optimization:" << endl << endl;
                print(qtree);
                optimize(qtree, sysMan);
                cout << endl << "After optimization:" << endl << endl;
                print(qtree);
                cout << endl;
                Executor exec(sysMan);
                exec.prepare(move(qtree));
                auto result = exec.execute();
                if (result.size() == 0)
                    cout << "<empty set>" << endl;
                else
                    for (const auto& row : result) {
                        cout << row << endl;
                    }
            }
        }
        catch (const SQLException& e) {
            cout << e.what() << endl;
        }
        cout << endl;
    }
}