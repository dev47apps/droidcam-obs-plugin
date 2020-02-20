#include <stdio.h>
#include <libgen.h>

#include <plugin.h>
#include <adb_command.h>

int main(int argc, char** argv) {
    AdbMgr adbMgr;
    adbMgr.reload();
    return 0;
}
