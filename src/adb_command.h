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
    int iter;
    AdbDevice* deviceList[DEVICES_LIMIT];
    AdbMgr();
    ~AdbMgr();
    bool Reload(void);

    inline void ResetIter(void){ iter = 0; }
    AdbDevice* NextDevice(int *is_offline);
};

#endif
