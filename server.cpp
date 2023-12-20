#include <iostream>
#include <dbus/dbus.h>

void clean_up(int status, void *conn) {
    dbus_connection_unref((DBusConnection*) conn);

    fprintf(stderr, "Status: %d \n", status);
}


int main() {
    
    DBusError err;
    DBusConnection *conn;
    int ret {};

    dbus_error_init(&err);

    if (dbus_error_is_set(&err)) {
        fprintf(stderr, "Error: %s \n", err.message);
        dbus_error_free(&err);
    }

    conn = dbus_bus_get(DBUS_BUS_SESSION, &err);

    if (conn == NULL) {
        EXIT_FAILURE;
    }


    ret = dbus_bus_request_name(conn, 
        "com.test.app_bus",
        DBUS_NAME_FLAG_REPLACE_EXISTING,
        &err);



    if (ret != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER) {
        exit(1);
    }

    const char *unique_name = dbus_bus_get_unique_name(conn);

    fprintf(stdout, "Unique Name: %s\n", unique_name);

    while(1) {}
    

    // dbus_connection_close(conn);
    on_exit(&clean_up, conn);

    return 0;
}
