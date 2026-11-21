
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <mysql.h>

#define SIZE 5
#define MAX_WAIT 10
#define REG 20
#define RATE 20

/* MySQL connection settings */
#define DB_HOST "localhost"
#define DB_USER "parkease_app"
#define DB_PASS "ParkEase_App_2026!"
#define DB_NAME "parkease_db"

/* Stack: parked vehicles */
char parking[SIZE][REG];
int top = -1;

/* Circular queue: waiting vehicles */
char waiting[MAX_WAIT][REG];
int front = 0;
int rear = -1;
int count = 0;

MYSQL *conn;

/* ---------- JSON RESPONSES ---------- */

void json_success(const char *message)
{
    printf("{\"success\":true,\"message\":\"%s\"}\n", message);
}

void json_error(const char *message)
{
    printf("{\"success\":false,\"message\":\"%s\"}\n", message);
}

/* ---------- DATABASE ---------- */

int connect_db(void)
{
    conn = mysql_init(NULL);

    if (conn == NULL)
        return 0;

    if (mysql_real_connect(
            conn,
            DB_HOST,
            DB_USER,
            DB_PASS,
            DB_NAME,
            3306,
            NULL,
            0) == NULL)
    {
        mysql_close(conn);
        conn = NULL;
        return 0;
    }

    return 1;
}

int db_query(const char *sql)
{
    return mysql_query(conn, sql) == 0;
}

/* Accept letters and numbers only */
int valid_vehicle(const char *vehicle)
{
    int i;

    if (vehicle == NULL || vehicle[0] == '\0')
        return 0;

    if (strlen(vehicle) >= REG)
        return 0;

    for (i = 0; vehicle[i] != '\0'; i++)
    {
        if (!isalnum((unsigned char)vehicle[i]))
            return 0;
    }

    return 1;
}

/* ---------- STACK OPERATIONS ---------- */

int push_vehicle(const char *vehicle)
{
    if (top >= SIZE - 1)
        return 0;

    top++;
    strcpy(parking[top], vehicle);
    return 1;
}

int pop_vehicle(char *vehicle)
{
    if (top < 0)
        return 0;

    strcpy(vehicle, parking[top]);
    top--;
    return 1;
}

/* ---------- CIRCULAR QUEUE OPERATIONS ---------- */

int enqueue_vehicle(const char *vehicle)
{
    if (count >= MAX_WAIT)
        return 0;

    rear = (rear + 1) % MAX_WAIT;
    strcpy(waiting[rear], vehicle);
    count++;

    return 1;
}

int dequeue_vehicle(char *vehicle)
{
    if (count == 0)
        return 0;

    strcpy(vehicle, waiting[front]);
    front = (front + 1) % MAX_WAIT;
    count--;

    return 1;
}

/* ---------- LOAD CURRENT DATABASE STATE ---------- */

int load_state(void)
{
    MYSQL_RES *res;
    MYSQL_ROW row;

    top = -1;
    front = 0;
    rear = -1;
    count = 0;

    /* Oldest parked vehicle first; newest becomes stack top */
    if (!db_query(
            "SELECT vehicle_number FROM vehicles "
            "WHERE status='Parked' "
            "ORDER BY entry_time ASC, vehicle_id ASC"))
    {
        return 0;
    }

    res = mysql_store_result(conn);

    if (res == NULL)
        return 0;

    while ((row = mysql_fetch_row(res)) != NULL)
    {
        if (top >= SIZE - 1)
        {
            mysql_free_result(res);
            return 0;
        }

        if (row[0] == NULL)
        {
            mysql_free_result(res);
            return 0;
        }

        push_vehicle(row[0]);
    }

    mysql_free_result(res);

    /* Oldest waiting vehicle enters the queue first */
    if (!db_query(
            "SELECT vehicle_number FROM waiting_queue "
            "ORDER BY queue_id ASC"))
    {
        return 0;
    }

    res = mysql_store_result(conn);

    if (res == NULL)
        return 0;

    while ((row = mysql_fetch_row(res)) != NULL)
    {
        if (count >= MAX_WAIT)
        {
            mysql_free_result(res);
            return 0;
        }

        if (row[0] == NULL)
        {
            mysql_free_result(res);
            return 0;
        }

        enqueue_vehicle(row[0]);
    }

    mysql_free_result(res);
    return 1;
}

/* ---------- FIND AN AVAILABLE PARKING SLOT ---------- */

int get_available_slot(int *slot_id)
{
    MYSQL_RES *res;
    MYSQL_ROW row;

    if (!db_query(
            "SELECT slot_id FROM parking_slots "
            "WHERE status='Available' "
            "ORDER BY slot_id ASC LIMIT 1"))
    {
        return 0;
    }

    res = mysql_store_result(conn);

    if (res == NULL)
        return 0;

    row = mysql_fetch_row(res);

    if (row == NULL || row[0] == NULL)
    {
        mysql_free_result(res);
        return 0;
    }

    *slot_id = atoi(row[0]);
    mysql_free_result(res);

    return 1;
}

/* ---------- CHECK DUPLICATE ACTIVE VEHICLES ---------- */

int vehicle_is_active(const char *vehicle)
{
    MYSQL_RES *res;
    char sql[512];
    int found;

    snprintf(
        sql, sizeof(sql),
        "SELECT vehicle_id FROM vehicles "
        "WHERE vehicle_number='%s' "
        "AND status IN ('Parked','Waiting') LIMIT 1",
        vehicle);

    if (!db_query(sql))
        return -1;

    res = mysql_store_result(conn);

    if (res == NULL)
        return -1;

    found = mysql_num_rows(res) > 0;
    mysql_free_result(res);

    return found;
}

/* ---------- PARK A VEHICLE ---------- */

void park_vehicle(const char *vehicle)
{
    int slot_id;
    int active;
    char sql[512];

    if (!valid_vehicle(vehicle))
    {
        json_error("Enter a valid vehicle number using letters and numbers only.");
        return;
    }

    if (!load_state())
    {
        json_error("Could not load parking data from the database.");
        return;
    }

    active = vehicle_is_active(vehicle);

    if (active == -1)
    {
        json_error("Could not check the vehicle in the database.");
        return;
    }

    if (active)
    {
        json_error("This vehicle is already parked or waiting.");
        return;
    }

    /* A free slot is available: push onto the stack */
    if (top < SIZE - 1)
    {
        if (!get_available_slot(&slot_id))
        {
            json_error("No available parking slot was found.");
            return;
        }

        if (!db_query("START TRANSACTION"))
        {
            json_error("Could not start the database transaction.");
            return;
        }

        snprintf(
            sql, sizeof(sql),
            "INSERT INTO vehicles "
            "(vehicle_number, slot_id, status, entry_time, fee) "
            "VALUES ('%s', %d, 'Parked', NOW(), 0)",
            vehicle, slot_id);

        if (!db_query(sql))
        {
            db_query("ROLLBACK");
            json_error("Could not save the parked vehicle.");
            return;
        }

        snprintf(
            sql, sizeof(sql),
            "UPDATE parking_slots SET status='Occupied' "
            "WHERE slot_id=%d AND status='Available'",
            slot_id);

        if (!db_query(sql) || mysql_affected_rows(conn) != 1)
        {
            db_query("ROLLBACK");
            json_error("Could not update the parking slot.");
            return;
        }

        if (!db_query("COMMIT"))
        {
            db_query("ROLLBACK");
            json_error("Could not complete the parking operation.");
            return;
        }

        push_vehicle(vehicle);
        json_success("Vehicle parked successfully.");
        return;
    }

    /* Parking is full: enqueue the vehicle */
    if (count >= MAX_WAIT)
    {
        json_error("Parking and waiting queue are both full.");
        return;
    }

    if (!db_query("START TRANSACTION"))
    {
        json_error("Could not start the database transaction.");
        return;
    }

    snprintf(
        sql, sizeof(sql),
        "INSERT INTO waiting_queue (vehicle_number) VALUES ('%s')",
        vehicle);

    if (!db_query(sql))
    {
        db_query("ROLLBACK");
        json_error("Could not add the vehicle to the waiting queue.");
        return;
    }

    snprintf(
        sql, sizeof(sql),
        "INSERT INTO vehicles "
        "(vehicle_number, slot_id, status, entry_time, fee) "
        "VALUES ('%s', NULL, 'Waiting', NOW(), 0)",
        vehicle);

    if (!db_query(sql))
    {
        db_query("ROLLBACK");
        json_error("Could not save the waiting vehicle.");
        return;
    }

    if (!db_query("COMMIT"))
    {
        db_query("ROLLBACK");
        json_error("Could not complete the queue operation.");
        return;
    }

    enqueue_vehicle(vehicle);
    json_success("Parking is full. Vehicle added to the waiting queue.");
}

/* ---------- LEAVE PARKING ---------- */

void leave_vehicle(const char *vehicle)
{
    char sql[1024];
    char top_vehicle[REG];
    char waiting_vehicle[REG];
    int slot_id;
    int fee;
    MYSQL_RES *res;
    MYSQL_ROW row;

    if (!valid_vehicle(vehicle))
    {
        json_error("Enter a valid vehicle number using letters and numbers only.");
        return;
    }

    if (!load_state())
    {
        json_error("Could not load parking data from the database.");
        return;
    }

    if (top < 0)
    {
        json_error("No vehicles are currently parked.");
        return;
    }

    /* Stack LIFO: only the top vehicle can leave */
    if (strcmp(parking[top], vehicle) != 0)
    {
        snprintf(
            sql, sizeof(sql),
            "Only the top vehicle (%s) can leave under the Stack LIFO rule.",
            parking[top]);

        json_error(sql);
        return;
    }

    snprintf(
        sql, sizeof(sql),
        "SELECT slot_id, "
        "GREATEST(1, CEIL(TIMESTAMPDIFF(MINUTE, entry_time, NOW()) / 60)) "
        "* %d AS calculated_fee "
        "FROM vehicles "
        "WHERE vehicle_number='%s' AND status='Parked' "
        "ORDER BY entry_time DESC, vehicle_id DESC LIMIT 1",
        RATE, vehicle);

    if (!db_query(sql))
    {
        json_error("Could not retrieve vehicle details.");
        return;
    }

    res = mysql_store_result(conn);

    if (res == NULL)
    {
        json_error("Could not read vehicle details.");
        return;
    }

    row = mysql_fetch_row(res);

    if (row == NULL || row[0] == NULL || row[1] == NULL)
    {
        mysql_free_result(res);
        json_error("Parked vehicle was not found in the database.");
        return;
    }

    slot_id = atoi(row[0]);
    fee = atoi(row[1]);

    mysql_free_result(res);

    if (!db_query("START TRANSACTION"))
    {
        json_error("Could not start the database transaction.");
        return;
    }

    /* Record the exit and fee */
    snprintf(
        sql, sizeof(sql),
        "INSERT INTO exit_history "
        "(vehicle_number, slot_id, entry_time, exit_time, fee) "
        "SELECT vehicle_number, slot_id, entry_time, NOW(), %d "
        "FROM vehicles "
        "WHERE vehicle_number='%s' AND status='Parked' "
        "ORDER BY entry_time DESC, vehicle_id DESC LIMIT 1",
        fee, vehicle);

    if (!db_query(sql) || mysql_affected_rows(conn) != 1)
    {
        db_query("ROLLBACK");
        json_error("Could not save the exit history.");
        return;
    }

    snprintf(
        sql, sizeof(sql),
        "UPDATE vehicles SET status='Exited', exit_time=NOW(), fee=%d "
        "WHERE vehicle_number='%s' AND status='Parked' "
        "ORDER BY entry_time DESC, vehicle_id DESC LIMIT 1",
        fee, vehicle);

    if (!db_query(sql) || mysql_affected_rows(conn) != 1)
    {
        db_query("ROLLBACK");
        json_error("Could not update the vehicle exit status.");
        return;
    }

    snprintf(
        sql, sizeof(sql),
        "UPDATE parking_slots SET status='Available' WHERE slot_id=%d",
        slot_id);

    if (!db_query(sql) || mysql_affected_rows(conn) != 1)
    {
        db_query("ROLLBACK");
        json_error("Could not make the parking slot available.");
        return;
    }

    /* If someone is waiting, promote the oldest queue vehicle */
    if (count > 0)
    {
        strcpy(waiting_vehicle, waiting[front]);

        snprintf(
            sql, sizeof(sql),
            "UPDATE vehicles SET status='Parked', slot_id=%d, "
            "entry_time=NOW(), exit_time=NULL, fee=0 "
            "WHERE vehicle_number='%s' AND status='Waiting'",
            slot_id, waiting_vehicle);

        if (!db_query(sql) || mysql_affected_rows(conn) != 1)
        {
            db_query("ROLLBACK");
            json_error("Could not move the next waiting vehicle into parking.");
            return;
        }

        snprintf(
            sql, sizeof(sql),
            "DELETE FROM waiting_queue WHERE vehicle_number='%s'",
            waiting_vehicle);

        if (!db_query(sql) || mysql_affected_rows(conn) != 1)
        {
            db_query("ROLLBACK");
            json_error("Could not remove the promoted vehicle from the queue.");
            return;
        }

        snprintf(
            sql, sizeof(sql),
            "UPDATE parking_slots SET status='Occupied' WHERE slot_id=%d",
            slot_id);

        if (!db_query(sql) || mysql_affected_rows(conn) != 1)
        {
            db_query("ROLLBACK");
            json_error("Could not update the slot for the waiting vehicle.");
            return;
        }
    }

    if (!db_query("COMMIT"))
    {
        db_query("ROLLBACK");
        json_error("Could not complete the exit operation.");
        return;
    }

    /* Update in-memory Stack and Circular Queue */
    pop_vehicle(top_vehicle);

    if (count > 0)
    {
        dequeue_vehicle(waiting_vehicle);
        push_vehicle(waiting_vehicle);
    }

    snprintf(
        sql, sizeof(sql),
        "Vehicle exited successfully. Parking fee: Rs %d.",
        fee);

    json_success(sql);
}

/* ---------- MAIN ---------- */

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        json_error("Usage: parking.exe park VEHICLE or parking.exe leave VEHICLE");
        return 1;
    }

    if (!connect_db())
    {
        json_error("Could not connect to the ParkEase database.");
        return 1;
    }

    if (strcmp(argv[1], "park") == 0)
    {
        park_vehicle(argv[2]);
    }
    else if (strcmp(argv[1], "leave") == 0)
    {
        leave_vehicle(argv[2]);
    }
    else
    {
        json_error("Unknown action. Use park or leave.");
    }

    mysql_close(conn);
    return 0;
}
