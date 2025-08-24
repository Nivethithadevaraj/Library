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
bool login() {
    string username, password;
    cout << "\n=== Login ===\nUsername: ";
    getline(cin, username);
    cout << "Password: ";
    getline(cin, password);

    string sql = "SELECT Role FROM Users WHERE Username='" + username +
                 "' AND Password='" + password + "'";
    SQLHSTMT stmt;
    if (!SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt))) return false;
    if (!SQL_SUCCEEDED(SQLExecDirect(stmt, (SQLCHAR*)sql.c_str(), SQL_NTS))) {
        showError("Login", stmt, SQL_HANDLE_STMT);
        SQLFreeHandle(SQL_HANDLE_STMT, stmt); return false;
    }

    SQLCHAR role[20];
    if (SQLFetch(stmt) == SQL_SUCCESS) {
        SQLGetData(stmt, 1, SQL_C_CHAR, role, sizeof(role), NULL);
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        currentUserRole = (char*)role;
        cout << "Login successful. Role: " << currentUserRole << "\n";
        return true;
    } else {
        cout << "Invalid credentials.\n";
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return false;
    }
}

void addBook() {
    if (currentUserRole != "Admin") {
        cout << "Access denied. Only admin can perform this action.\n";
        return;
    }
    string title, author, genre, publisher, isbn, edition, rack, language;
    int year;
    double price;

    cout << "Title: "; getline(cin, title);
    cout << "Author: "; getline(cin, author);
    cout << "Genre: "; getline(cin, genre);
    cout << "Publisher: "; getline(cin, publisher);
    cout << "ISBN (unique): "; getline(cin, isbn);
    if (exists("SELECT * FROM Books WHERE ISBN='" + isbn + "'")) {
        cout << "ISBN already exists.\n"; return;
    }
    cout << "Edition: "; getline(cin, edition);
    cout << "Published Year: "; cin >> year; cin.ignore();
    cout << "Price: "; cin >> price; cin.ignore();
    cout << "Rack Location: "; getline(cin, rack);
    cout << "Language: "; getline(cin, language);

    string sql = "INSERT INTO Books (Title, Author, Genre, Publisher, ISBN, Edition, PublishedYear, Price, RackLocation, Language, Availability) VALUES ('" +
                 title + "','" + author + "','" + genre + "','" + publisher + "','" + isbn + "','" + edition + "'," +
                 to_string(year) + "," + to_string(price) + ",'" + rack + "','" + language + "', 1)";
    cout << (execSQL(sql) ? "Book added successfully.\n" : "Failed to add book.\n");
}

void updateBook() {
    if (currentUserRole != "Admin") { cout << "Access denied.\n"; return; }
    string isbn;
    cout << "Enter ISBN to update: "; getline(cin, isbn);
    if (!exists("SELECT * FROM Books WHERE ISBN='" + isbn + "'")) {
        cout << "Book not found.\n"; return;
    }
    string title, genre;
    cout << "New Title (leave blank to skip): "; getline(cin, title);
    cout << "New Genre (leave blank to skip): "; getline(cin, genre);

    string sql = "UPDATE Books SET ";
    if (!title.empty()) sql += "Title='" + title + "'";
    if (!genre.empty()) {
        if (!title.empty()) sql += ", ";
        sql += "Genre='" + genre + "'";
    }
    sql += " WHERE ISBN='" + isbn + "'";
    cout << (execSQL(sql) ? "Book updated.\n" : "Update failed.\n");
}

void deleteBook() {
    if (currentUserRole != "Admin") { cout << "Access denied.\n"; return; }
    string isbn;
    cout << "Enter ISBN to delete: "; getline(cin, isbn);
    if (!exists("SELECT * FROM Books WHERE ISBN='" + isbn + "'")) {
        cout << "Book not found.\n"; return;
    }
    string check = "SELECT * FROM Transactions WHERE BookID = (SELECT BookID FROM Books WHERE ISBN='" + isbn + "') AND ReturnDate IS NULL";
    if (exists(check)) {
        cout << "Book currently issued, cannot delete.\n"; return;
    }
    cout << (execSQL("DELETE FROM Books WHERE ISBN='" + isbn + "'") ? "Book deleted.\n" : "Delete failed.\n");
}

void viewBooks() {
    int pageSize = 5, page = 0;
    string choice;
    do {
        int offset = page * pageSize;
        string sql = "SELECT BookID, Title, ISBN, Availability FROM Books ORDER BY Title OFFSET " +
                     to_string(offset) + " ROWS FETCH NEXT " + to_string(pageSize) + " ROWS ONLY";
        SQLHSTMT stmt;
        SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt);
        if (SQLExecDirect(stmt, (SQLCHAR*)sql.c_str(), SQL_NTS) == SQL_SUCCESS) {
            cout << "\nPage " << page + 1 << "\nID\tTitle\tISBN\tAvailable\n";
            while (SQLFetch(stmt) == SQL_SUCCESS) {
                int id, avail;
                char title[255], isbn[20];
                SQLGetData(stmt, 1, SQL_C_SLONG, &id, 0, NULL);
                SQLGetData(stmt, 2, SQL_C_CHAR, title, sizeof(title), NULL);
                SQLGetData(stmt, 3, SQL_C_CHAR, isbn, sizeof(isbn), NULL);
                SQLGetData(stmt, 4, SQL_C_LONG, &avail, 0, NULL);
                cout << id << "\t" << title << "\t" << isbn << "\t" << (avail ? "Yes" : "No") << "\n";
            }
        }
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        cout << "[N]ext, [P]revious, [Q]uit: ";
        getline(cin, choice);
        if (choice == "N" || choice == "n") page++;
        else if ((choice == "P" || choice == "p") && page > 0) page--;
    } while (choice != "Q" && choice != "q");
}

void searchBooks() {
    cout << "Enter keyword to search in title: ";
    string keyword;
    getline(cin, keyword);
    string sql = "SELECT BookID, Title, ISBN FROM Books WHERE Title LIKE '%" + keyword + "%'";
    SQLHSTMT stmt;
    SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt);
    if (SQLExecDirect(stmt, (SQLCHAR*)sql.c_str(), SQL_NTS) == SQL_SUCCESS) {
        cout << "ID\tTitle\tISBN\n";
        while (SQLFetch(stmt) == SQL_SUCCESS) {
            int id;
            char title[255], isbn[20];
            SQLGetData(stmt, 1, SQL_C_SLONG, &id, 0, NULL);
            SQLGetData(stmt, 2, SQL_C_CHAR, title, sizeof(title), NULL);
            SQLGetData(stmt, 3, SQL_C_CHAR, isbn, sizeof(isbn), NULL);
            cout << id << "\t" << title << "\t" << isbn << "\n";
        }
    }
    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
}

void bulkImportBooks() {
    if (currentUserRole != "Admin") { cout << "Access denied.\n"; return; }
    cout << "Enter CSV file path: ";
    string file;
    getline(cin, file);
    ifstream in(file);
    if (!in) { cout << "File not found.\n"; return; }
    string line; int success = 0, fail = 0, lineNo = 0;
    while (getline(in, line)) {
        lineNo++;
        if (line.empty()) continue;
        stringstream ss(line);
        string title, author, genre, publisher, isbn, edition, year, price, rack, lang;
        if (!getline(ss, title, ',') || !getline(ss, author, ',') || !getline(ss, genre, ',') ||
            !getline(ss, publisher, ',') || !getline(ss, isbn, ',') || !getline(ss, edition, ',') ||
            !getline(ss, year, ',') || !getline(ss, price, ',') || !getline(ss, rack, ',') || !getline(ss, lang)) {
            cout << "Line " << lineNo << ": Incorrect format.\n"; fail++; continue;
        }
        string sql = "INSERT INTO Books (Title, Author, Genre, Publisher, ISBN, Edition, PublishedYear, Price, RackLocation, Language, Availability) VALUES ('" +
                     title + "','" + author + "','" + genre + "','" + publisher + "','" + isbn + "','" + edition + "'," +
                     year + "," + price + ",'" + rack + "','" + lang + "', 1)";
        if (execSQL(sql)) success++;
        else { cout << "Line " << lineNo << ": Failed to insert.\n"; fail++; }
    }
    cout << "Bulk import complete. Success: " << success << ", Failed: " << fail << "\n";
}
void addMember() {
    if (currentUserRole != "Admin") { cout << "Access denied.\n"; return; }
    string name, email, phone, address, membership;
    cout << "Name: "; getline(cin, name);
    cout << "Email: "; getline(cin, email);
    if (exists("SELECT * FROM Members WHERE Email='" + email + "'")) {
        cout << "Email already exists.\n"; return;
    }
    cout << "Phone: "; getline(cin, phone);
    cout << "Address: "; getline(cin, address);
    cout << "Membership Type (Regular/Premium): "; getline(cin, membership);
    string sql = "INSERT INTO Members (Name, Email, Phone, Address, MembershipType, JoinDate, Status) VALUES ('" +
                 name + "','" + email + "','" + phone + "','" + address + "','" + membership + "', GETDATE(), 'Active')";
    cout << (execSQL(sql) ? "Member added.\n" : "Failed to add member.\n");
}

void updateMember() {
    if (currentUserRole != "Admin") { cout << "Access denied.\n"; return; }
    cout << "Enter email of member to update: ";
    string email; getline(cin, email);
    if (!exists("SELECT * FROM Members WHERE Email='" + email + "'")) {
        cout << "Member not found.\n"; return;
    }
    cout << "New phone: "; string phone; getline(cin, phone);
    cout << "New address: "; string address; getline(cin, address);
    string sql = "UPDATE Members SET Phone='" + phone + "', Address='" + address + "' WHERE Email='" + email + "'";
    cout << (execSQL(sql) ? "Member updated.\n" : "Failed to update.\n");
}

void deleteMember() {
    if (currentUserRole != "Admin") { cout << "Access denied.\n"; return; }
    cout << "Enter email of member to delete: ";
    string email; getline(cin, email);
    if (!exists("SELECT * FROM Members WHERE Email='" + email + "'")) {
        cout << "Member not found.\n"; return;
    }
    string sql = "UPDATE Members SET Status='Inactive' WHERE Email='" + email + "'";
    cout << (execSQL(sql) ? "Member deactivated.\n" : "Failed to deactivate.\n");
}

void viewMembers() {
    SQLHSTMT stmt;
    SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt);
    string sql = "SELECT MemberID, Name, Email, Status FROM Members";
    if (SQLExecDirect(stmt, (SQLCHAR*)sql.c_str(), SQL_NTS) == SQL_SUCCESS) {
        cout << "\nID\tName\tEmail\tStatus\n";
        while (SQLFetch(stmt) == SQL_SUCCESS) {
            int id; char name[255], email[255], status[20];
            SQLGetData(stmt, 1, SQL_C_SLONG, &id, 0, NULL);
            SQLGetData(stmt, 2, SQL_C_CHAR, name, sizeof(name), NULL);
            SQLGetData(stmt, 3, SQL_C_CHAR, email, sizeof(email), NULL);
            SQLGetData(stmt, 4, SQL_C_CHAR, status, sizeof(status), NULL);
            cout << id << "\t" << name << "\t" << email << "\t" << status << "\n";
        }
    }
    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
}

void searchMembers() {
    cout << "Enter name/email to search: ";
    string keyword; getline(cin, keyword);
    string sql = "SELECT MemberID, Name, Email FROM Members WHERE Name LIKE '%" + keyword + "%' OR Email LIKE '%" + keyword + "%'";
    SQLHSTMT stmt;
    SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt);
    if (SQLExecDirect(stmt, (SQLCHAR*)sql.c_str(), SQL_NTS) == SQL_SUCCESS) {
        cout << "\nID\tName\tEmail\n";
        while (SQLFetch(stmt) == SQL_SUCCESS) {
            int id; char name[255], email[255];
            SQLGetData(stmt, 1, SQL_C_SLONG, &id, 0, NULL);
            SQLGetData(stmt, 2, SQL_C_CHAR, name, sizeof(name), NULL);
            SQLGetData(stmt, 3, SQL_C_CHAR, email, sizeof(email), NULL);
            cout << id << "\t" << name << "\t" << email << "\n";
        }
    }
    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
}
