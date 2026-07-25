# -*- coding: utf-8 -*-
"""Fix incomplete loc. prefixes and ref fields after tick split."""
from pathlib import Path
import re

root = Path(r"C:\Users\74738\Documents\My Games\ArmaReforgerWorkbench\addons\Realistic-Stamina-System")
loop = root / "scripts/Game/Integration/PlayerBase_UpdateLoop.c"
text = loop.read_text(encoding="utf-8")

# Extract field names from RSS_StaminaTickLocals
m = re.search(r"class RSS_StaminaTickLocals\s*\{(.*?)\n\}", text, re.S)
if not m:
    raise SystemExit("locals class not found")
body = m.group(1)
fields = re.findall(r"^\s+(?:ref\s+)?[\w<>,\s]+\s+(\w+)\s*;", body, re.M)
print("fields", len(fields), fields[:10], "...")

# Add ref to Managed-like types in locals class
managed = {
    "StaminaDrainTickParams",
    "StaminaDrainTickResult",
    "RSS_StatusMetabLogSnapshot",
    "RSS_StaminaDebugOutputParams",
    "GradeCalculationResult",
    "SpeedCalculationResult",
    "WetWeightUpdateResult",
    "SCR_RSS_CriticalPowerModel",
    "SCR_RSS_Settings",
    "SCR_CharacterDamageManagerComponent",
    "IEntity",  # usually not ref in Enforce for IEntity
}


def fix_locals_refs(src: str) -> str:
    def repl_field(mm: re.Match) -> str:
        typ = mm.group(1).strip()
        name = mm.group(2)
        # skip if already ref
        line = mm.group(0)
        if line.lstrip().startswith("ref "):
            return line
        base = typ.split()[-1] if typ else typ
        if base in managed and base not in ("IEntity", "World", "vector", "string"):
            indent = mm.group(0)[: len(mm.group(0)) - len(mm.group(0).lstrip())]
            return f"{indent}ref {typ} {name};"
        return line

    return re.sub(
        r"^(\s+)((?:ref\s+)?[\w]+(?:\s+[\w]+)*)\s+(\w+)\s*;",
        lambda mm: mm.group(0)
        if "ref " in mm.group(0)
        else (
            f"{mm.group(1)}ref {mm.group(2).strip()} {mm.group(3)};"
            if mm.group(2).strip().split()[-1] in managed
            and mm.group(2).strip().split()[-1]
            not in ("IEntity", "World", "vector", "string", "float", "bool", "int")
            else mm.group(0)
        ),
        src,
        flags=re.M,
    )


# Simpler: explicit replacements in locals block
locals_old = m.group(0)
locals_new = locals_old
for typ in (
    "StaminaDrainTickParams",
    "StaminaDrainTickResult",
    "RSS_StatusMetabLogSnapshot",
    "RSS_StaminaDebugOutputParams",
    "GradeCalculationResult",
    "SpeedCalculationResult",
    "WetWeightUpdateResult",
):
    locals_new = re.sub(
        rf"^(\s+){typ}\s+(\w+)\s*;",
        rf"\1ref {typ} \2;",
        locals_new,
        flags=re.M,
    )
text = text.replace(locals_old, locals_new)
loop.write_text(text, encoding="utf-8")
print("updated locals refs")

# Fix bare field names in phase files
names = sorted(fields, key=len, reverse=True)
phase_files = [
    root / "scripts/Game/RSS/Core/SCR_RSS_StaminaTickPhaseA.c",
    root / "scripts/Game/RSS/Core/SCR_RSS_StaminaTickPhaseB.c",
    root / "scripts/Game/RSS/Core/SCR_RSS_StaminaTickPhaseC.c",
]

for pf in phase_files:
    src = pf.read_text(encoding="utf-8")
    lines_out = []
    for line in src.splitlines(True):
        # skip comments
        if line.lstrip().startswith("//"):
            lines_out.append(line)
            continue
        nl = line
        for n in names:
            # replace bare identifier not already prefixed by loc. or m_ or .
            nl = re.sub(rf"(?<![\w.]){re.escape(n)}(?![\w])", f"loc.{n}", nl)
        nl = nl.replace("loc.loc.", "loc.")
        lines_out.append(nl)
    pf.write_text("".join(lines_out), encoding="utf-8")
    print("fixed", pf.name)

# Make phase methods non-protected in case visibility is an issue (optional)
# Actually undefined was because phases failed to compile — should be fine after fix.

# Spot-check remaining bare names from error list
err_names = [
    "staminaPercent",
    "isSprintingNow",
    "isSprintActive",
    "world",
    "currentTimeForExerciseMs",
    "finalSpeedMultiplier",
    "speedToApply",
    "rainWeight",
    "currentWeight",
    "currentWeightWithWet",
    "isSwimming",
    "currentSpeed",
    "gradeResult",
    "phaseNow",
    "effectivePhase",
    "drainParams",
]
for pf in phase_files:
    src = pf.read_text(encoding="utf-8")
    for n in err_names:
        for i, line in enumerate(src.splitlines(), 1):
            if re.search(rf"(?<![\w.]){n}(?![\w])", line) and "loc." not in line.split(n)[0][-4:]:
                # still bare?
                if re.search(rf"(?<!loc\.)(?<![\w.]){n}(?![\w])", line):
                    # false positive on loc.n — check properly
                    pass
        bare = len(re.findall(rf"(?<![\w.]){n}(?![\w])", src))
        locn = src.count(f"loc.{n}")
        # count bare = total word minus those in loc.n — approximate
        # After fix, every occurrence should be loc.n; word n alone shouldn't appear
        for i, line in enumerate(src.splitlines(), 1):
            tmp = line
            tmp = tmp.replace(f"loc.{n}", "")
            if re.search(rf"(?<![\w.]){n}(?![\w])", tmp):
                print(f"STILL BARE {pf.name}:{i}: {line.strip()[:100]}")
