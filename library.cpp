#define SQL_NOUNICODEMAP
#include <windows.h>
#include <sql.h>
#include <iomanip>
#include <sqlext.h>
#include <iostream>
#include <string>
#include <sstream>
#include <fstream>
using namespace std;

SQLHENV env = NULL;
SQLHDBC dbc = NULL;
string currentUserRole;

// Utility function to show SQL errors
void showError(const char* fn, SQLHANDLE handle, SQLSMALLINT type) {
    SQLINTEGER i = 0, native;
    SQLCHAR state[7], text[256];
    SQLSMALLINT len;
    while (SQLGetDiagRec(type, handle, ++i, state, &native, text, sizeof(text), &len) == SQL_SUCCESS)
        cerr << fn << " error: " << state << " " << native << " " << text << "\n";
}

bool exists(const string& sql) {
    SQLHSTMT stmt;
    if (!SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt))) return false;
    if (!SQL_SUCCEEDED(SQLExecDirect(stmt, (SQLCHAR*)sql.c_str(), SQL_NTS))) {
        SQLFreeHandle(SQL_HANDLE_STMT, stmt); return false;
    }
    SQLRETURN r = SQLFetch(stmt);
    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    return (r == SQL_SUCCESS || r == SQL_SUCCESS_WITH_INFO);
}

bool execSQL(const string& sql) {
    SQLHSTMT stmt;
    if (!SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt))) return false;
    SQLRETURN ret = SQLExecDirect(stmt, (SQLCHAR*)sql.c_str(), SQL_NTS);
    if (!SQL_SUCCEEDED(ret)) showError("execSQL", stmt, SQL_HANDLE_STMT);
    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    return SQL_SUCCEEDED(ret);
}