#include "rill_shell.h"

#include <stdio.h>
#include <string.h>

static int
clamp_index(int value, int count)
{
    if(count <= 0)
        return -1;
    if(value < 0)
        return 0;
    if(value >= count)
        return count - 1;
    return value;
}

void
RillShellInit(RillShellState *shell)
{
    if(shell == NULL)
        return;
    memset(shell, 0, sizeof(*shell));
    shell->selected_launcher = -1;
    shell->selected_task = -1;
    shell->panel_side = RILL_PANEL_TOP;
    shell->focused_app = -1;
    shell->next_app_id = 1;
    shell->next_task_id = 1000;
    RillShellSetStatus(shell, "Rill is starting");
}

void
RillShellSetStatus(RillShellState *shell, const char *status)
{
    if(shell == NULL)
        return;
    if(status == NULL)
        status = "";
    snprintf(shell->status, sizeof(shell->status), "%s", status);
}

void
RillShellRefresh(RillShellState *shell, const RillPlatformServices *platform)
{
    int launcher_count;
    int task_count;

    if(shell == NULL || platform == NULL)
        return;

    launcher_count = 0;
    task_count = 0;
    if(platform->list_launchers != NULL)
        launcher_count = platform->list_launchers(shell->launchers,
                                                  RILL_MAX_LAUNCHERS);

    if(launcher_count < 0)
        launcher_count = 0;
    if(launcher_count > RILL_MAX_LAUNCHERS)
        launcher_count = RILL_MAX_LAUNCHERS;
    shell->launcher_count = launcher_count;
    shell->task_count = 0;
    for(int i = 0; i < shell->app_count && shell->task_count < RILL_MAX_TASKS;
        i++) {
        shell->tasks[i].id = shell->apps[i].id;
        snprintf(shell->tasks[i].title, sizeof(shell->tasks[i].title), "%s",
                 shell->apps[i].title);
        shell->tasks[i].focused = shell->apps[i].focused;
        shell->tasks[i].urgent = 0;
        shell->task_count++;
    }
    if(platform->list_tasks != NULL &&
       shell->task_count < RILL_MAX_TASKS) {
        task_count = platform->list_tasks(
            &shell->tasks[shell->task_count],
            RILL_MAX_TASKS - shell->task_count);
        if(task_count < 0)
            task_count = 0;
        if(task_count > RILL_MAX_TASKS - shell->task_count)
            task_count = RILL_MAX_TASKS - shell->task_count;
        shell->task_count += task_count;
    }
    for(int i = 0;
        i < shell->external_task_count && shell->task_count < RILL_MAX_TASKS;
        i++) {
        shell->tasks[shell->task_count] = shell->external_tasks[i];
        shell->task_count++;
    }
    shell->selected_launcher = clamp_index(shell->selected_launcher,
                                           shell->launcher_count);
    shell->selected_task = clamp_index(shell->selected_task,
                                       shell->task_count);
}

static RillAppKind
kind_for_launcher(const RillLauncher *launcher)
{
    const char *command;

    if(launcher == NULL)
        return RILL_APP_ABOUT;
    command = launcher->command;
    if(strcmp(command, "internal:terminal") == 0 ||
       strcmp(command, "host:kterm") == 0 ||
       strcmp(command, "host:kapsule") == 0)
        return RILL_APP_TERMINAL;
    if(strcmp(command, "internal:files") == 0)
        return RILL_APP_FILES;
    if(strcmp(command, "host:shelf") == 0)
        return RILL_APP_FILES;
    if(strcmp(command, "internal:settings") == 0)
        return RILL_APP_SETTINGS;
    return RILL_APP_ABOUT;
}

static void
focus_app_index(RillShellState *shell, int index)
{
    RillAppWindow app;
    int i;

    if(shell == NULL || index < 0 || index >= shell->app_count)
        return;
    app = shell->apps[index];
    if(index < shell->app_count - 1) {
        memmove(&shell->apps[index], &shell->apps[index + 1],
                (size_t)(shell->app_count - index - 1) *
                    sizeof(shell->apps[0]));
        shell->apps[shell->app_count - 1] = app;
    }
    for(i = 0; i < shell->app_count; i++)
        shell->apps[i].focused = i == shell->app_count - 1;
    shell->focused_app = shell->apps[shell->app_count - 1].id;
}

int
RillShellOpenLauncher(RillShellState *shell, const RillLauncher *launcher)
{
    RillAppWindow *app;
    int offset;

    if(shell == NULL || launcher == NULL)
        return 0;
    for(int i = 0; i < shell->app_count; i++) {
        if(strcmp(shell->apps[i].title, launcher->name) == 0) {
            focus_app_index(shell, i);
            snprintf(shell->status, sizeof(shell->status), "%s", launcher->name);
            return 1;
        }
    }
    if(shell->app_count >= RILL_MAX_APPS) {
        RillShellSetStatus(shell, "Too many open Rill apps");
        return 0;
    }

    offset = shell->app_count * 28;
    app = &shell->apps[shell->app_count++];
    memset(app, 0, sizeof(*app));
    app->id = shell->next_app_id++;
    app->kind = kind_for_launcher(launcher);
    if(strncmp(launcher->command, "host:", 5) == 0)
        snprintf(app->host_id, sizeof(app->host_id), "%s",
                 launcher->command + 5);
    snprintf(app->title, sizeof(app->title), "%s", launcher->name);
    app->x = 150 + offset;
    app->y = 86 + offset;
    app->w = app->kind == RILL_APP_SETTINGS ? 460 : 560;
    app->h = app->kind == RILL_APP_SETTINGS ? 330 : 380;
    focus_app_index(shell, shell->app_count - 1);
    snprintf(shell->status, sizeof(shell->status), "Opened %s", launcher->name);
    return 1;
}

int
RillShellLaunchSelectedInternal(RillShellState *shell)
{
    if(shell == NULL || shell->selected_launcher < 0 ||
       shell->selected_launcher >= shell->launcher_count)
        return 0;
    return RillShellOpenLauncher(shell,
                                 &shell->launchers[shell->selected_launcher]);
}

int
RillShellSelectLauncher(RillShellState *shell, int index)
{
    if(shell == NULL || index < 0 || index >= shell->launcher_count)
        return 0;
    shell->selected_launcher = index;
    return 1;
}

int
RillShellFocusApp(RillShellState *shell, int app_id)
{
    if(shell == NULL)
        return 0;
    for(int i = 0; i < shell->app_count; i++) {
        if(shell->apps[i].id == app_id) {
            focus_app_index(shell, i);
            snprintf(shell->status, sizeof(shell->status), "%s",
                     shell->apps[i].title);
            return 1;
        }
    }
    return 0;
}

int
RillShellCloseApp(RillShellState *shell, int app_id)
{
    int i;

    if(shell == NULL)
        return 0;
    for(i = 0; i < shell->app_count; i++) {
        if(shell->apps[i].id == app_id) {
            memmove(&shell->apps[i], &shell->apps[i + 1],
                    (size_t)(shell->app_count - i - 1) *
                        sizeof(shell->apps[0]));
            shell->app_count--;
            if(shell->app_count > 0)
                focus_app_index(shell, shell->app_count - 1);
            else
                shell->focused_app = -1;
            RillShellSetStatus(shell, "Closed app");
            return 1;
        }
    }
    return 0;
}

static void
focus_external_task_index(RillShellState *shell, int index)
{
    int i;

    if(shell == NULL || index < 0 || index >= shell->external_task_count)
        return;
    for(i = 0; i < shell->app_count; i++)
        shell->apps[i].focused = 0;
    for(i = 0; i < shell->external_task_count; i++)
        shell->external_tasks[i].focused = i == index;
    shell->focused_app = -1;
}

int
RillShellLaunchSelected(RillShellState *shell,
                        const RillPlatformServices *platform)
{
    RillLauncher *launcher;

    if(shell == NULL || platform == NULL || platform->launch == NULL)
        return 0;
    if(shell->selected_launcher < 0 ||
       shell->selected_launcher >= shell->launcher_count)
        return 0;

    launcher = &shell->launchers[shell->selected_launcher];
    if(strncmp(launcher->command, "internal:", 9) == 0 ||
       strncmp(launcher->command, "host:", 5) == 0)
        return RillShellOpenLauncher(shell, launcher);

    if(platform->launch(launcher)) {
        snprintf(shell->status, sizeof(shell->status), "Launched %s",
                 launcher->name);
        return 1;
    }

    snprintf(shell->status, sizeof(shell->status), "Could not launch %s",
             launcher->name);
    return 0;
}

int
RillShellSelectTask(RillShellState *shell, int index)
{
    if(shell == NULL || index < 0 || index >= shell->task_count)
        return 0;
    shell->selected_task = index;
    return 1;
}

int
RillShellFocusSelectedTask(RillShellState *shell,
                           const RillPlatformServices *platform)
{
    RillTask *task;

    if(shell == NULL || platform == NULL || platform->focus_task == NULL)
        return 0;
    if(shell->selected_task < 0 || shell->selected_task >= shell->task_count)
        return 0;

    task = &shell->tasks[shell->selected_task];
    if(RillShellFocusApp(shell, task->id))
        return 1;
    for(int i = 0; i < shell->external_task_count; i++) {
        if(shell->external_tasks[i].id == task->id) {
            focus_external_task_index(shell, i);
            snprintf(shell->status, sizeof(shell->status), "%s",
                     shell->external_tasks[i].title);
            return 1;
        }
    }
    if(platform->focus_task(task->id)) {
        snprintf(shell->status, sizeof(shell->status), "Focused %s",
                 task->title);
        return 1;
    }

    snprintf(shell->status, sizeof(shell->status), "Could not focus %s",
             task->title);
    return 0;
}
