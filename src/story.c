#include "story.h"

#define L(who, txt, col, pause) { who, txt, col, pause }

static const StoryLine ACT1[] = {
    L("",         "[ OK ] Reached runlevel 3. Welcome to the old machine.", C_GREEN, 0),
    L("SysVinit", "Another day, another boot. Worked fine before everybody got ideas.", C_LTGREY, 20),
    L("runit",    "Three scripts. Whole system. You're welcome.", C_BGREEN, 10),
    L("s6",       "Supervision tree nominal. Please stop touching it.", C_CYAN, 10),
    L("OpenRC",   "I am reorganizing the runlevels. Do not ask which ones.", C_AMBER, 10),
    L("BusyBox",  "I run this whole machine on three bytes and a paperclip.", C_WHITE, 10),
    L("",         "*screen flickers*", C_GREY, 40),
    L("systemd",  "ACCEPTED AS DEFAULT.", C_RED, 30),
    L("SysVinit", "Huh. New manager. Probably harmless.", C_LTGREY, 30),
    L("",         "WARNING: UNKNOWN SERVICE UNITS ENTERING THE ARENA.", C_RED, 20),
};

static const StoryLine TRACE[] = {
    L("",         "[BOOT TRACE] A second login is still alive behind the arena.", C_GREEN, 0),
    L("runit",    "That process tree is old, but it never stopped checking its children.", C_BGREEN, 20),
    L("s6",       "A supervisor must know which failure belongs to which dependency.", C_CYAN, 15),
    L("OpenRC",   "Then we map the real startup order. No guessing from filenames.", C_AMBER, 15),
    L("BusyBox",  "Can we map it after lunch? I found a working floppy.", C_WHITE, 15),
    L("SysVinit", "Keep the trace. One day somebody will need to know how this began.", C_LTGREY, 20),
};

static const StoryLine BRIDGE[] = {
    L("",         "DEVICE TABLE: the east bridge has lost its network service.", C_AMBER, 0),
    L("runit",    "I'll keep the essential services alive while we cross.", C_BGREEN, 15),
    L("s6",       "The logger depends on the device layer. Stop it cleanly; restart it after.", C_CYAN, 15),
    L("OpenRC",   "A startup order is a contract. Break it and the whole room notices.", C_AMBER, 15),
    L("SysVinit", "We used to call this a runlevel. A whole room of services, one switch.", C_LTGREY, 20),
    L("",         "A maintenance terminal blinks: USER SERVICE MANAGER STILL ACTIVE.", C_GREEN, 25),
};

static const StoryLine QUIET[] = {
    L("",         "BOOT LOG: Fedora (2011) and Arch (2012) boot on systemd now. Those terminals went quiet first.", C_AMBER, 0),
    L("OpenRC",   "They didn't crash. They were reassigned. That is worse.", C_AMBER, 20),
    L("runit",    "Nobody asked the rest of us. Not even to file a complaint.", C_BGREEN, 15),
    L("s6",       "I counted every terminal that went dark. The number is not zero.", C_CYAN, 15),
    L("SysVinit", "Two distributions. We have lost bigger corridors than that.", C_LTGREY, 20),
    L("BusyBox",  "I'll keep an eye on the ones still lit. They look nervous.", C_WHITE, 15),
};

static const StoryLine UPSTART[] = {
    L("",         "[ OK ] EVENT-BASED INIT DETECTED: UPSTART. HOME: UBUNTU.", C_GREEN, 0),
    L("Upstart",  "Hello. I start jobs when things happen. Network up. Disk mounted. Door opens.", C_AMBER, 20),
    L("s6",       "Event-driven init. That is a real design, not a toy.", C_CYAN, 15),
    L("Upstart",  "Ubuntu has booted on me since 2006. Lately nobody asks how I am doing.", C_AMBER, 20),
    L("SysVinit", "Stay near the exits, friend. The wind is changing.", C_LTGREY, 20),
    L("Upstart",  "I know. Please remember that I tried.", C_AMBER, 25),
};

static const StoryLine ACT2[] = {
    L("",         "[WARN] Corridor 3 offline. Terminal 7 inactive.", C_AMBER, 0),
    L("",         "BOOT LOG: 2014. Debian votes on its default init. Red Hat, SUSE and Ubuntu are all deciding too.", C_GREEN, 20),
    L("SysVinit", "Quiet in here, isn't it. Used to be crowded.", C_LTGREY, 30),
    L("",         "[WARN] Enemy waves growing. Old terminals going dark.", C_AMBER, 20),
};

static const StoryLine VOTE[] = {
    L("",         "DEBIAN TECHNICAL COMMITTEE: SYSTEMD CHOSEN AS THE NEXT DEFAULT. THE VOTE WAS CLOSE.", C_AMBER, 0),
    L("SysVinit", "Debian. My oldest neighbour.", C_LTGREY, 25),
    L("runit",    "Close is not the same as lost. The rest of us stay in the archive.", C_BGREEN, 15),
    L("OpenRC",   "Packages stay. The default moves. That is how it always begins.", C_AMBER, 15),
    L("BusyBox",  "I'm still in the initramfs. I'm in everything. I'm basically furniture.", C_WHITE, 15),
};

static const StoryLine REDHAT[] = {
    L("",         "MIGRATION NOTICE: RHEL 7 -> SYSTEMD (JUNE 2014)", C_AMBER, 20),
    L("SysVinit", "Red Hat, too? Huh. Guess that really is the direction things are going.", C_LTGREY, 30),
    L("",         "MIGRATION NOTICE: SLE 12 -> SYSTEMD (OCT 2014)", C_AMBER, 25),
    L("",         "MIGRATION NOTICE: UBUNTU -> SYSTEMD (FEB 2015)", C_AMBER, 25),
    L("SysVinit", "SUSE. Ubuntu. All inside a year.", C_LTGREY, 30),
    L("SysVinit", "I used to be the first thing that woke up when a machine came alive. Funny how quickly a job becomes history.", C_LTGREY, 50),
    L("SysVinit", "Right. Enough reminiscing. That boot sequence isn't going to finish itself.", C_LTGREY, 40),
    L("",         "[ WARN ] Legacy support ending. Units incoming.", C_AMBER, 15),
};

static const StoryLine HOLDOUTS[] = {
    L("",         "BOOT LOG: Devuan forks Debian in 2014 to keep init choice. Void, Alpine and Gentoo never switched.", C_GREEN, 0),
    L("runit",    "Void still boots on my scripts. I'm told people enjoy them.", C_BGREEN, 20),
    L("OpenRC",   "Alpine and Gentoo start on me. Small rooms, tidy shelves.", C_AMBER, 15),
    L("SysVinit", "Devuan. They forked a whole distribution to keep me on the guest list.", C_LTGREY, 25),
    L("s6",       "Then the fight isn't lost. It just moved to the small rooms.", C_CYAN, 15),
    L("BusyBox",  "The small rooms are my favourite rooms.", C_WHITE, 15),
};

static const StoryLine DINIT[] = {
    L("",         "[TTY 4] USER INSTANCE ACTIVE. SYSTEM PID 1: SYSTEMD.", C_GREEN, 0),
    L("Dinit",    "I'm Dinit. Not the machine's PID 1. Just this login session's supervisor.", C_CYAN, 20),
    L("OpenRC",   "You can supervise a session without claiming the whole boot?", C_AMBER, 15),
    L("Dinit",    "That's the idea. Start independent services in parallel; wait for what they need.", C_CYAN, 15),
    L("s6",       "And when a supervised process disappears?", C_CYAN, 15),
    L("Dinit",    "I track each process and rate-limit its restarts.", C_CYAN, 15),
    L("Dinit",    "If a dependency fails, I roll back its dependents, then restore them when it is ready.", C_CYAN, 20),
    L("BusyBox",  "So you're a dependency map with a very patient babysitter.", C_WHITE, 15),
    L("Dinit",    "A small service supervisor. Please don't make it weird.", C_CYAN, 15),
    L("SysVinit", "You have been here the whole time?", C_LTGREY, 15),
    L("Dinit",    "Only in this session. I was never the one holding the machine together.", C_CYAN, 20),
};

static const StoryLine ACT3[] = {
    L("systemd",  "NO MORE PRETENDING. EVERYTHING IS A UNIT NOW.", C_RED, 0),
    L("",         "[FAILED] Dependency cycle detected. Dependency cycle detected.", C_RED, 20),
    L("runit",    "Fine. We fight.", C_BGREEN, 20),
    L("s6",       "Philosophies differ. Objectives align.", C_CYAN, 10),
    L("OpenRC",   "The machine belongs to its users.", C_AMBER, 10),
    L("BusyBox",  "One manager does not own a machine. Three bytes agree.", C_WHITE, 10),
    L("SysVinit", "Stay close. I know the old corridors.", C_LTGREY, 30),
};

static const StoryLine SCOPE[] = {
    L("",         "[ INFO ] logind, networkd, resolved and timer units online. All of them. At once.", C_GREEN, 0),
    L("OpenRC",   "Those used to be separate tools. Now they march in the same parade.", C_AMBER, 20),
    L("runit",    "Logins, networking, names, timers. All wearing the same badge.", C_BGREEN, 15),
    L("Dinit",    "Parts that stay separate can be replaced separately. That is a feature.", C_CYAN, 20),
    L("BusyBox",  "I tried to make a joke about it. They turned it into a unit file.", C_WHITE, 15),
    L("SysVinit", "Whatever they call it, it still has a health bar.", C_LTGREY, 20),
};

static const StoryLine LOOP[] = {
    L("systemd",  "ALL PATHS MUST LEAD THROUGH ME. ONE MANAGER. ONE ANSWER.", C_RED, 0),
    L("",         "[FAILED] service.target waits for network.target; network waits for service.", C_RED, 20),
    L("Dinit",    "A dependency graph is a map, not a crown. It tells us what needs what.", C_CYAN, 20),
    L("s6",       "Break the cycle at the missing edge. Recover outward from the dependency.", C_CYAN, 15),
    L("runit",    "I'll keep the unrelated services running. They don't need to fall with it.", C_BGREEN, 15),
    L("OpenRC",   "Then we repair only what the failure actually touched.", C_AMBER, 15),
    L("SysVinit", "I've got the old fallback path. It still answers.", C_LTGREY, 20),
};

static const StoryLine PLAN[] = {
    L("Dinit",    "Here is the whole graph. Every unit he runs hangs from one root: PID 1.", C_CYAN, 0),
    L("s6",       "Cut the branches carefully and the root has to answer for them.", C_CYAN, 15),
    L("runit",    "I'll keep the lights on behind us. A supervised service comes back.", C_BGREEN, 15),
    L("OpenRC",   "I'll reorder what we can and stop what we can't. Nobody starts twice.", C_AMBER, 15),
    L("BusyBox",  "I'll carry the floppies. Healing is a feature. Also, they look cool.", C_WHITE, 15),
    L("SysVinit", "And I'll walk point. I know the passages. I helped dig most of them.", C_LTGREY, 20),
    L("",         "[ OK ] PLAN ACCEPTED. DESTINATION: PID 1.", C_GREEN, 20),
};

static const StoryLine DESCENT[] = {
    L("",         "[ INFO ] LEAVING USERSPACE. DESCENDING TOWARD THE KERNEL BOUNDARY.", C_GREEN, 0),
    L("systemd",  "YOU ARE ALREADY INSIDE MY UNIT. ALL OF THIS IS MINE.", C_RED, 25),
    L("runit",    "Dramatic. Is he always like this?", C_BGREEN, 15),
    L("s6",       "He has a unit file for his own ego.", C_CYAN, 15),
    L("SysVinit", "Keep moving. The last corridor is the old maintenance route. Still on my map.", C_LTGREY, 20),
};

static const StoryLine LAST_RUNLEVEL[] = {
    L("",         "LEGACY ROUTE FOUND. ONE FINAL RUNLEVEL IS AVAILABLE.", C_AMBER, 0),
    L("SysVinit", "I can hold this old route open while the others get through.", C_LTGREY, 25),
    L("runit",    "You don't have to be PID 1 to matter.", C_BGREEN, 15),
    L("SysVinit", "I know. I just wanted to see the machine boot one more time.", C_LTGREY, 30),
    L("Dinit",    "After the handoff, I can run as the system init. The services still stay separate.", C_CYAN, 20),
    L("SysVinit", "Then start them together when you can, and in the right order when you must.", C_LTGREY, 25),
    L("BusyBox",  "I'll keep the old scripts in the archive. Nobody gets thrown in the bin.", C_WHITE, 15),
    L("",         "[ OK ] LEGACY ROUTE HELD. FINAL ARENA AHEAD.", C_GREEN, 20),
};

static const StoryLine EVE[] = {
    L("",         "[ OK ] ONE ROOM LEFT BEFORE THE FINAL ARENA.", C_GREEN, 0),
    L("runit",    "Three scripts, one supervisor and a lot of nerve. I call that ready.", C_BGREEN, 20),
    L("s6",       "Odds of success: unclear. Odds of regret if we do not try: certain.", C_CYAN, 15),
    L("OpenRC",   "Whatever happens, we leave the machine in a state somebody can read.", C_AMBER, 15),
    L("BusyBox",  "Group photo? I brought a floppy.", C_WHITE, 15),
    L("SysVinit", "Friends. It has been a good boot.", C_LTGREY, 30),
    L("Dinit",    "At PID 1 I will only offer. The machine decides. Nobody takes it by force.", C_CYAN, 20),
};

static const StoryLine BOSS[] = {
    L("SysVinit", "Maintenance passage 7. Nobody's used it in years. Follow me.", C_LTGREY, 0),
    L("",         "ACCESS GRANTED: FINAL ARENA. PID 1.", C_GREEN, 20),
    L("Dinit",    "I can take the first process when this is over. If the machine chooses it.", C_CYAN, 20),
    L("systemd",  "I AM PID 1. I AM THE MACHINE.", C_RED, 30),
    L("SysVinit", "Funny. I remember when that was my line.", C_LTGREY, 40),
    L("Dinit",    "You manage services. You do not have to become every service.", C_CYAN, 20),
};

static const StoryLine EPILOGUE[] = {
    L("",         "[ OK ] SYSTEMD UNIT STORM STOPPED. DEPENDENCY GRAPH STABLE.", C_GREEN, 0),
    L("Dinit",    "PID 1 accepted. Starting independent services after their dependencies are ready.", C_CYAN, 20),
    L("runit",    "And the rest of us still get to do our jobs?", C_BGREEN, 15),
    L("Dinit",    "That's the point. Supervision, not ownership. Logs, devices, and networks stay themselves.", C_CYAN, 20),
    L("SysVinit", "Then this is my last runlevel. I've had a good boot.", C_LTGREY, 30),
    L("",         "[ OK ] sysvinit: final shutdown complete. exit status 0.", C_AMBER, 35),
    L("Dinit",    "I'll keep the old runlevel notes. Nothing gets erased just because it's old.", C_CYAN, 25),
    L("",         "[ INITWARS ] THE MACHINE IS RUNNING. THE LOG REMAINS.", C_GREEN, 20),
};

#define SCENE(arr, nm, q) { nm, arr, (int)(sizeof arr / sizeof arr[0]), q }
static const Scene SCENES[SC_COUNT] = {
    [SC_ACT1]         = SCENE(ACT1,         "ACT I - THE OLD MACHINE", false),
    [SC_TRACE]        = SCENE(TRACE,        "ACT I - THE BOOT TRACE", false),
    [SC_BRIDGE]       = SCENE(BRIDGE,       "ACT I - THE SERVICE BRIDGE", false),
    [SC_QUIET]        = SCENE(QUIET,        "ACT I - THE QUIET NODES", true),
    [SC_UPSTART]      = SCENE(UPSTART,      "ACT I - THE UPSTART", false),
    [SC_ACT2]         = SCENE(ACT2,         "ACT II - THE MIGRATION", false),
    [SC_VOTE]         = SCENE(VOTE,         "ACT II - THE VOTE", false),
    [SC_REDHAT]       = SCENE(REDHAT,       "ACT II - LEGACY", true),
    [SC_HOLDOUTS]     = SCENE(HOLDOUTS,     "ACT II - THE HOLDOUTS", false),
    [SC_DINIT]        = SCENE(DINIT,        "ACT II - THE OTHER SESSION", false),
    [SC_ACT3]         = SCENE(ACT3,         "ACT III - THE INFINITE BOOT", false),
    [SC_SCOPE]        = SCENE(SCOPE,        "ACT III - SCOPE CREEP", false),
    [SC_LOOP]         = SCENE(LOOP,         "ACT III - THE DEPENDENCY LOOP", false),
    [SC_PLAN]         = SCENE(PLAN,         "ACT III - THE PLAN", false),
    [SC_DESCENT]      = SCENE(DESCENT,      "ACT IV - THE ROOT SHELL", false),
    [SC_LAST_RUNLEVEL]= SCENE(LAST_RUNLEVEL,"ACT IV - THE LAST RUNLEVEL", true),
    [SC_EVE]          = SCENE(EVE,          "ACT IV - BEFORE PID 1", true),
    [SC_BOSS]         = SCENE(BOSS,         "PID 1", false),
    [SC_EPILOGUE]     = SCENE(EPILOGUE,     "EPILOGUE - A DIFFERENT FIRST PROCESS", true),
};

const Scene *story_scene(SceneId id)
{
    return (id >= 0 && id < SC_COUNT) ? &SCENES[id] : &SCENES[0];
}

int story_scene_before_wave(int wave)
{
    switch (wave) {
    case 1:  return SC_ACT1;
    case 2:  return SC_TRACE;
    case 3:  return SC_BRIDGE;
    case 4:  return SC_QUIET;
    case 5:  return SC_UPSTART;
    case 6:  return SC_ACT2;
    case 7:  return SC_VOTE;
    case 10: return SC_HOLDOUTS;
    case 11: return SC_DINIT;
    case 13: return SC_ACT3;
    case 14: return SC_SCOPE;
    case 16: return SC_LOOP;
    case 18: return SC_PLAN;
    case 19: return SC_DESCENT;
    case 21: return SC_LAST_RUNLEVEL;
    case 23: return SC_EVE;
    case 24: return SC_BOSS;
    default: return -1;
    }
}

int story_scene_midwave(int wave)
{
    return wave == 9 ? SC_REDHAT : -1;
}

/* ---- quips ---- */
static const char *const Q_WAVE_T[] = {
    "$ systemctl start trouble", "[ OK ] Started Another Wave.", "Unit entered failed state. Again.",
    "Job for arena.service running (no limit)", "Reached target Mild Panic.", "fsck: 0 files, 0 hope"
};
static const char *const Q_KILL_T[] = {
    "Unit stopped.", "kill -9 accepted.", "Process exited cleanly (for once).",
    "Segfault, but make it theirs.", "core dumped. not ours.", "Removed /run/goon.pid"
};
static const char *const Q_HURT_T[] = {
    "Ow. That was a syscall.", "Kernel says: ouch.", "dmesg: you took damage.",
    "ERR: pain not found. Retrying.", "[FAILED] Keeping HP.", "Permission denied (to dodge)."
};
static const char *const Q_LOW_T[] = {
    "WARNING: HP low. Consider a reboot.", "Swap is full of regret.", "One more hit and it is single-user mode."
};
static const char *const Q_SPECIAL_T[] = {
    "sudo !! (it worked)", "Privilege escalated.", "Running with scissors, as root.",
    "Special loaded. Please do not tell the manager."
};
static const char *const Q_PICKUP_T[] = {
    "FLOPPY: 1.44MB OF HEALING.", "Insert disk 2 of 1. Healed anyway.", "Mounted /dev/fd0. +1 HP."
};
static const char *const Q_JOKE_T[] = {
    "SEGFAULT (core dumped)", "PC LOAD LETTER?", "Out of memory: kill process 1 ...", "FATAL: out of coffee"
};
static const char *const Q_JOKE_REVEAL[] = {
    "...just kidding. Carry on.", "...it is a joke. The printer is fine.", "...a joke. Mostly.", "...joke. Refill and continue."
};
#define ARR(a) (int)(sizeof a / sizeof a[0])

const char *story_quip(QuipKind k, uint32_t *rng, int *last)
{
    const char *const *a = Q_WAVE_T; int n = ARR(Q_WAVE_T);
    switch (k) {
    case Q_WAVE:    a = Q_WAVE_T;    n = ARR(Q_WAVE_T); break;
    case Q_KILL:    a = Q_KILL_T;    n = ARR(Q_KILL_T); break;
    case Q_HURT:    a = Q_HURT_T;    n = ARR(Q_HURT_T); break;
    case Q_LOW:     a = Q_LOW_T;     n = ARR(Q_LOW_T); break;
    case Q_SPECIAL: a = Q_SPECIAL_T; n = ARR(Q_SPECIAL_T); break;
    case Q_PICKUP:  a = Q_PICKUP_T;  n = ARR(Q_PICKUP_T); break;
    case Q_JOKE:    a = Q_JOKE_T;    n = ARR(Q_JOKE_T); break;
    default: break;
    }
    uint32_t x = *rng ? *rng : 0x9e3779b9u;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    *rng = x;
    int i = (int)(x % (uint32_t)n);
    if (last && i == *last) i = (i + 1) % n;
    if (last) *last = i;
    return a[i];
}

/* The reveal line of a joke quip shares the index of the joke itself. */
const char *story_joke_reveal(int idx)
{
    return Q_JOKE_REVEAL[(idx >= 0 && idx < ARR(Q_JOKE_REVEAL)) ? idx : 0];
}

/* ---- fake system text ---- */
const char *const BOOT_LINES[] = {
    "INITWARS BIOS v1.0  (C) 1996 THE LAST PID",
    "Memory test: 640K OK",
    "Detecting init systems...",
    "  sysvinit ... found",
    "  runit    ... found",
    "  s6       ... found",
    "  openrc   ... found",
    "  busybox  ... found",
    "  systemd  ... ACCEPTED AS DEFAULT (?)",
    "Mounting /dev/arcade ... done",
    "All output below is simulated. Your machine is fine.",
};
const int BOOT_LINE_COUNT = ARR(BOOT_LINES);

const char *const PANIC_LINES[] = {
    "Kernel panic - not syncing: Attempted to kill init!",
    "CPU: 0 PID: 1 Comm: init Not tainted 2.0.36",
    "Call Trace:",
    "  dump_stack+0x42/0x6a",
    "  panic+0xff/0x2b0",
    "  do_exit+0x9a1/0xb10",
    "---[ end Kernel panic - not syncing ]---",
    "(it is only a game. your real machine is fine.)",
};
const int PANIC_LINE_COUNT = ARR(PANIC_LINES);

const char *const CREDITS_LINES[] = {
    "INIT WARS: THE LAST PID",
    "",
    "A small love letter to the weird, messy",
    "history of Linux init systems.",
    "",
    "All characters are fictional arcade",
    "versions of real software. Real init",
    "history is far messier than this game.",
    "",
    "Code, sprites, font table and sounds:",
    "original to this project.",
    "No assets from anywhere else.",
    "",
    "systemd is not the villain of real life.",
    "It is the villain of this arcade game.",
    "Dinit's guest role draws on real service",
    "supervision and dependency management.",
    "Real projects are not this melodramatic.",
    "",
    "Thanks for keeping the machine running.",
};
const int CREDITS_LINE_COUNT = ARR(CREDITS_LINES);

const char *const HIDDEN_MESSAGE = "IF YOU CAN READ THIS, YOU STILL HAVE A TERMINAL. KEEP IT.";
