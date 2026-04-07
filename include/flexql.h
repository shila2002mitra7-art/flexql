#ifndef FLEXQL_H
#define FLEXQL_H

#ifdef __cplusplus
extern "C" {
#endif

#define FLEXQL_OK 0
#define FLEXQL_ERROR 1

// opaque structure
typedef struct FlexQL FlexQL;

// callback signature from assignment PDF
typedef int (*FlexQLCallback)(
    void *data,
    int columnCount,
    char **values,
    char **columnNames);

// required APIs
int flexql_open(const char *host, int port, FlexQL **db);

int flexql_close(FlexQL *db);

int flexql_exec(
    FlexQL *db,
    const char *sql,
    FlexQLCallback callback,
    void *arg,
    char **errmsg);

void flexql_free(void *ptr);

#ifdef __cplusplus
}
#endif

#endif