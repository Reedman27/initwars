#include "codex.h"

#include <stddef.h>

#define E(type, text, tip) { CK_ENEMY, E_##type, NULL, text, tip }
#define H(ch, text, tip)   { CK_HERO, CH_##ch, NULL, text, tip }
#define P(upg, text, tip)  { CK_PATCH, UPG_##upg, NULL, text, tip }

static const CodexEntry UNITS[] = {
    E(JOURNALD, "Drifts around and fires a spread of three shots at you every couple of seconds. It flashes just before it fires.",
      "Clear it early, or keep moving sideways."),
    E(LOGIND, "Wanders slowly and drops barrier walls. Each wall is outlined first, then closes the spot to you for a few seconds.",
      "Leave the outlined spot. It never traps you."),
    E(NETWORKD, "Lays net lines across the arena. Touching a live line slows you down for about a second.",
      "Do not fight inside a net line. Step around it."),
    E(TIMER, "Keeps its distance, then counts down 3-2-1 above its head. At zero it fires a ring of shots and an aimed burst.",
      "Move away from the ring when the countdown starts."),
    E(UDEVD, "A slow wanderer that spawns hardware minions: usb sticks, floppy disks and printers. Up to six minions at once.",
      "Kill it first and the minions stop coming."),
    E(RESOLVED, "Stays at range and conjures flickering fake targets that copy other units. Fakes are harmless and vanish when shot.",
      "Fakes flicker and look dim. Aim at the solid ones."),
    E(DEPTARGET, "Links up to three nearby units. Linked units take half damage and show an amber line until the link breaks.",
      "Kill the dependency.target first, then the rest."),
    E(CRON, "Wanders and, on a schedule, releases a slow ring of eight shots. It flashes just before it fires.",
      "Slip through the gaps in the ring."),
    E(OOMD, "A heavy brute. It chases you slowly, flashes, then lunges in a straight line. Takes a lot of damage to stop.",
      "Sidestep the lunge, then hit it from behind."),
    E(USB, "A fast little chaser released by systemd-udevd. Touching it hurts. You do not need to kill it to clear a wave.",
      "Shoot it as it comes in. It dies to a couple of hits."),
    E(FLOPPY, "Chases you in a wobbly spiral. Released by systemd-udevd. Touching it hurts. Optional to kill.",
      "It zigzags, so wait until it is close."),
    E(PRINTER, "A slow chaser that stops to fire now and then. PC LOAD LETTER. Optional to kill.",
      "It is slow. Shoot it from range."),
    E(FAKE, "A decoy made by systemd-resolved. It mimics another unit, flickers and is harmless. Worth no points.",
      "Ignore it or pop it for free. It never hurts."),
    E(MARKER, "Left behind by a unit marked R (Restart=always). Counts down, then the unit returns once with 60% health.",
      "Shoot the marker before the number hits zero."),
    E(BOSS, "The final boss. It roams the whole upper arena in every direction and fights in four phases: units, links, restarts, and a last stand.",
      "See PHASES in the RULES tab. Keep moving."),
};

static const CodexEntry INITS[] = {
    H(SYSVINIT, "The old veteran. Tough and hits hard, but after the Act II migration notices he weakens (LEGACY MODE).",
      "Slow shots: aim carefully. Save the special."),
    H(RUNIT, "Quiet, efficient, slightly smug. The fastest gun and the fastest feet, but each shot is weak.",
      "The dash makes you briefly untouchable."),
    H(S6, "Precise and analytical. Quick shots and a long-range bullet, and a special that slows time near you.",
      "Stasis slows bullets too. Use it when the screen fills."),
    H(OPENRC, "Organized and adaptable. Balanced stats and a special that clears the arena of every shot, line and barrier.",
      "A panic button for the big barrages."),
    H(BUSYBOX, "Tiny, fearless, absurd. Very little health but a small hitbox, a fast gun and a heal-and-shield special.",
      "Slip between bullets. The shield also heals 1 HP."),
    { CK_GUEST, 0, "Dinit", "A small service supervisor you meet in Act II. Understands dependency order and restarts. Takes PID 1 in the epilogue. Not playable.",
      "Story guest. Supervision, not ownership." },
    { CK_GUEST, 1, "Upstart", "Ubuntu's event-driven init from the earlier days of this story. Appears once in Act I and is not playable.",
      "Story guest. Remember that it tried." },
};

static const CodexEntry PATCHES[] = {
    P(HP, "Raises your maximum health by one and heals two.", "Best taken when low on health."),
    P(FIRE, "Shoots faster. Stacks.", "Pairs well with high-damage characters."),
    P(DMG, "Each shot hits harder. Stacks.", "Shortens boss phases."),
    P(SPEED, "Moves you faster. Stacks.", "Helps most against OOM killers and the boss."),
    P(SPECIAL, "Your special recharges faster. Stacks.", "Strong on SysVinit and BusyBox."),
};

static const CodexEntry RULES[] = {
    { CK_RULE, 0, "BARRIER", "A solid wall dropped by systemd-logind. It flashes as an outline for a moment, then blocks your movement for a few seconds.",
      "Leave before it solidifies." },
    { CK_RULE, 0, "NET LINE", "A line laid by systemd-networkd. It becomes dangerous after a short warning and slows you while you stand on it.",
      "Keep moving. It only slows." },
    { CK_RULE, 0, "DEPENDENCY LINK", "An amber dotted line between a dependency.target and a linked unit. The linked unit takes half damage.",
      "Break the link by killing the target." },
    { CK_RULE, 0, "RESTART=ALWAYS", "A unit with an R badge comes back once after it dies, with 60% health, after a countdown marker.",
      "Shoot the marker to stop it." },
    { CK_RULE, 0, "FLOPPY PICKUP", "Sometimes a defeated unit drops a floppy disk. Walk over it for +1 HP and 100 points.",
      "It vanishes after about ten seconds." },
    { CK_RULE, 0, "MAINTENANCE BREAK", "Every fourth wave begins with a short break: you recover 1 HP (not above your max).",
      "Waves 4, 8, 12, 16, 20 and 24." },
    { CK_RULE, 0, "PATCH LIMITS", "Between waves you pick one of three patches. Every patch can be stacked up to five times (special recharge: three).",
      "Once a patch is capped, pick another." },
    { CK_RULE, 0, "PHASE 1: UNITS", "systemd summons service units and fires aimed bursts. Kill the helpers before they swarm you.",
      "Boss is invulnerable briefly at each phase change." },
    { CK_RULE, 0, "PHASE 2: LINKS", "A dependency.target links the helpers. While the link lives, the boss takes half damage.",
      "Kill the target first, then hit the boss." },
    { CK_RULE, 0, "PHASE 3: RESTARTS", "Restart=always helpers join in and the boss fires a timed radial barrage that flashes before it goes off.",
      "Shoot the markers and slip through the barrage gaps." },
    { CK_RULE, 0, "PHASE 4: SURVIVE", "For about 9 seconds the boss is invulnerable and sprays spirals. Then it is exposed and every shot counts.",
      "Survive, then unload everything." },
};

#define COUNT(a) ((int)(sizeof (a) / sizeof (a)[0]))

const char *codex_tab_name(int tab)
{
    static const char *const names[CX_TAB_COUNT] = { "UNITS", "INIT SYSTEMS", "PATCHES", "RULES" };
    return (tab >= 0 && tab < CX_TAB_COUNT) ? names[tab] : "";
}

int codex_entry_count(int tab)
{
    switch (tab) {
    case CX_UNITS:   return COUNT(UNITS);
    case CX_INITS:   return COUNT(INITS);
    case CX_PATCHES: return COUNT(PATCHES);
    case CX_RULES:   return COUNT(RULES);
    default:         return 0;
    }
}

const CodexEntry *codex_entry(int tab, int idx)
{
    if (idx < 0 || idx >= codex_entry_count(tab)) return NULL;
    switch (tab) {
    case CX_UNITS:   return &UNITS[idx];
    case CX_INITS:   return &INITS[idx];
    case CX_PATCHES: return &PATCHES[idx];
    default:         return &RULES[idx];
    }
}

const char *codex_name(const CodexEntry *e)
{
    if (!e) return "";
    if (e->name) return e->name;
    switch (e->kind) {
    case CK_ENEMY: return (e->ref > 0 && e->ref < E_COUNT) ? ENEMIES[e->ref].name : "";
    case CK_HERO:  return (e->ref >= 0 && e->ref < CH_COUNT) ? CHARS[e->ref].name : "";
    case CK_PATCH: return (e->ref >= 0 && e->ref < UPG_COUNT) ? UPG_NAME[e->ref] : "";
    default:       return "";
    }
}
