# -*- coding: utf-8 -*-
"""Split UpdateSpeedBasedOnStamina into 3 modded-class phase files for ICE relief."""
from pathlib import Path
import re

root = Path(r"C:\Users\74738\Documents\My Games\ArmaReforgerWorkbench\addons\Realistic-Stamina-System")
backup = (root / "tools/bisect_backup/PlayerBase_UpdateLoop.c.full").read_text(encoding="utf-8")
lines = backup.splitlines(True)

start = None
for i, l in enumerate(lines):
    if "void UpdateSpeedBasedOnStamina()" in l:
        start = i
        break

depth = 0
end = None
begun = False
for j in range(start, len(lines)):
    for ch in lines[j]:
        if ch == "{":
            depth += 1
            begun = True
        elif ch == "}":
            depth -= 1
            if begun and depth == 0:
                end = j
                break
    if end is not None:
        break

body_lines = lines[start : end + 1]
# body_lines[0]=signature, [1]=opening brace line or brace on same line
if "{" in body_lines[0]:
    # signature { on same line — rare
    inner_lines = body_lines[1:-1]
else:
    inner_lines = body_lines[2:-1]


def find_inner(substr):
    for i, l in enumerate(inner_lines):
        if substr in l:
            return i
    raise SystemExit("missing " + substr)


s1 = find_inner("bool isSwimming = SCR_RSS_SwimmingStateManager.IsSwimming(this);")
s2 = find_inner("StaminaDrainTickParams drainParams = new StaminaDrainTickParams();")
phase_a = "".join(inner_lines[:s1])
phase_b = "".join(inner_lines[s1:s2])
phase_c = "".join(inner_lines[s2:])
print("phase bytes", len(phase_a.encode()), len(phase_b.encode()), len(phase_c.encode()))

# DTOs from backup (everything before modded class)
dto = backup.split("modded class SCR_CharacterControllerComponent")[0]

# Remaining methods after UpdateSpeedBasedOnStamina (LoopStart etc.) from backup
after = "".join(lines[end + 1 :])
# Strip leading whitespace junk; keep from next method
rest_modded = after  # still inside modded class until final }
# backup structure: after UpdateSpeed method, more methods, then closing }
# Find last closing brace of modded class
rest_modded = rest_modded.rstrip()
if rest_modded.endswith("}"):
    # remove class closing brace — UpdateLoop stub will close? 
    # We'll put LoopStart etc. in UpdateLoop thin file
    rest_inner = rest_modded[:-1]

# Rewrite phases: replace top-level locals with loc-> fields
# Collect declarations at 8-space indent
decl_re = re.compile(
    r"^(\s*)(float|bool|int|vector|string|IEntity|World|"
    r"SpeedCalculationResult|GradeCalculationResult|WetWeightUpdateResult|"
    r"StaminaDrainTickParams|StaminaDrainTickResult|"
    r"RSS_StatusMetabLogSnapshot|RSS_StaminaDebugOutputParams|"
    r"SCR_CharacterDamageManagerComponent|SCR_RSS_CriticalPowerModel|SCR_RSS_Settings"
    r")\s+(\w+)\b(.*)$"
)

# Identify which names are declared at method top-level (8 spaces / 2 tabs)
top_names = []
for l in inner_lines:
    m = decl_re.match(l.rstrip("\n"))
    if not m:
        continue
    indent = m.group(1).replace("\t", "    ")
    if len(indent) == 8:
        top_names.append(m.group(3))

# unique
seen = set()
top_unique = []
for n in top_names:
    if n not in seen:
        seen.add(n)
        top_unique.append(n)
print("top locals", len(top_unique))

# Build locals class fields — use string type for all as loosely typed? No, need types from first decl
name_to_type = {}
for l in inner_lines:
    m = decl_re.match(l.rstrip("\n"))
    if not m:
        continue
    indent = m.group(1).replace("\t", "    ")
    if len(indent) != 8:
        continue
    name_to_type[m.group(3)] = m.group(2)

fields_src = []
for n in top_unique:
    fields_src.append(f"    {name_to_type[n]} {n};\n")

locals_cls = (
    "//! Cross-phase scratch for stamina tick (ICE split of UpdateSpeedBasedOnStamina)\n"
    "class RSS_StaminaTickLocals\n{\n"
    + "".join(fields_src)
    + "}\n"
)


def rewrite_phase(src: str) -> str:
    """Rewrite top-level decls and uses of top locals to loc.NAME."""
    out_lines = []
    for l in src.splitlines(True):
        m = decl_re.match(l.rstrip("\n"))
        if m:
            indent = m.group(1).replace("\t", "    ")
            name = m.group(3)
            rest = m.group(4)
            if len(indent) == 8 and name in seen:
                # float foo = 1.0;  ->  loc.foo = 1.0;
                # float foo; -> skip or loc.foo = 0
                rest_s = rest.strip()
                if rest_s.startswith("="):
                    out_lines.append(f"{m.group(1)}loc.{name} {rest_s}\n")
                elif rest_s.startswith(";"):
                    # bare decl — zero init optional, skip
                    continue
                elif "(" in rest_s:
                    # float foo = Foo(); already in rest with =
                    out_lines.append(f"{m.group(1)}loc.{name}{rest}\n")
                else:
                    out_lines.append(f"{m.group(1)}loc.{name}{rest}\n")
                continue
        # replace word-boundary uses of top locals — careful not to replace loc.foo
        nl = l
        for n in sorted(seen, key=len, reverse=True):
            nl = re.sub(rf"(?<![\w.]){n}(?![\w])", f"loc.{n}", nl)
        # fix double loc.loc.
        nl = nl.replace("loc.loc.", "loc.")
        out_lines.append(nl)
    return "".join(out_lines)


pa = rewrite_phase(phase_a)
pb = rewrite_phase(phase_b)
pc = rewrite_phase(phase_c)

hdr = (
    "//! ICE workaround: phase of UpdateSpeedBasedOnStamina "
    "(split so no single .c carries the full tick AST).\n"
)

def write_phase(path: Path, method: str, body: str):
    content = (
        hdr
        + "modded class SCR_CharacterControllerComponent\n{\n"
        + f"    //! @return false = early-out (caller should CallLater and return)\n"
        + f"    protected bool {method}(RSS_StaminaTickLocals loc)\n"
        + "    {\n"
        + body
        + "        return true;\n"
        + "    }\n"
        + "}\n"
    )
    # Phase A may contain early returns that should return false for orchestrator
    # Replace bare `return;` inside phase with `return false;` and final success true
    # Careful: only method-level early exits
    
    path.write_text(content, encoding="utf-8")
    print("wrote", path.name, path.stat().st_size)

# Fix phase A early returns: `return;` -> `return false;`
pa_fixed = re.sub(r"(?m)^(\s+)return;\s*$", r"\1return false;", pa)
# Phase A also has `return;` after CallLater — those should be false
pb_fixed = re.sub(r"(?m)^(\s+)return;\s*$", r"\1return false;", pb)
pc_fixed = re.sub(r"(?m)^(\s+)return;\s*$", r"\1return false;", pc)
# Phase C originally ended with CallLater — orchestrator schedules; strip trailing CallLater block
pc_fixed = re.sub(
    r"\n\s*m_bRssStaminaLoopActive = true;\s*\n\s*GetGame\(\)\.GetCallqueue\(\)\.CallLater\([^\n]+\n\s*this\);\s*\n?\s*$",
    "\n",
    pc_fixed,
)

out_dir = root / "scripts/Game/RSS/Core"
write_phase(out_dir / "SCR_RSS_StaminaTickPhaseA.c", "RSS_StaminaTickPhaseA", pa_fixed)
write_phase(out_dir / "SCR_RSS_StaminaTickPhaseB.c", "RSS_StaminaTickPhaseB", pb_fixed)
write_phase(out_dir / "SCR_RSS_StaminaTickPhaseC.c", "RSS_StaminaTickPhaseC", pc_fixed)

# Locals + DTOs + thin UpdateLoop
loop = (
    "//! Thin UpdateLoop orchestrator (ICE: full tick split into RSS_StaminaTickPhaseA/B/C).\n"
    + dto
    + locals_cls
    + "\nmodded class SCR_CharacterControllerComponent\n{\n"
    + "    void UpdateSpeedBasedOnStamina()\n"
    + "    {\n"
    + "        if (m_bIsDeleted)\n"
    + "            return;\n"
    + "        IEntity owner = GetOwner();\n"
    + "        if (!owner)\n"
    + "        {\n"
    + "            m_bIsDeleted = true;\n"
    + "            RSS_NotifyEntityDeleting();\n"
    + "            return;\n"
    + "        }\n"
    + "        if (!GetGame())\n"
    + "        {\n"
    + "            m_bIsDeleted = true;\n"
    + "            return;\n"
    + "        }\n"
    + "\n"
    + "        RSS_StaminaTickLocals loc = new RSS_StaminaTickLocals();\n"
    + "        if (!RSS_StaminaTickPhaseA(loc))\n"
    + "            return;\n"
    + "        if (!RSS_StaminaTickPhaseB(loc))\n"
    + "            return;\n"
    + "        RSS_StaminaTickPhaseC(loc);\n"
    + "\n"
    + "        m_bRssStaminaLoopActive = true;\n"
    + "        GetGame().GetCallqueue().CallLater(SCR_PlayerBaseLoop.Tick, GetSpeedUpdateIntervalMs(), false, this);\n"
    + "    }\n"
    + "\n"
)

# Append LoopStart / Ensure / Debug from backup rest
# rest after UpdateSpeed method
rest = "".join(lines[end + 1 :])
# rest still has methods and final }
# remove final closing brace of class — we'll add with loop
if rest.rstrip().endswith("}"):
    rest = rest.rstrip()[:-1] + "\n"

loop_path = root / "scripts/Game/Integration/PlayerBase_UpdateLoop.c"
loop_path.write_text(loop + rest + "}\n", encoding="utf-8")
print("wrote UpdateLoop", loop_path.stat().st_size)

# Verify braces
for rel in [
    "scripts/Game/Integration/PlayerBase_UpdateLoop.c",
    "scripts/Game/RSS/Core/SCR_RSS_StaminaTickPhaseA.c",
    "scripts/Game/RSS/Core/SCR_RSS_StaminaTickPhaseB.c",
    "scripts/Game/RSS/Core/SCR_RSS_StaminaTickPhaseC.c",
]:
    tt = (root / rel).read_text(encoding="utf-8")
    print(rel, "brace", tt.count("{") - tt.count("}"), "bytes", (root / rel).stat().st_size)
