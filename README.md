# FlexQL

FlexQL is a simplified SQL-like client-server database system written in C++. It supports a small subset of relational database functionality required by the assignment, including table creation, row insertion, selection, filtering, joins, caching, indexing, and multithreaded request handling.

## Features

- Client-server architecture over TCP sockets
- Interactive client terminal (REPL)
- C/C++ client API: `flexql_open`, `flexql_close`, `flexql_exec`, `flexql_free`
- Supported SQL-like commands:
  - `CREATE TABLE`
  - `INSERT INTO`
  - `SELECT *`
  - `SELECT column1, column2`
  - `WHERE` with a single condition
  - `INNER JOIN`
- Primary key indexing on the first column
- LRU-style query caching
- Expiration timestamp handling for rows
- Multithreaded server with shared database state
- Benchmark program for insertion throughput and SQL subset validation

## Requirements

This project was developed on Windows and uses Winsock.

Required tools:

- Windows OS
- `g++` with C++17 support
- Winsock library (`ws2_32`, available on Windows)

One working setup is:

- MSYS2 / MinGW-w64 UCRT64 
- Compiler path:
  `C:/msys64/ucrt64/bin/g++.exe`

No external database libraries are required.

## Build Instructions

Run these commands from `C:\flexql`.

Build the server:

```powershell
C:/msys64/ucrt64/bin/g++.exe -std=c++17 src/server/server.cpp src/parser/parser.cpp src/storage/storage.cpp src/cache/lru_cache.cpp src/index/primary_index.cpp src/concurrency/db_lock.cpp -Iinclude -Iinclude/cache -Iinclude/index -Iinclude/concurrency -o server.exe -lws2_32
```

Build the interactive client:

```powershell
C:/msys64/ucrt64/bin/g++.exe -std=c++17 src/client/client.cpp -o client.exe -lws2_32
```

Build the benchmark:

```powershell
C:/msys64/ucrt64/bin/g++.exe -std=c++17 src/client/benchmark_flexql.cpp src/client/flexql.cpp -Iinclude -o benchmark.exe -lws2_32
```

## How To Run

Step 1. Start the server:

```powershell
.\server.exe
```

Step 2. In another terminal, start the client:

```powershell
.\client.exe
```

Step 3. Enter SQL-like commands ending with `;`

Example:

CREATE TABLE USERS(ID DECIMAL, NAME VARCHAR(64), BALANCE DECIMAL, EXPIRES_AT DECIMAL);
INSERT INTO USERS VALUES (1, 'Alice', 1200, 1893456000);
INSERT INTO USERS VALUES (2, 'Bob', 450, 1893456000);
INSERT INTO USERS VALUES (3, 'Carol', 2200, 1893456000);
SELECT * FROM USERS;
SELECT NAME, BALANCE FROM USERS;
SELECT NAME FROM USERS WHERE BALANCE > 1000;
CREATE TABLE ORDERS(ORDER_ID DECIMAL, USER_ID DECIMAL, AMOUNT DECIMAL, EXPIRES_AT DECIMAL);
INSERT INTO ORDERS VALUES (101, 1, 50, 1893456000);
INSERT INTO ORDERS VALUES (102, 2, 150, 1893456000);
INSERT INTO ORDERS VALUES (103, 1, 300, 1893456000);
SELECT USERS.NAME, ORDERS.AMOUNT FROM USERS INNER JOIN ORDERS ON USERS.ID = ORDERS.USER_ID;
SELECT USERS.NAME, ORDERS.AMOUNT FROM USERS INNER JOIN ORDERS ON USERS.ID = ORDERS.USER_ID WHERE ORDERS.AMOUNT > 100;
SELECT UNKNOWN_COLUMN FROM USERS;
SELECT * FROM MISSING_TABLE;
exit;


## Benchmark Usage

Run the benchmark after the server is already running.

Default benchmark:

```powershell
.\benchmark.exe
```

Unit-test-only mode:

```powershell
.\benchmark.exe --unit-test
```

Run with a custom insert count:

```powershell
.\benchmark.exe 100000
```

Note:

- The default row count in the benchmark source is currently small for quick validation.
- The assignment mentions testing on very large datasets such as 10 million rows.
- This implementation stores data in memory, so very large runs may consume significant RAM.


## Limitations

- Only a small SQL-like subset is supported.
- Only one `WHERE` condition is supported.
- In this implementation, the first column of each table is treated as the primary key and is used for primary indexing.
- Rows are stored in memory, not on disk.
- Expired rows are skipped during query processing.
- Query results are displayed in the client terminal, while the server handles execution internally.

## Current Status

- Interactive client works
- Benchmark works
- Required SQL subset works
- Unit tests in the benchmark pass

