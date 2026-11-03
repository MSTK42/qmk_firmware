// SPDX-License-Identifier: GPL-2.0-or-later
#include "mejiro_input.h"
#include <string.h>

static uint64_t bit(uint8_t p) {
    return UINT64_C(1) << p;
}
static uint32_t elapsed(uint32_t now, uint32_t then) {
    return now - then;
}
static bool older(uint32_t a, uint32_t b) {
    return (int32_t)(a - b) < 0;
}
static bool has_hold(const mji_token_t *t) {
    return t->binding.hold_code || t->binding.hold_mods || t->binding.hold_layer != MJI_NO_LAYER;
}

static mji_context_t get_context(mji_engine_t *e, bool live) {
    mji_context_t c      = {0};
    uint32_t      before = e->serial + 1;
    for (;;) {
        int best = -1;
        for (uint8_t i = 0; i < MJI_TOKEN_COUNT; ++i) {
            mji_token_t *t = &e->tokens[i];
            if (t->state != MJI_ACTIVE && t->state != MJI_PREPARED) continue;
            if ((live || e->mato_mode || t->residual) && t->released) continue;
            c.mods |= t->binding.hold_mods;
            if (t->binding.hold_layer != MJI_NO_LAYER && t->activated < before && (best < 0 || t->activated > e->tokens[best].activated)) best = i;
        }
        if (best < 0) break;
        mji_token_t *t = &e->tokens[best];
        if (c.layer_count < MJI_LAYER_COUNT - 1) c.layers[c.layer_count++] = t->binding.hold_layer;
        before = t->activated;
    }
    if (e->mato_mode) {
        // Mato's numbered momentary layers take precedence independently of
        // the order in which their thumbs were pressed. Keep the base last.
        for (uint8_t i = 1; i < c.layer_count; ++i) {
            uint8_t layer = c.layers[i], j = i;
            while (j && c.layers[j - 1] < layer) {
                c.layers[j] = c.layers[j - 1];
                --j;
            }
            c.layers[j] = layer;
        }
    }
    c.layers[c.layer_count++] = e->base_layer;
    if (e->sticky_target >= 0) c.mods |= MJI_SHIFT_MASK & UINT8_C(0x02);
    return c;
}
static mji_context_t context(mji_engine_t *e) {
    return get_context(e, false);
}
static uint8_t output_mods(mji_engine_t *e) {
    // Logical Hold is available immediately for lookup. Publish its modifiers
    // only once used by a successor or held through the profile's threshold.
    uint8_t mods = 0;
    for (uint8_t i = 0; i < MJI_TOKEN_COUNT; ++i) {
        mji_token_t *t = &e->tokens[i];
        if (t->state != MJI_ACTIVE && t->state != MJI_PREPARED) continue;
        if (t->residual && t->released) continue;
        uint32_t end = t->released ? t->ended : e->now;
        uint16_t threshold = e->mato_mode ? e->timing.long_hold_ms : e->timing.combo_ms;
        if (!t->binding.tap || t->used || t->mods_exposed || t->residual || elapsed(end, t->born) >= threshold)
            mods |= t->binding.hold_mods;
    }
    if (e->sticky_target >= 0) mods |= UINT8_C(0x02);
    if (e->output_target >= 0) {
        mji_token_t *t      = &e->tokens[e->output_target];
        mji_stroke_t stroke = e->output_is_hold ? t->hold_stroke : t->tap_stroke;
        mods                = (mods & ~stroke.clear_mods) | stroke.mods;
    }
    return mods;
}
static void sync_mods(mji_engine_t *e) {
    if (e->callbacks.mods) e->callbacks.mods(e->user, output_mods(e));
}
static mji_stroke_t resolve_stroke(mji_engine_t *e, uint16_t code, uint8_t layer, uint8_t mods, bool skip_alt) {
    mji_key_t key = {code, 0};
    if (e->callbacks.decode && !e->callbacks.decode(e->user, code, &key)) return (mji_stroke_t){code, 0, 0};
    mji_layout_config_t config = e->layout;
    if (skip_alt) config.alt_enabled = false;
    mji_stroke_t stroke = mji_layout_resolve(&config, key, layer, mods);
    return e->callbacks.encode ? e->callbacks.encode(e->user, stroke, mods) : stroke;
}
static void resolve_binding(mji_engine_t *e, mji_token_t *t, const mji_context_t *c) {
    t->binding     = e->callbacks.lookup(e->user, t->logical, c);
    t->tap_stroke  = resolve_stroke(e, t->binding.tap, t->binding.layer, c->mods, t->binding.skip_alt);
    t->hold_stroke = resolve_stroke(e, t->binding.hold_code, t->binding.layer, c->mods, t->binding.skip_alt);
}
static void output(mji_engine_t *e, uint8_t source, bool hold, bool pressed) {
    mji_token_t *t      = &e->tokens[source];
    mji_stroke_t stroke = hold ? t->hold_stroke : t->tap_stroke;
    if (!stroke.code) return;
    if (pressed) {
        e->output_target  = source;
        e->output_is_hold = hold;
    }
    uint32_t epoch = e->epoch;
    sync_mods(e);
    e->callbacks.key(e->user, source, stroke.code, pressed, output_mods(e));
    if (epoch != e->epoch) return;
    if (!pressed && e->output_target == source) {
        e->output_target = -1;
        sync_mods(e);
    }
}

void mji_reset(mji_engine_t *e, uint8_t base_layer) {
    e->epoch++;
    e->ignored        = e->physical;
    e->event_count    = 0;
    e->base_layer     = base_layer;
    e->sticky_target  = -1;
    e->output_target  = -1;
    e->sticky_waiting = false;
    e->serial         = 0;
    memset(e->tokens, 0, sizeof(e->tokens));
    memset(e->owner, 0, sizeof(e->owner));
    if (e->callbacks.clear) e->callbacks.clear(e->user);
    sync_mods(e);
}
static void overflow(mji_engine_t *e) {
    e->overflows++;
    mji_reset(e, e->base_layer);
}
static bool enqueue(mji_engine_t *e, uint8_t token, bool pressed) {
    if (e->event_count == MJI_EVENT_COUNT) {
        overflow(e);
        return false;
    }
    e->events[e->event_count++] = (mji_event_t){token, pressed};
    return true;
}
static void collect(mji_engine_t *e) {
    bool referenced[MJI_TOKEN_COUNT] = {0};
    for (uint16_t i = 0; i < e->event_count; ++i)
        referenced[e->events[i].token] = true;
    for (uint8_t i = 0; i < MJI_TOKEN_COUNT; ++i)
        if (!referenced[i] && (e->tokens[i].state == MJI_ENDED || e->tokens[i].state == MJI_CANCELLED)) e->tokens[i].state = MJI_FREE;
}
static int allocate(mji_engine_t *e) {
    collect(e);
    for (uint8_t i = 0; i < MJI_TOKEN_COUNT; ++i)
        if (e->tokens[i].state == MJI_FREE) {
            memset(&e->tokens[i], 0, sizeof(e->tokens[i]));
            e->tokens[i].binding.hold_layer = MJI_NO_LAYER;
            return i;
        }
    overflow(e);
    return -1;
}
static uint8_t first_source(uint64_t sources) {
    for (uint8_t i = 0; i < MJI_PHYSICAL_COUNT; ++i)
        if (sources & bit(i)) return i;
    return 0;
}
static bool single(const mji_token_t *t) {
    return t->sources && !(t->sources & (t->sources - 1));
}
// A visible but unassigned combo must not consume its two ordinary keys.
static bool pair_enabled(mji_engine_t *e, const mji_pair_t *pair) {
    mji_context_t c = context(e);
    mji_binding_t b = e->callbacks.lookup(e->user, pair->logical, &c);
    return b.tap || b.hold_code || b.hold_mods || b.hold_layer != MJI_NO_LAYER;
}
static bool candidate(mji_engine_t *e, uint8_t p) {
    for (uint8_t i = 0; i < e->pair_count; ++i)
        if ((e->pairs[i].a == p || e->pairs[i].b == p) && pair_enabled(e, &e->pairs[i])) return true;
    return false;
}

static void activate(mji_engine_t *e, uint8_t id) {
    mji_token_t *t   = &e->tokens[id];
    e->output_target = -1;
    if (e->sticky_target >= 0) {
        e->sticky_target = -1;
        sync_mods(e);
    }
    bool          prepared = t->state == MJI_PREPARED;
    mji_context_t c        = context(e);
    // Waiting Sticky participates in binding lookup, including shifted layouts.
    if (e->sticky_waiting) c.mods |= UINT8_C(0x02);
    uint32_t epoch = e->epoch;
    if (!prepared && !t->binding_resolved) resolve_binding(e, t, &c);
    if (epoch != e->epoch) return;
    // Every preceding Hold contributes, even if only modifiers and not a layer.
    for (uint8_t i = 0; i < MJI_TOKEN_COUNT; ++i)
        if ((e->tokens[i].state == MJI_ACTIVE || e->tokens[i].state == MJI_PREPARED) && !(e->tokens[i].residual && e->tokens[i].released) && i != id && has_hold(&e->tokens[i])) e->tokens[i].used = true;
    t->mods_exposed = e->mato_mode ? t->mods_exposed : !!(e->physical & ~t->sources);
    t->activated = ++e->serial;
    t->state     = MJI_ACTIVE;
    if (has_hold(t)) {
        sync_mods(e);
        output(e, id, true, true);
    } else {
        if (e->sticky_waiting && t->binding.tap != MJI_STICKY_SHIFT && t->binding.tap) {
            e->sticky_waiting = false;
            e->sticky_target  = id;
        }
        sync_mods(e);
        t->emitted = true;
        if (t->binding.tap == MJI_STICKY_SHIFT) {
            e->sticky_waiting = !e->sticky_waiting;
            t->emitted        = false;
        } else
            output(e, id, false, true);
    }
}
static void finish(mji_engine_t *e, uint8_t id) {
    mji_token_t *t = &e->tokens[id];
    if (t->state != MJI_ACTIVE) return;
    bool     hold  = has_hold(t);
    bool     tap   = hold && !t->used && elapsed(t->ended, t->born) < e->timing.long_hold_ms;
    uint32_t epoch = e->epoch;
    // Keep the target Shift through key-up, then remove only this source.
    if (t->emitted) output(e, id, false, false);
    if (epoch != e->epoch) return;
    t->state = MJI_ENDED;
    if (e->sticky_target == id) e->sticky_target = -1;
    sync_mods(e);
    if (hold) output(e, id, true, false);
    if (epoch != e->epoch || !tap) return;
    if (t->binding.tap == MJI_STICKY_SHIFT) {
        e->sticky_waiting = !e->sticky_waiting;
        return;
    }
    if (e->sticky_waiting && t->binding.tap) {
        e->sticky_waiting = false;
        e->sticky_target  = id;
        sync_mods(e);
    }
    output(e, id, false, true);
    if (epoch != e->epoch) return;
    output(e, id, false, false);
    if (epoch != e->epoch) return;
    if (e->sticky_target == id) e->sticky_target = -1;
    sync_mods(e);
}
static void drain(mji_engine_t *e) {
    while (e->event_count) {
        mji_event_t  event = e->events[0];
        mji_token_t *t     = &e->tokens[event.token];
        if (event.pressed && t->state == MJI_WAIT) break;
        memmove(e->events, e->events + 1, --e->event_count * sizeof(*e->events));
        uint32_t epoch = e->epoch;
        if (event.pressed && (t->state == MJI_READY || t->state == MJI_PREPARED))
            activate(e, event.token);
        else if (!event.pressed)
            finish(e, event.token);
        if (epoch != e->epoch) return;
    }
    collect(e);
}

// Merge two still-unresolved starts. Preserve the older start event in place.
static void merge(mji_engine_t *e, uint8_t a, uint8_t b, uint16_t logical) {
    mji_token_t *x = &e->tokens[a], *y = &e->tokens[b];
    if (older(y->order, x->order)) {
        uint8_t swap = a;
        a            = b;
        b            = swap;
        x            = &e->tokens[a];
        y            = &e->tokens[b];
    }
    x->sources |= y->sources;
    x->logical = logical;
    x->state   = MJI_READY;
    x->binding_resolved = false; // Resolve the combo, not its first letter.
    y->state   = MJI_CANCELLED;
    for (uint8_t p = 0; p < MJI_PHYSICAL_COUNT; ++p)
        if (x->sources & bit(p)) e->owner[p] = a + 1;
}
static int pending_pair(mji_engine_t *e, uint8_t id, bool vertical_only) {
    mji_token_t *t    = &e->tokens[id];
    uint8_t      p    = first_source(t->sources);
    int          best = -1;
    for (uint8_t i = 0; i < e->pair_count; ++i) {
        const mji_pair_t *pair = &e->pairs[i];
        if (vertical_only && pair->priority) continue;
        int other = pair->a == p ? pair->b : pair->b == p ? pair->a : -1;
        if (other < 0 || !e->owner[other] || !pair_enabled(e, pair)) continue;
        uint8_t      j = e->owner[other] - 1;
        mji_token_t *u = &e->tokens[j];
        if (j == id || u->state != MJI_WAIT || u->residual || u->released || !single(u)) continue;
        if (e->mato_mode && u->pressed_layer != t->pressed_layer) continue;
        uint32_t delta = older(u->born, t->born) ? t->born - u->born : u->born - t->born;
        if (delta >= e->timing.combo_ms) continue;
        if (best < 0 || pair->priority < e->pairs[best].priority)
            best = i;
        else if (pair->priority == e->pairs[best].priority) {
            const mji_pair_t *old = &e->pairs[best];
            uint8_t           op  = old->a == p ? old->b : old->a;
            if (older(u->order, e->tokens[e->owner[op] - 1].order)) best = i;
        }
    }
    return best;
}
static bool resolve_pair(mji_engine_t *e, uint8_t id, bool vertical_only) {
    int hit = pending_pair(e, id, vertical_only);
    if (hit < 0) return false;
    const mji_pair_t *pair = &e->pairs[hit];
    merge(e, e->owner[pair->a] - 1, e->owner[pair->b] - 1, pair->logical);
    return true;
}
static void close_token(mji_engine_t *e, uint8_t id, uint32_t now) {
    mji_token_t *t = &e->tokens[id];
    t->released    = true;
    t->ended       = now;
    // A later unresolved start may block logical retirement, but must not
    // prolong an already emitted key into the host's auto-repeat window.
    // Keep the token/context queued; only release its live Tap output now.
    if (t->emitted) {
        t->emitted = false;
        uint32_t epoch = e->epoch;
        output(e, id, false, false);
        if (epoch != e->epoch) return;
    }
    enqueue(e, id, false);
}
static int start_token(mji_engine_t *e, uint64_t sources, uint16_t logical, uint32_t born, bool residual, bool ready) {
    int id = allocate(e);
    if (id < 0) return -1;
    mji_token_t *t = &e->tokens[id];
    t->sources     = sources;
    t->logical     = logical;
    t->born        = born;
    t->order       = ++e->next_order;
    t->residual    = residual;
    t->state       = ready ? MJI_READY : MJI_WAIT;
    if (e->mato_mode) {
        mji_context_t c = context(e);
        if (e->sticky_waiting) c.mods |= UINT8_C(0x02);
        t->pressed_layer = c.layers[0];
        resolve_binding(e, t, &c);
        t->binding_resolved = true;
    }
    for (uint8_t p = 0; p < MJI_PHYSICAL_COUNT; ++p)
        if (sources & bit(p)) e->owner[p] = id + 1;
    if (!residual && !enqueue(e, id, true)) return -1;
    return id;
}

// Mato commits earlier ordinary letters before a different next key can
// change their layer/modifiers. Only a still-eligible partner stays pending.
static int mato_pending_partner(mji_engine_t *e, uint8_t p, uint32_t now) {
    mji_context_t c = context(e);
    for (uint8_t i = 0; i < e->pair_count; ++i) {
        const mji_pair_t *pair = &e->pairs[i];
        int other = pair->a == p ? pair->b : pair->b == p ? pair->a : -1;
        if (other < 0 || !e->owner[other] || !pair_enabled(e, pair)) continue;
        int id = e->owner[other] - 1;
        mji_token_t *t = &e->tokens[id];
        if (t->state == MJI_WAIT && !t->released && !t->residual && single(t) &&
            t->pressed_layer == c.layers[0] && elapsed(now, t->born) < e->timing.combo_ms)
            return id;
    }
    return -1;
}
static void mato_flush_pending(mji_engine_t *e, int except) {
    for (uint8_t i = 0; i < MJI_TOKEN_COUNT; ++i)
        if (i != except && e->tokens[i].state == MJI_WAIT && !e->tokens[i].residual)
            e->tokens[i].state = MJI_READY;
    drain(e);
}

bool mji_init(mji_engine_t *e, const mji_pair_t *pairs, uint8_t pair_count, mji_timing_t timing, uint8_t base_layer, mji_callbacks_t callbacks, void *user) {
    if (!e || !callbacks.lookup || !callbacks.key || !callbacks.mods || !callbacks.clear || !timing.combo_ms || !timing.continuation_ms || timing.long_hold_ms <= timing.continuation_ms || base_layer >= MJI_LAYER_COUNT || (pair_count && !pairs)) return false;
    for (uint8_t i = 0; i < pair_count; ++i)
        if (pairs[i].a >= MJI_PHYSICAL_COUNT || pairs[i].b >= MJI_PHYSICAL_COUNT || pairs[i].a == pairs[i].b) return false;
    memset(e, 0, sizeof(*e));
    e->pairs         = pairs;
    e->pair_count    = pair_count;
    e->timing        = timing;
    e->base_layer    = base_layer;
    e->callbacks     = callbacks;
    e->user          = user;
    e->sticky_target = -1;
    e->output_target = -1;
    return true;
}

void mji_tick(mji_engine_t *e, uint32_t now) {
    e->now = now;
    // Resolve expired initial candidates by their birth order, not token slot.
    for (;;) {
        int oldest = -1;
        for (uint8_t i = 0; i < MJI_TOKEN_COUNT; ++i) {
            mji_token_t *t = &e->tokens[i];
            if (t->state != MJI_WAIT || t->residual || elapsed(now, t->born) < e->timing.combo_ms) continue;
            if (oldest < 0 || older(t->order, e->tokens[oldest].order)) oldest = i;
        }
        if (oldest < 0) break;
        if (!resolve_pair(e, oldest, false)) e->tokens[oldest].state = MJI_READY;
    }
    // Continuations do not block unrelated starts. Once mature, their Hold is
    // available even to an earlier start whose initial combo is still pending.
    for (uint8_t i = 0; i < MJI_TOKEN_COUNT; ++i) {
        mji_token_t *t = &e->tokens[i];
        if (t->state == MJI_WAIT && t->residual && elapsed(now, t->born) >= e->timing.continuation_ms) {
            // Resolve the continuation in its live context, but retain output
            // order behind earlier unresolved starts. Its Hold can already
            // contribute to those starts when they eventually resolve.
            mji_context_t c = get_context(e, true);
            if (e->sticky_waiting) c.mods |= UINT8_C(0x02);
            uint32_t epoch = e->epoch;
            resolve_binding(e, t, &c);
            if (epoch != e->epoch) return;
            t->state     = MJI_PREPARED;
            t->activated = ++e->serial;
            if (!enqueue(e, i, true)) return;
        }
    }
    drain(e);
    sync_mods(e);
}

void mji_event(mji_engine_t *e, uint8_t p, bool pressed, uint32_t now) {
    if (p >= MJI_PHYSICAL_COUNT) return;
    uint64_t mask = bit(p);
    if (!!(e->physical & mask) == pressed) return;
    uint32_t epoch = e->epoch;
    mji_tick(e, now);
    if (pressed)
        e->physical |= mask;
    else
        e->physical &= ~mask;
    if (epoch != e->epoch && pressed) e->ignored |= mask;
    if (e->ignored & mask) {
        if (!pressed) e->ignored &= ~mask;
        return;
    }
    if (pressed) {
        if (e->mato_mode) {
            mato_flush_pending(e, mato_pending_partner(e, p, now));
            if (epoch != e->epoch) return;
            // Any subsequent press chooses a dual-role key's Hold, even when
            // that subsequent key is itself awaiting its combo window.
            for (uint8_t i = 0; i < MJI_TOKEN_COUNT; ++i) {
                mji_token_t *t = &e->tokens[i];
                if (t->state == MJI_ACTIVE && !t->released && has_hold(t)) {
                    t->used = true;
                    t->mods_exposed = true;
                }
            }
        }
        // A new partner of an already confirmed singleton switches immediately,
        // including horizontal pairs and immature residual singletons.
        int hit = -1, source = -1;
        for (uint8_t i = 0; !e->mato_mode && i < e->pair_count; ++i) {
            const mji_pair_t *pair  = &e->pairs[i];
            int               other = pair->a == p ? pair->b : pair->b == p ? pair->a : -1;
            if (other < 0 || !e->owner[other] || !pair_enabled(e, pair)) continue;
            int          id = e->owner[other] - 1;
            mji_token_t *t  = &e->tokens[id];
            if (!single(t) || t->released || (t->state != MJI_ACTIVE && t->state != MJI_READY && t->state != MJI_PREPARED && !t->residual)) continue;
            if (hit < 0 || pair->priority < e->pairs[hit].priority || (pair->priority == e->pairs[hit].priority && older(t->order, e->tokens[source].order))) {
                hit    = i;
                source = id;
            }
        }
        if (hit >= 0) {
            mji_token_t *t       = &e->tokens[source];
            uint64_t     sources = t->sources | mask;
            if (t->residual && t->state == MJI_WAIT)
                t->state = MJI_CANCELLED;
            else
                close_token(e, source, now);
            if (epoch != e->epoch) return;
            start_token(e, sources, e->pairs[hit].logical, now, false, true);
        } else {
            int id = start_token(e, mask, p, now, false, !candidate(e, p));
            if (id >= 0 && e->tokens[id].state == MJI_WAIT) resolve_pair(e, id, true);
        }
    } else if (e->owner[p]) {
        uint8_t      id = e->owner[p] - 1;
        mji_token_t *t  = &e->tokens[id];
        if (e->mato_mode && (t->binding.hold_mods || t->tap_stroke.mods || t->hold_stroke.mods)) {
            // The queued letter must see the modifier which was held at its
            // press, even when the user releases the modifier first.
            mato_flush_pending(e, -1);
            if (epoch != e->epoch) return;
        }
        if (t->state == MJI_WAIT && t->residual) {
            t->state    = MJI_CANCELLED;
            e->owner[p] = 0;
        } else {
            // Match before marking released: this event ends the last overlap.
            if (t->state == MJI_WAIT) {
                if (!resolve_pair(e, id, false)) t->state = MJI_READY;
                id = e->owner[p] - 1;
                t  = &e->tokens[id];
            }
            uint64_t remaining = t->sources & ~mask & e->physical;
            for (uint8_t k = 0; k < MJI_PHYSICAL_COUNT; ++k)
                if (t->sources & bit(k)) e->owner[k] = 0;
            close_token(e, id, now);
            if (epoch != e->epoch) return;
            if (remaining) {
                if (e->mato_mode)
                    e->ignored |= remaining; // Consumed until its physical up.
                else
                    start_token(e, remaining, first_source(remaining), now, true, false);
            }
        }
    }
    // A physical successor may still be waiting for its own combo. Expose
    // already active providers now, without waiting for that resolution.
    for (uint8_t i = 0; i < MJI_TOKEN_COUNT; ++i) {
        mji_token_t *t = &e->tokens[i];
        if (!e->mato_mode && t->state == MJI_ACTIVE && !t->released && (e->physical & ~t->sources))
            t->mods_exposed = true;
    }
    drain(e);
    sync_mods(e);
}

bool mji_set_layout(mji_engine_t *e, const mji_layout_config_t *config) {
    if (!config || config->language > MJI_JAPANESE) return false;
    for (uint8_t i = 0; i < 2; i++)
        if (config->maps[i].count && !config->maps[i].entries) return false;
    mji_layout_config_t selected = *config;
    mji_reset(e, e->base_layer);
    e->layout = selected;
    return true;
}
