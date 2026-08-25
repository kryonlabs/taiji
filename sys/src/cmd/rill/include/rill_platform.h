#ifndef RILL_PLATFORM_H
#define RILL_PLATFORM_H

#define RILL_MAX_LAUNCHERS 32
#define RILL_MAX_TASKS 32

typedef struct RillLauncher {
    char id[64];
    char name[96];
    char command[256];
    char icon_path[512];
} RillLauncher;

typedef struct RillTask {
    int id;
    char title[128];
    int focused;
    int urgent;
} RillTask;

typedef struct RillPlatformServices {
    const char *name;
    int (*list_launchers)(RillLauncher *out, int cap);
    int (*list_tasks)(RillTask *out, int cap);
    int (*launch)(const RillLauncher *launcher);
    int (*focus_task)(int task_id);
    int (*close_task)(int task_id);
    const char *(*settings_root)(void);
} RillPlatformServices;

const RillPlatformServices *RillPlatformCurrent(void);
const RillPlatformServices *RillPlatformStub(void);

#endif
