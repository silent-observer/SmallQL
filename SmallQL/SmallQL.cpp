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

    PageManager pageManager("Big.dbf", "meta.dbm");
    SystemInfoManager sysMan(pageManager);
    sysMan.load();
    BlobManager blobManager(pageManager);

    //string blob1(100000, 'Y');
    //blobManager.addBlob(blob1.c_str(), blob1.size());
    //string blob2(20000, '2');
    //blobManager.addBlob(blob2.c_str(), blob2.size());
    //blobManager.deleteBlob(1);

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
                Executor exec(pageManager, sysMan, blobManager);
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
    }
}