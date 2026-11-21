#include <stdio.h>
#include <string.h>

#define SIZE 5
#define MAX_WAIT 10
#define REG 20


char parking[SIZE][REG];
int top = -1;


char waiting[MAX_WAIT][REG];
int front = 0, rear = -1, count = 0;


void parkVehicle() {
    char vehicle[REG];

    printf("Enter vehicle registration number: ");
    scanf("%19s", vehicle);

    if (top < SIZE - 1) {
        strcpy(parking[++top], vehicle);
        printf("Vehicle parked successfully.\n");
    } else if (count < MAX_WAIT) {
        rear = (rear + 1) % MAX_WAIT;
        strcpy(waiting[rear], vehicle);
        count++;
        printf("Parking full. Vehicle added to waiting queue.\n");
    } else {
        printf("Parking and waiting queue are full!\n");
    }
}


void leaveParking() {
    if (top == -1) {
        printf("Parking area is empty!\n");
        return;
    }

    printf("Vehicle %s has left parking.\n", parking[top]);
    top--;

    
    if (count > 0) {
        top++;
        strcpy(parking[top], waiting[front]);

        printf("Vehicle %s parked from waiting queue.\n",
               waiting[front]);

        front = (front + 1) % MAX_WAIT;
        count--;
    }
}


void displayParking() {
    if (top == -1) {
        printf("No vehicles parked.\n");
        return;
    }

    printf("\n--- PARKED VEHICLES (STACK) ---\n");

    for (int i = top; i >= 0; i--) {
        printf("%d. %s\n", i + 1, parking[i]);
    }
}


void displayQueue() {
    if (count == 0) {
        printf("Waiting queue is empty.\n");
        return;
    }

    printf("\n--- WAITING VEHICLES (QUEUE) ---\n");

    for (int i = 0; i < count; i++) {
        int index = (front + i) % MAX_WAIT;
        printf("%d. %s\n", i + 1, waiting[index]);
    }
}

int main() {
    int choice;

    do {
        printf("\n===== PARKING MANAGEMENT SYSTEM =====\n");
        printf("1. Park Vehicle\n");
        printf("2. Leave Parking\n");
        printf("3. Display Parked Vehicles\n");
        printf("4. Display Waiting Queue\n");
        printf("5. Exit\n");
        printf("Enter your choice: ");

        if (scanf("%d", &choice) != 1) {
            printf("Invalid input!\n");
            return 1;
        }

        switch (choice) {
            case 1:
                parkVehicle();
                break;

            case 2:
                leaveParking();
                break;

            case 3:
                displayParking();
                break;

            case 4:
                displayQueue();
                break;

            case 5:
                printf("Exiting Parking Management System.\n");
                break;

            default:
                printf("Invalid choice! Try again.\n");
        }

    } while (choice != 5);

    return 0;
}
#include <stdio.h>
#include <string.h>

#define SIZE 5
#define MAX_WAIT 10
#define REG 20
#define RATE 20

char parking[SIZE][REG];
int top = -1;

char waiting[MAX_WAIT][REG];
int front = 0, rear = -1, count = 0;

void parkVehicle() {
    char vehicle[REG];

    printf("Enter vehicle registration number: ");
    scanf("%19s", vehicle);

    if (top < SIZE - 1) {
        strcpy(parking[++top], vehicle);
        printf("Vehicle parked successfully.\n");
    }
    else if (count < MAX_WAIT) {
        rear = (rear + 1) % MAX_WAIT;
        strcpy(waiting[rear], vehicle);
        count++;
        printf("Parking full. Vehicle added to waiting queue.\n");
    }
    else {
        printf("Parking and waiting queue are full!\n");
    }
}

void leaveParking() {
    int hours;
    float fee;

    if (top == -1) {
        printf("Parking area is empty!\n");
        return;
    }

    printf("Vehicle %s is leaving.\n", parking[top]);

    printf("Enter parking duration in hours: ");
    if (scanf("%d", &hours) != 1 || hours < 1) {
        printf("Invalid duration!\n");
        return;
    }

    fee = hours * RATE;

    printf("\n----- PARKING BILL -----\n");
    printf("Vehicle Number : %s\n", parking[top]);
    printf("Hours Parked   : %d\n", hours);
    printf("Rate per Hour  : Rs. %d\n", RATE);
    printf("Total Fee      : Rs. %.2f\n", fee);
    printf("------------------------\n");

    top--;

    if (count > 0) {
        strcpy(parking[++top], waiting[front]);

        printf("Vehicle %s parked from waiting queue.\n",
               waiting[front]);

        front = (front + 1) % MAX_WAIT;
        count--;
    }
}

void displayParking() {
    if (top == -1) {
        printf("No vehicles parked.\n");
        return;
    }

    printf("\n--- PARKED VEHICLES (STACK) ---\n");

    for (int i = top; i >= 0; i--) {
        printf("%d. %s\n", i + 1, parking[i]);
    }
}

void displayQueue() {
    if (count == 0) {
        printf("Waiting queue is empty.\n");
        return;
    }

    printf("\n--- WAITING VEHICLES (QUEUE) ---\n");

    for (int i = 0; i < count; i++) {
        int index = (front + i) % MAX_WAIT;
        printf("%d. %s\n", i + 1, waiting[index]);
    }
}

int main() {
    int choice;

    do {
        printf("\n===== PARKEASE PARKING SYSTEM =====\n");
        printf("1. Park Vehicle\n");
        printf("2. Leave Parking and Calculate Fee\n");
        printf("3. Display Parked Vehicles\n");
        printf("4. Display Waiting Queue\n");
        printf("5. Exit\n");
        printf("Enter your choice: ");

        if (scanf("%d", &choice) != 1) {
            printf("Invalid input!\n");
            return 1;
        }

        switch (choice) {
            case 1:
                parkVehicle();
                break;

            case 2:
                leaveParking();
                break;

            case 3:
                displayParking();
                break;

            case 4:
                displayQueue();
                break;

            case 5:
                printf("Exiting Parking Management System.\n");
                break;

            default:
                printf("Invalid choice! Try again.\n");
        }

    } while (choice != 5);

    return 0;
}
