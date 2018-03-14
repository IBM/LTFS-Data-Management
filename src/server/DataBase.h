/*******************************************************************************
 * Copyright 2018 IBM Corp. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *  https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *******************************************************************************/
#pragma once

class DataBase
{
private:
    sqlite3 *db;
    bool dbNeedsClosed;
    static void fits(sqlite3_context *ctx, int argc, sqlite3_value **argv);
    static const std::string CREATE_JOB_QUEUE;
    static const std::string CREATE_REQUEST_QUEUE;
public:
    enum operation
    {
        MOUNT,     /**@< 0 */
        UNMOUNT,   /**@< 1 */
        TRARECALL, /**@< 2 */
        SELRECALL, /**@< 3 */
        MIGRATION, /**@< 4 */
        NOOP       /**@< 5 */
    };
    enum req_state
    {
        REQ_NEW, /**@< 0 */
        REQ_INPROGRESS, /**@< 1 */
        REQ_COMPLETED /**@< 2 */
    };
    static std::mutex trans_mutex;
    DataBase() :
            db(NULL), dbNeedsClosed(false)
    {
    }
    ~DataBase();
    void cleanup();
    void open(bool dbUseMemory);
    void createTables();
    int lastUpdates();
    sqlite3 *getDB()
    {
        return db;
    }
    static std::string opStr(operation op);
    static std::string reqStateStr(req_state reqs);
};

extern DataBase DB;

class SQLStatement
{
private:
    std::string fmtstr;
    sqlite3_stmt *stmt;
    boost::format fmt;
    int stmt_rc;

    std::string encode(std::string s);
    std::string decode(std::string s);
    void getColumn(int *result, int column);
    void getColumn(unsigned int *result, int column);
    void getColumn(DataBase::operation *result, int column);
    void getColumn(DataBase::req_state *result, int column);
    void getColumn(FsObj::file_state *result, int column);
    void getColumn(long *result, int column);
    void getColumn(unsigned long *result, int column);
    void getColumn(unsigned long long *result, int column);
    void getColumn(std::string *result, int column);

    void eval(int column)
    {
    }

    template<typename T>
    void eval(int column, T s)
    {
        getColumn(s, column);
    }

    template<typename T, typename ... Args>
    void eval(int column, T s, Args ... args)
    {
        getColumn(s, column);
        column++;
        eval(column, args ...);
    }

public:
    SQLStatement() :
            fmtstr(""), stmt(nullptr), fmt(""), stmt_rc(0)
    {
    }
    SQLStatement(std::string _fmtstr) :
            fmtstr(_fmtstr), stmt(nullptr), fmt(boost::format(fmtstr)), stmt_rc(
                    0)
    {
    }
    SQLStatement& operator()(std::string _fmtstr);
    ~SQLStatement()
    {
    }

    // convert unsigned to signed since there is unsigned in SQLite
    SQLStatement& operator<<(unsigned long long llu)
    {
        try {
            fmt % static_cast<long>(llu);
        } catch (const std::exception& e) {
            MSG(LTFSDMS0102E);
            THROW(Error::GENERAL_ERROR, e.what(), fmtstr);
        }
        return *this;
    }

    SQLStatement& operator<<(unsigned long lu)
    {
        try {
            fmt % static_cast<long>(lu);
        } catch (const std::exception& e) {
            MSG(LTFSDMS0102E);
            THROW(Error::GENERAL_ERROR, e.what(), fmtstr);
        }

        return *this;
    }

    SQLStatement& operator<<(std::string s)
    {
        try {
            fmt % encode(s);
        } catch (const std::exception& e) {
            MSG(LTFSDMS0102E);
            THROW(Error::GENERAL_ERROR, e.what(), fmtstr);
        }

        return *this;
    }

    SQLStatement& operator<<(unsigned int u)
    {
        try {
            fmt % static_cast<int>(u);
        } catch (const std::exception& e) {
            MSG(LTFSDMS0102E);
            THROW(Error::GENERAL_ERROR, e.what(), fmtstr);
        }

        return *this;
    }

    template<typename T>
    SQLStatement& operator<<(T s)
    {
        try {
            fmt % (s);
        } catch (const std::exception& e) {
            MSG(LTFSDMS0102E);
            THROW(Error::GENERAL_ERROR, e.what(), fmtstr);
        }

        return *this;
    }

    std::string str();
    void bind(int num, int value);
    void bind(int num, std::string value);
    void prepare();

    template<typename ... Args>
    bool step(Args ... args)
    {
        int column = 0;

        stmt_rc = sqlite3_step(stmt);

        if (stmt_rc != SQLITE_ROW)
            return false;

        eval(column, args ...);

        return true;
    }

    void finalize();
    void doall();
};
