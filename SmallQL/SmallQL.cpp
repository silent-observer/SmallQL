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
#include "PrettyTablePrinter.h"
#include "BlobManager.h"

#include <set>
#include <ctime>

int main()
{
    srand(1);

    BlobManager blobManager;
    for (int i = 0; i < 5000; i++) {
        /*auto p = blobManager.readBlob(i);
        if (p.second == nullptr)
            cout << i << " -> NULL" << endl;
        else {
            cout << i << " -> " << string(p.second, p.second + p.first) << endl;
        }*/
        if (i % 100 == 0)
            cout << i << endl;
        for (int j = 0; j < 10; j++) {
            string blob = to_string(i) + '_' + to_string(j) + string(rand() % 5, ' ');
            blobManager.addBlob(blob.c_str(), blob.size());
        }
        blobManager.deleteBlob(rand() % ((i + 1) * 9));
        blobManager.deleteBlob(rand() % ((i + 1) * 9));
        blobManager.deleteBlob(rand() % ((i + 1) * 9));
    }
    //string blob = "This is a very big string that is so big so that it won't fit in a small space.";
    //blobManager.addBlob(blob.c_str(), blob.size());
    //blob = "smstr.";
    //blobManager.addBlob(blob.c_str(), blob.size());
    //blob = "Small string.";
    //blobManager.addBlob(blob.c_str(), blob.size());
    //cout << id << endl;
    //blobManager.readBlob(0);
    //blobManager.readBlob(1);
    //blobManager.readBlob(2);
    //blobManager.deleteBlob(1);

    /*SystemInfoManager sysMan;
    sysMan.load();

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
        try {
            Parser parser(text);
            auto parsed = parser.parse();
            //cout << parsed->prettyPrint();

            if (!tryDDL(parsed, sysMan)) {
                auto qtree = parsed->algebrize(sysMan);
                //cout << endl << "Before optimization:" << endl << endl;
                //print(qtree);
                optimize(qtree, sysMan);
                //cout << endl << "After optimization:" << endl << endl;
                //print(qtree);
                //cout << endl;
                Executor exec(sysMan);
                exec.prepare(move(qtree));
                auto result = exec.execute();
                if (result.size() == 0)
                    cout << exec.message << endl;
                else
                    cout << prettyPrintTable(result, exec.resultType);
            }
        }
        catch (const SQLException& e) {
            cout << e.what() << endl;
        }
        cout << endl;
    }*/
}