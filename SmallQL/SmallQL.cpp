﻿// SmallQL.cpp : Этот файл содержит функцию "main". Здесь начинается и заканчивается выполнение программы.
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
#include <thread>
#include <future>

int main()
{
    srand(1);

    TransactionManager trMan("Big.dbf", "meta.dbm");
    SystemInfoManager sysMan(trMan);
    sysMan.load();
    BlobManager blobManager(trMan);
    trMan.commit();

    //string blob1(100000, 'Y');
    //blobManager.addBlob(blob1.c_str(), blob1.size());
    //string blob2(20000, '2');
    //blobManager.addBlob(blob2.c_str(), blob2.size());
    //blobManager.deleteBlob(1);

    while (true) {
        cout << "> ";
        auto textFuture = async(launch::async, []() {
            stringstream ss;
            while (true) {
                string s;
                getline(cin, s);
                ss << s << " ";
                if (!s.empty() && s.back() == ';') break;
                cout << ". ";
            }
            cout << endl;
            return ss.str();
        });

        while (textFuture.wait_for(chrono::seconds(0)) != future_status::ready) {
            bool flushed = trMan.flushOne();
            if (!flushed) {
                trMan.flushMetadata();
                break;
            }
        }

        string text = textFuture.get();
        if (text.substr(0, 4) == "exit" || text.substr(0, 4) == "EXIT")
            break;
        try {
            Parser parser(text);
            auto parsed = parser.parse();
            //cout << parsed->prettyPrint();

            bool success = true;

            if (!tryDDL(parsed, trMan, sysMan)) {
                auto qtree = parsed->algebrize(sysMan);
                //cout << endl << "Before optimization:" << endl << endl;
                //print(qtree);
                optimize(qtree, sysMan);
                //cout << endl << "After optimization:" << endl << endl;
                //print(qtree);
                //cout << endl;
                Executor exec(trMan, sysMan, blobManager);
                exec.prepare(move(qtree));
                auto result = exec.execute();
                if (result.second.size() == 0)
                    cout << exec.message << endl;
                else
                    cout << prettyPrintTable(result.second, exec.resultType);
                success = result.first;
            }

            if (!success) {
                trMan.rollback();
                sysMan.autoCommitMode = true;
                cout << "Changes rolled back!" << endl;
            }
            else if (sysMan.autoCommitMode)
                trMan.commit();
        }
        catch (const SQLException& e) {
            cout << e.what() << endl;
        }
        cout << endl;
    }
}