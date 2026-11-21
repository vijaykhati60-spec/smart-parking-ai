#include <stdio.h>
#include <mysql.h>

int main(void) {
    MYSQL *conn = mysql_init(NULL);

    if (conn == NULL) {
        printf("MySQL initialization failed.\n");
        return 1;
    }

    if (mysql_real_connect(
            conn,
            "localhost",
            "parkease_app",
            "ParkEase_App_2026!",
            "parkease_db",
            3306,
            NULL,
            0) == NULL) {

        printf("Connection failed: %s\n", mysql_error(conn));
        mysql_close(conn);
        return 1;
    }

    printf("Connected to parkease_db successfully!\n");

    mysql_close(conn);
    return 0;
}