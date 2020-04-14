#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>

const int CMD_SIZE = 256;
const int ORDER_MAP_SIZE = 1024;

typedef struct {
    char name[64];
    char phone[64];
} employee_t;

bool read_entry(FILE *database, int i, employee_t *entry, const short *order_map, short order_map_size) {
    fseek(database,(long) sizeof(order_map_size) + (long) sizeof(order_map[0]) * ORDER_MAP_SIZE
                   + order_map[i] * (long) sizeof(employee_t), SEEK_SET);
    if (fread(entry, sizeof(employee_t), 1, database) != 1) {
        puts("Database structure is corrupted");
        return false;
    }
    return true;
}

int binary_search(FILE *database, employee_t *employee, short *order_map, short order_map_size) {
    int l = 0, r = order_map_size;
    employee_t entry;

    while (l < r) {
        int mid = (l + r) / 2;
        if (!read_entry(database, mid, &entry, order_map, order_map_size)) {
            return -1;
        }
        if (strcmp(employee->phone, entry.phone) >= 0) {
            l = mid + 1;
        } else {
            r = mid;
        }
    }

    return l;
}

bool check_database(FILE *database) {
    if (!database) {
        puts("No database is used. Execute 'use db.bin' to use a database (replace 'db.bin' with database file name)");
        return false;
    }
    return true;
}

bool use_database(FILE **database, char *cmd) {
    if (*database) {
        fclose(*database);
    }

    *database = fopen(cmd + 4, "a+");
    if (!*database) {
        puts("Failed to open database file");
        return false;
    }

    fclose(*database);
    *database = fopen(cmd + 4, "rb+");
    if (!*database) {
        puts("Failed to open database file");
        return false;
    }

    printf("Database %s is loaded successfully\n", cmd + 4);
    return true;
}

bool read_order_map(FILE *database, short *order_map, short *order_map_size) {
    fseek(database, 0, SEEK_SET);

    if (fread(order_map_size, sizeof(*order_map_size), 1, database) != 1) {
        puts("Database is not initialized. Execute 'restore' (WARNING: all previous data will be lost) to initialize the database");
        return false;
    }

    if (fread(order_map, sizeof(order_map[0]), ORDER_MAP_SIZE, database) != ORDER_MAP_SIZE) {
        puts("Database structure is corrupted");
        return false;
    }

    return true;
}

bool select_entries(FILE *database, employee_t *entry, short *order_map, short order_map_size) {
    if (!read_order_map(database, order_map, &order_map_size)) {
        return false;
    }

    puts("+-----+------------------+----------------------+");
    puts("| #   | Name             | Phone                |");
    puts("+=====+==================+======================+");
    for (int i = 0; i < order_map_size; ++i) {
        if (!read_entry(database, i, entry, order_map, order_map_size)) {
            break;
        }
        printf("| %-3d | %-16s | %-20s |\n", i + 1, entry->name, entry->phone);
        puts("+-----+------------------+----------------------+");
    }
    printf("\nShowing %d entries\n", order_map_size);

    return true;
}

bool parse_insert_cmd(char *cmd, employee_t *entry) {
    char *name, *phone;
    name = strtok(cmd + 7, " ");
    if (name == NULL) {
        puts("Incorrect command format");
        return false;
    }

    phone = strtok(NULL, " ");
    if (phone == NULL) {
        puts("Incorrect command format");
        return false;
    }

    if (strtok(NULL, " ") != NULL) {
        puts("Incorrect command format");
        return false;
    }

    strcpy(entry->name, name);
    strcpy(entry->phone, phone);

    return true;
}

bool insert(FILE *database, employee_t *entry, short *order_map, short order_map_size, char *cmd) {
    int index;

    if (!parse_insert_cmd(cmd, entry)) {
        return false;
    }

    if (!check_database(database)) {
        return false;
    }

    if (!read_order_map(database, order_map, &order_map_size)) {
        return false;
    }

    index = binary_search(database, entry, order_map, order_map_size);
    if (index == -1) {
        return false;
    }

    fseek(database, 0, SEEK_SET);
    ++order_map_size;
    fwrite(&order_map_size, sizeof(order_map_size), 1, database);
    --order_map_size;

    if (index == 0) {
        fwrite(&order_map_size, sizeof(order_map[0]), 1, database);
        fwrite(order_map, sizeof(order_map[0]), ORDER_MAP_SIZE - 1, database);
    } else if (index == ORDER_MAP_SIZE) {
        fwrite(order_map, sizeof(order_map[0]), ORDER_MAP_SIZE - 1, database);
        fwrite(&order_map_size, sizeof(order_map[0]), 1, database);
    } else {
        fwrite(order_map, sizeof(order_map[0]), index, database);
        fwrite(&order_map_size, sizeof(order_map[0]), 1, database);
        fwrite(order_map + index, sizeof(order_map[0]), ORDER_MAP_SIZE - index - 1, database);
    }

    fseek(database, 0, SEEK_END);
    fwrite(entry, sizeof(employee_t), 1, database);
    fflush(database);

    return true;
}

int main() {
    employee_t entry;
    FILE *database = NULL;
    char cmd[CMD_SIZE];
    short order_map_size = 0;
    short order_map[ORDER_MAP_SIZE];

    cmd[0] = '\0';
    order_map[0] = -1;

    while (true) {
        printf("~> ");

        fgets(cmd, CMD_SIZE - 1, stdin);
        cmd[strlen(cmd) - 1] = '\0';

        if (strncmp(cmd, "use ", 4) == 0) {
            use_database(&database, cmd);
        } else if (strcmp(cmd, "select") == 0) {
            if (check_database(database)) {
                select_entries(database, &entry, order_map, order_map_size);
            }
        } else if (strncmp(cmd, "insert ", 7) == 0) {
            insert(database, &entry, order_map, order_map_size, cmd);
        } else if (strcmp(cmd, "restore") == 0) {
            if (check_database(database)) {
                fseek(database, 0, SEEK_SET);
                order_map_size = 0;
                fwrite(&order_map_size, sizeof(order_map_size), 1, database);
                fwrite(&order_map, sizeof(order_map), 1, database);
                fflush(database);
                ftruncate(fileno(database), ftell(database));
            }
        } else if (strcmp(cmd, "exit") == 0) {
            break;
        } else {
            puts("Command is not recognized");
        }
    }

    if (database) {
        fclose(database);
    }

    return 0;
}