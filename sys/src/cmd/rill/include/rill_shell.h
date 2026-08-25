#ifndef RILL_SHELL_H
#define RILL_SHELL_H

#include "rill_platform.h"

typedef enum RillPanelSide {
    RILL_PANEL_TOP = 0,
    RILL_PANEL_BOTTOM,
    RILL_PANEL_LEFT,
    RILL_PANEL_RIGHT
} RillPanelSide;

typedef enum RillAppKind {
    RILL_APP_TERMINAL = 0,
    RILL_APP_FILES,
    RILL_APP_SETTINGS,
    RILL_APP_ABOUT
} RillAppKind;

typedef struct RillAppWindow {
    int id;
    RillAppKind kind;
    char title[96];
    int x;
    int y;
    int w;
    int h;
    int focused;
    char host_id[64];
} RillAppWindow;

#define RILL_MAX_APPS 16
#define RILL_MAX_EXTERNAL_TASKS 16

typedef struct RillShellState {
    RillLauncher launchers[RILL_MAX_LAUNCHERS];
    int launcher_count;
    int selected_launcher;

    RillTask tasks[RILL_MAX_TASKS];
    int task_count;
    int selected_task;

    RillPanelSide panel_side;
    RillAppWindow apps[RILL_MAX_APPS];
    int app_count;
    RillTask external_tasks[RILL_MAX_EXTERNAL_TASKS];
    int external_task_count;
    int focused_app;
    int next_app_id;
    int next_task_id;
    int dragging_app;
    int drag_offset_x;
    int drag_offset_y;
    int menu_open;
    int settings_open;
    char status[160];
} RillShellState;

void RillShellInit(RillShellState *shell);
void RillShellRefresh(RillShellState *shell,
                      const RillPlatformServices *platform);
int RillShellSelectLauncher(RillShellState *shell, int index);
int RillShellLaunchSelected(RillShellState *shell,
                            const RillPlatformServices *platform);
int RillShellLaunchSelectedInternal(RillShellState *shell);
int RillShellOpenLauncher(RillShellState *shell,
                          const RillLauncher *launcher);
int RillShellFocusApp(RillShellState *shell, int app_id);
int RillShellCloseApp(RillShellState *shell, int app_id);
int RillShellSelectTask(RillShellState *shell, int index);
int RillShellFocusSelectedTask(RillShellState *shell,
                               const RillPlatformServices *platform);
void RillShellSetStatus(RillShellState *shell, const char *status);

#endif
