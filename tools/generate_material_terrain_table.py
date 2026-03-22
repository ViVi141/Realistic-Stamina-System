# -*- coding: utf-8 -*-
# 从 tools/EST_AllGameMaterialDensities.csv 生成 SCR_MaterialTerrainTable.c

import os
import re

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CSV_PATH = os.path.join(ROOT, "tools", "EST_AllGameMaterialDensities.csv")
OUT_PATH = os.path.join(
    ROOT,
    "scripts",
    "Game",
    "Components",
    "Stamina",
    "SCR_MaterialTerrainTable.c",
)


def main():
    rows = []
    with open(CSV_PATH, "r", encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("material"):
                continue
            parts = line.split(",")
            if len(parts) < 2:
                continue
            raw_path = parts[0]
            density = float(parts[1])
            key = re.sub(r"^\{[^}]+\}", "", raw_path)
            base = key.split("/")[-1]
            rows.append((base, density))

    lines_out = []
    lines_out.append("// 自动生成：python tools/generate_material_terrain_table.py")
    lines_out.append("// 数据源：tools/EST_AllGameMaterialDensities.csv（basename -> g/cm³）")
    lines_out.append("")
    lines_out.append("class MaterialTerrainTable")
    lines_out.append("{")
    lines_out.append("    protected static ref map<string, float> s_DensityByBasename;")
    lines_out.append("    protected static bool s_bInitialized;")
    lines_out.append("")
    lines_out.append("    static string NormalizeMaterialKey(string raw)")
    lines_out.append("    {")
    lines_out.append("        if (!raw || raw == \"\")")
    lines_out.append("            return \"\";")
    lines_out.append("        array<string> parts = new array<string>();")
    lines_out.append("        raw.Split(\"}\", parts, false);")
    lines_out.append("        if (parts.Count() >= 2)")
    lines_out.append("            return parts[1];")
    lines_out.append("        return raw;")
    lines_out.append("    }")
    lines_out.append("")
    lines_out.append("    static string GetMaterialBasename(string path)")
    lines_out.append("    {")
    lines_out.append("        if (!path || path == \"\")")
    lines_out.append("            return \"\";")
    lines_out.append("        array<string> segs = new array<string>();")
    lines_out.append("        path.Split(\"/\", segs, false);")
    lines_out.append("        int n = segs.Count();")
    lines_out.append("        if (n < 1)")
    lines_out.append("            return \"\";")
    lines_out.append("        return segs[n - 1];")
    lines_out.append("    }")
    lines_out.append("")
    lines_out.append("    static void EnsureInit()")
    lines_out.append("    {")
    lines_out.append("        if (s_bInitialized)")
    lines_out.append("            return;")
    lines_out.append("        s_bInitialized = true;")
    lines_out.append("        s_DensityByBasename = new map<string, float>();")

    for base, density in rows:
        be = base.replace("\\", "\\\\").replace('"', '\\"')
        lines_out.append(
            '        s_DensityByBasename.Insert("%s", %s);' % (be, format(density, ".12g"))
        )

    lines_out.append("    }")
    lines_out.append("")
    lines_out.append("    static float ResolveDensity(GameMaterial material, float ballisticDensity)")
    lines_out.append("    {")
    lines_out.append("        EnsureInit();")
    lines_out.append("        if (!material)")
    lines_out.append("            return ballisticDensity;")
    lines_out.append("        string norm = NormalizeMaterialKey(material.ToString());")
    lines_out.append("        string bn = GetMaterialBasename(norm);")
    lines_out.append("        if (bn != \"\" && s_DensityByBasename.Contains(bn))")
    lines_out.append("            return s_DensityByBasename.Get(bn);")
    lines_out.append("        return ballisticDensity;")
    lines_out.append("    }")
    lines_out.append("")
    lines_out.append("    static int GetEmbeddedEntryCount()")
    lines_out.append("    {")
    lines_out.append("        EnsureInit();")
    lines_out.append("        return s_DensityByBasename.Count();")
    lines_out.append("    }")
    lines_out.append("}")
    lines_out.append("")

    with open(OUT_PATH, "w", encoding="utf-8", newline="\n") as out:
        out.write("\n".join(lines_out))

    print("Wrote", OUT_PATH, "entries", len(rows))


if __name__ == "__main__":
    main()
