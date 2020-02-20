// Copyright (C) 2020 github.com/aramg
#ifndef __ADB_CMD_H__
#define __ADB_CMD_H__

#define DEVICES_LIMIT 8

struct AdbDevice {
    char serial[80];
    char state[48];
    AdbDevice(){}
    ~AdbDevice(){}
};

struct AdbMgr {
    AdbDevice* deviceList[DEVICES_LIMIT];
    AdbMgr();
    ~AdbMgr();
    bool reload(void);
};

#endif
