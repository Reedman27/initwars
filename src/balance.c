#include "balance.h"

const CharDef CHARS[CH_COUNT] = {
    { "SysVinit", "Old veteran. Reliable, stubborn.",
      "telinit 1", "Shockwave: wipes nearby shots, hurts foes",
      6, 1.50f, 20, 3.4f, 3.6f, 4, 600, SP_PULSE, C_LTGREY },
    { "runit", "Quiet, efficient, slightly smug.",
      "sv dash", "Quick dash with brief invulnerability",
      5, 1.90f, 7, 1.0f, 4.2f, 4, 270, SP_DASH, C_BGREEN },
    { "s6", "Precise and analytical.",
      "s6-stasis", "Slows nearby foes and their shots",
      5, 1.55f, 14, 2.0f, 5.0f, 4, 660, SP_STASIS, C_CYAN },
    { "OpenRC", "Organized and adaptable.",
      "rc-update", "Resets every shot, line and barrier",
      5, 1.50f, 15, 2.0f, 3.8f, 4, 600, SP_CLEAR, C_AMBER },
    { "BusyBox init", "Tiny, fearless, absurd.",
      "--emergency", "Short shield and heals 1 HP",
      3, 1.75f, 9, 1.0f, 4.0f, 3, 960, SP_SHIELD, C_WHITE },
};

const EnemyDef ENEMIES[E_COUNT] = {
    [E_NONE]      = { "none", "", 0, 0, 0, 0, false, false },
    [E_JOURNALD]  = { "systemd-journald", "NOTICE: PLAYER STILL RUNNING. INVESTIGATING.",
                      8, 12, 12, 100, true, true },
    [E_LOGIND]    = { "systemd-logind", "SESSION TERMINATED. PLEASE LEAVE THE MACHINE.",
                      12, 12, 12, 150, true, true },
    [E_NETWORKD]  = { "systemd-networkd", "LINK IS UP. YOUR CHANCES ARE DOWN.",
                      10, 12, 12, 150, true, true },
    [E_TIMER]     = { "systemd-timer", "YOUR APPOINTMENT WITH FAILURE IS NOW.",
                      10, 12, 12, 200, true, true },
    [E_UDEVD]     = { "systemd-udevd", "NEW DEVICE DETECTED: UNLICENSED INIT SYSTEM.",
                      14, 12, 12, 200, true, true },
    [E_RESOLVED]  = { "systemd-resolved", "NAME RESOLUTION FAILED. TRY SHOOTING THE OTHER ONE.",
                      8, 12, 12, 150, true, true },
    [E_DEPTARGET] = { "dependency.target", "CANNOT DEFEAT ME UNTIL YOU DEFEAT MY REQUIREMENTS.",
                      20, 16, 16, 250, true, true },
    [E_USB]       = { "usb stick", "", 2, 8, 8, 25, true, false },
    [E_FLOPPY]    = { "floppy disk", "", 2, 8, 8, 25, true, false },
    [E_PRINTER]   = { "printer", "PC LOAD LETTER?", 3, 12, 8, 40, true, false },
    [E_FAKE]      = { "fake target", "NXDOMAIN.", 1, 12, 12, 0, false, false },
    [E_MARKER]    = { "restart marker", "PROCESS EXITED. RESTARTING.", 1, 8, 8, 0, false, true },
    [E_CRON]      = { "cron daemon", "SCHEDULED TASK: YOUR TERMINATION.", 13, 14, 14, 225, true, true },
    [E_OOMD]      = { "OOM killer", "OUT OF MEMORY. OUT OF MERCY.", 22, 16, 16, 300, true, true },
    [E_BOSS]      = { "systemd", "SYSTEMD: EVERYTHING IS A UNIT.", 270, 48, 32, 5000, true, true },
};

#define SP(t, n, r) { E_##t, n, r }
const WaveDef WAVES[NUM_WAVES] = {
    /* Act I - The Old Machine: learn to move, dodge and shoot (waves 1-5) */
    { { SP(JOURNALD, 3, 0) }, 1 },
    { { SP(JOURNALD, 2, 0), SP(LOGIND, 1, 0) }, 2 },
    { { SP(NETWORKD, 1, 0), SP(JOURNALD, 2, 0), SP(LOGIND, 1, 0) }, 3 },
    { { SP(TIMER, 1, 0), SP(JOURNALD, 2, 0), SP(NETWORKD, 1, 0) }, 3 },
    { { SP(JOURNALD, 3, 0), SP(LOGIND, 1, 0), SP(TIMER, 1, 0) }, 3 },
    /* Act II - The Migration (waves 6-12): hardware minions, first Restart=always */
    { { SP(UDEVD, 1, 0), SP(LOGIND, 1, 0), SP(NETWORKD, 1, 0), SP(JOURNALD, 2, 0) }, 4 },
    { { SP(UDEVD, 1, 0), SP(LOGIND, 2, 0), SP(TIMER, 1, 0), SP(JOURNALD, 1, 1) }, 4 },
    { { SP(RESOLVED, 1, 0), SP(NETWORKD, 1, 0), SP(JOURNALD, 2, 0), SP(TIMER, 1, 0) }, 4 },
    { { SP(UDEVD, 1, 0), SP(LOGIND, 2, 0), SP(TIMER, 1, 0), SP(JOURNALD, 2, 1) }, 4 },
    { { SP(RESOLVED, 1, 0), SP(NETWORKD, 1, 0), SP(JOURNALD, 2, 1), SP(UDEVD, 1, 0), SP(TIMER, 1, 0) }, 5 },
    { { SP(DEPTARGET, 1, 0), SP(JOURNALD, 3, 0), SP(TIMER, 2, 0), SP(LOGIND, 1, 0) }, 4 },
    { { SP(DEPTARGET, 1, 0), SP(NETWORKD, 2, 0), SP(LOGIND, 1, 0), SP(TIMER, 1, 0), SP(JOURNALD, 1, 1) }, 5 },
    /* Act III - The Infinite Boot (waves 13-18): dependency links, cron and the OOM killer join in */
    { { SP(CRON, 1, 0), SP(JOURNALD, 3, 0), SP(TIMER, 1, 0), SP(NETWORKD, 1, 0), SP(LOGIND, 1, 0) }, 5 },
    { { SP(CRON, 1, 0), SP(UDEVD, 1, 0), SP(RESOLVED, 1, 0), SP(TIMER, 1, 0), SP(JOURNALD, 2, 1) }, 5 },
    { { SP(DEPTARGET, 1, 0), SP(CRON, 1, 0), SP(TIMER, 2, 0), SP(JOURNALD, 2, 0), SP(NETWORKD, 1, 0) }, 5 },
    { { SP(DEPTARGET, 1, 0), SP(UDEVD, 1, 0), SP(JOURNALD, 2, 1), SP(TIMER, 2, 0), SP(NETWORKD, 1, 0) }, 5 },
    { { SP(OOMD, 1, 0), SP(JOURNALD, 3, 0), SP(LOGIND, 1, 0), SP(TIMER, 1, 0), SP(NETWORKD, 1, 0) }, 5 },
    { { SP(OOMD, 1, 0), SP(CRON, 1, 0), SP(RESOLVED, 1, 0), SP(NETWORKD, 1, 0), SP(TIMER, 1, 0), SP(JOURNALD, 1, 1) }, 6 },
    /* Act IV - PID 1 (waves 19-23): everything at once, then the boss */
    { { SP(DEPTARGET, 2, 0), SP(OOMD, 1, 0), SP(TIMER, 2, 0), SP(LOGIND, 1, 0) }, 4 },
    { { SP(DEPTARGET, 1, 0), SP(CRON, 2, 0), SP(UDEVD, 1, 0), SP(TIMER, 1, 0), SP(NETWORKD, 1, 0), SP(JOURNALD, 2, 1) }, 6 },
    { { SP(DEPTARGET, 2, 0), SP(RESOLVED, 1, 0), SP(TIMER, 2, 0), SP(NETWORKD, 1, 0),
        SP(LOGIND, 1, 1), SP(UDEVD, 1, 0) }, 6 },
    { { SP(OOMD, 1, 0), SP(CRON, 1, 0), SP(DEPTARGET, 1, 0), SP(UDEVD, 1, 0), SP(TIMER, 2, 0), SP(LOGIND, 1, 1) }, 6 },
    { { SP(OOMD, 2, 0), SP(CRON, 1, 0), SP(DEPTARGET, 2, 0), SP(TIMER, 1, 0), SP(RESOLVED, 1, 0), SP(UDEVD, 1, 0) }, 6 },
    /* Boss */
    { { SP(BOSS, 1, 0) }, 1 },
};

const char *const WAVE_NAMES[NUM_WAVES] = {
    "COLD BOOT", "LOGIN PROMPT", "CABLE DAMAGE", "QUIET NODES", "EVENT LOOP",
    "MIGRATION", "THE VOTE", "NAME CONFUSION", "LEGACY MODE", "THE HOLDOUTS",
    "THE OTHER SESSION", "LAST CALL",
    "THE INFINITE BOOT", "SCHEDULED TASKS", "EVERYTHING IS A UNIT", "DEPENDENCY HELL",
    "OUT OF MEMORY", "RESTART LOOP",
    "THE PLAN", "ROOT SHELL", "LAST RUNLEVEL", "SINGLE USER", "BEFORE PID 1",
    "PID 1"
};

const char *const UPG_NAME[UPG_COUNT] = {
    "MORE RAM", "FASTER TICK", "HEAVIER SYSCALL", "LIGHT KERNEL", "QUICK RELOAD"
};
const char *const UPG_DESC[UPG_COUNT] = {
    "+1 max HP, heal 2", "Fire 12% faster", "+20% damage",
    "Move 8% faster", "Special recharges 20% faster"
};
