"""Validate data/game.json so broken content never reaches players.

Checks the same rules the game engines rely on. Run: python tools/check_data.py
"""
import json
import math
import sys
from collections import Counter
from pathlib import Path

PATH = Path(__file__).resolve().parent.parent / "data" / "game.json"
SLOTS = ("morning", "afternoon", "night")
PHASES = ("early", "late")
ENDING_KEYS = ("true", "noMoney", "noLove", "broke")

errors = []


def err(msg):
    errors.append(msg)


def check_outcome(o, where, lovers):
    if not isinstance(o.get("text", ""), str):
        err(f"{where}: text must be a string")
    if not isinstance(o.get("money", 0), int):
        err(f"{where}: money must be an integer")
    for a in o.get("affection", []):
        t = a.get("target")
        if not (t == "all" or (isinstance(t, int) and 0 <= t < lovers)):
            err(f"{where}: invalid affection target {t!r}")
        if not isinstance(a.get("amount"), int):
            err(f"{where}: affection amount must be an integer")


def main():
    try:
        data = json.loads(PATH.read_text(encoding="utf-8"))
    except Exception as e:  # noqa: BLE001
        print(f"{PATH}: {e}")
        return 1

    s = data["settings"]
    lovers = len(data["lovers"])
    for key in ENDING_KEYS:
        if key not in data["endings"]:
            err(f"endings: missing {key}")
    for d in data["difficulties"]:
        if not 0 < d["eventPoolRatio"] <= 1:
            err(f"difficulty {d['name']}: eventPoolRatio must be in (0, 1]")
        for key in ("lossMultiplier", "gainMultiplier"):
            if not isinstance(d.get(key, 1), (int, float)) or d.get(key, 1) < 0:
                err(f"difficulty {d['name']}: {key} must be a number >= 0")
        if not isinstance(d.get("minAffectionDelta", 0), int):
            err(f"difficulty {d['name']}: minAffectionDelta must be an integer")

    # How many turns each (phase, slot) pool must cover.
    need = Counter()
    for turn in range(1, s["endingTurn"]):
        phase = "late" if turn >= s["lateGameStartTurn"] else "early"
        need[(phase, SLOTS[(turn - 1) % 3])] += 1

    # Gender tokens: {P:a|b}, {L:a|b|c}, {word}. {color} and {player} are filled in by the engine.
    import re

    n_player = len(data["genders"]["player"])
    n_lover = len(data["genders"]["lover"])
    words = data.get("genderWords", {})
    for name, w in words.items():
        p_ids = [g["id"] for g in data["genders"]["player"]]
        l_ids = [g["id"] for g in data["genders"]["lover"]]
        keys = {"player": p_ids, "lover": l_ids, "lover+player": [f"{l}:{p}" for l in l_ids for p in p_ids]}.get(w.get("by"))
        if keys is None:
            err(f"genderWords.{name}: by must be player, lover or lover+player")
        else:
            for k in keys:
                if k not in w.get("values", {}):
                    err(f"genderWords.{name}: missing value for {k}")

    def check_tokens(text, where):
        for inner in re.findall(r"\{([^{}]*)\}", text):
            if inner.startswith("P:") and len(inner[2:].split("|")) != n_player:
                err(f"{where}: {{{inner}}} needs {n_player} options")
            elif inner.startswith("L:") and len(inner[2:].split("|")) != n_lover:
                err(f"{where}: {{{inner}}} needs {n_lover} options")
            elif not inner.startswith(("P:", "L:")) and inner not in words and inner not in ("color", "player"):
                err(f"{where}: unknown token {{{inner}}}")

    def walk(value, where):
        if isinstance(value, str):
            check_tokens(value, where)
        elif isinstance(value, dict):
            for k, v in value.items():
                if k not in ("ui", "genderWords"):
                    walk(v, f"{where}.{k}")
        elif isinstance(value, list):
            for i, v in enumerate(value):
                walk(v, f"{where}[{i}]")

    walk({k: v for k, v in data.items() if k not in ("ui", "genderWords", "genders")}, "data")

    ids = set()
    for job in data["jobs"]:
        jid = job["id"]
        if job.get("confess") is not None and len(job["confess"]) != lovers:
            err(f"job {jid}: confess needs {lovers} lines")
        for key in job.get("endings", {}):
            if key not in ENDING_KEYS:
                err(f"job {jid}: unknown ending key {key}")
        pools = Counter()
        for ev in job["events"]:
            where = f"event {ev.get('id')} ({jid})"
            if ev["id"] in ids:
                err(f"{where}: duplicate id")
            ids.add(ev["id"])
            if ev.get("slot") not in SLOTS:
                err(f"{where}: bad slot {ev.get('slot')!r}")
            if ev.get("phase") not in PHASES:
                err(f"{where}: bad phase {ev.get('phase')!r}")
            if "choices" in ev:
                for k in ("y", "n"):
                    if k not in ev["choices"]:
                        err(f"{where}: missing choice {k}")
                    else:
                        check_outcome(ev["choices"][k], f"{where} choice {k}", lovers)
            elif "outcome" in ev:
                check_outcome(ev["outcome"], f"{where} outcome", lovers)
            else:
                err(f"{where}: needs choices or outcome")
            pools[(ev.get("phase"), ev.get("slot"))] += 1
        # Warn (not fail) when a pool is smaller than the turns it covers: events will repeat.
        for d in data["difficulties"]:
            for key, n in need.items():
                usable = max(1, math.ceil(pools[key] * d["eventPoolRatio"] - 1e-9)) if pools[key] else 0
                if usable < n:
                    print(f"warning: job {jid} {d['name']} {key[0]} {key[1]}: {usable} events for {n} turns (repeats)")

    for e in errors:
        print(f"error: {e}")
    total = sum(len(j["events"]) for j in data["jobs"])
    print(f"{len(data['jobs'])} jobs, {total} events, {len(errors)} errors")
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
