#!/usr/bin/env python3
"""Port v3.23.1 archive .c files to v5 scripts/ with class renames."""

from __future__ import annotations

import os
import re
import shutil

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ARCHIVE = os.path.join(ROOT, "archive", "v3.23.1", "scripts", "Game")
DEST = os.path.join(ROOT, "scripts", "Game")

# Old class/file -> new (None = skip, use v5 rewrite)
SKIP_FILES = {
    "RealisticStaminaSystem.c",
    "SCR_StaminaConstants.c",
    "SCR_StaminaUpdateCoordinator.c",
    "SCR_StaminaConfigBridge.c",
    "PlayerBase.c",
    "SCR_RSS_CharacterSpeedBridge.c",
    "SCR_RSS_AIGroupSync.c",
    "SCR_RSS_AIGroupStaminaProxy.c",
    "SCR_RSS_AIGroupLocomotionPolicy.c",
    "SCR_AIGroup_RSS.c",
}

FILE_RENAMES = {
    "SCR_RSS_CharacterSpeedBridge.c": "SCR_RSS_SpeedBridge.c",
    "SCR_EnvironmentFactor.c": "SCR_RSS_EnvironmentFactor.c",
    "SCR_EnvironmentWeatherApi.c": "SCR_RSS_WeatherApi.c",
    "SCR_EnvironmentPenaltyMath.c": "SCR_RSS_PenaltyMath.c",
    "SCR_EnvironmentAstronomyMath.c": "SCR_RSS_AstronomyMath.c",
    "SCR_EnvironmentIndoorDetection.c": "SCR_RSS_IndoorDetection.c",
    "SCR_MaterialTerrainTable.c": "SCR_RSS_MaterialTerrainTable.c",
    "SCR_TerrainDetection.c": "SCR_RSS_TerrainDetection.c",
    "SCR_SlopeSpeedTransition.c": "SCR_RSS_SlopeSpeedTransition.c",
    "SCR_SwimmingState.c": "SCR_RSS_SwimmingState.c",
    "SCR_JumpVaultDetection.c": "SCR_RSS_JumpVaultDetection.c",
    "SCR_FatigueSystem.c": "SCR_RSS_FatigueSystem.c",
    "SCR_EncumbranceCache.c": "SCR_RSS_EncumbranceCache.c",
    "SCR_CollapseTransition.c": "SCR_RSS_CollapseTransition.c",
    "SCR_ExerciseTracking.c": "SCR_RSS_ExerciseTracker.c",
    "SCR_EpocState.c": "SCR_RSS_EpocState.c",
    "SCR_StanceTransitionManager.c": "SCR_RSS_StanceTransitionManager.c",
    "SCR_StaminaConsumption.c": "SCR_RSS_StaminaConsumptionCalculator.c",
    "SCR_StaminaRecovery.c": "SCR_RSS_RecoveryCalculator.c",
    "SCR_SpeedCalculation.c": "SCR_RSS_SpeedCalculator.c",
    "SCR_SwimmingStaminaModel.c": "SCR_RSS_SwimmingStaminaModel.c",
    "SCR_StaminaHelpers.c": "SCR_RSS_StaminaHelpers.c",
    "SCR_DebugBatchManager.c": "SCR_RSS_DebugBatchManager.c",
    "SCR_CombatStimController.c": "SCR_RSS_CombatStimController.c",
    "SCR_NetworkSync.c": "SCR_RSS_NetworkSyncManager.c",
    "SCR_UISignalBridge.c": "SCR_RSS_UISignalBridge.c",
    "SCR_DebugDisplay.c": "SCR_RSS_DebugDisplay.c",
    "SCR_MudSlipEffects.c": "SCR_RSS_MudSlipEffects.c",
    "SCR_StaminaHUDComponent.c": "SCR_RSS_StaminaHUDComponent.c",
    "SCR_InventoryStorageManagerComponent_Override.c": "SCR_RSS_InventoryOverride.c",
}

CLASS_RENAMES = {
    "StaminaConstants": "SCR_RSS_Constants",
    "StaminaConfigBridge": "SCR_RSS_ConfigBridge",
    "StaminaUpdateCoordinator": "SCR_RSS_UpdateCoordinator",
    "EnvironmentFactor": "SCR_RSS_EnvironmentFactor",
    "SCR_EnvironmentWeatherApi": "SCR_RSS_WeatherApi",
    "SCR_EnvironmentPenaltyMath": "SCR_RSS_PenaltyMath",
    "SCR_EnvironmentAstronomyMath": "SCR_RSS_AstronomyMath",
    "SCR_EnvironmentIndoorDetection": "SCR_RSS_IndoorDetection",
    "MaterialTerrainTable": "SCR_RSS_MaterialTerrainTable",
    "TerrainDetector": "SCR_RSS_TerrainDetector",
    "SlopeSpeedTransition": "SCR_RSS_SlopeSpeedTransition",
    "SwimmingStateManager": "SCR_RSS_SwimmingStateManager",
    "JumpVaultDetector": "SCR_RSS_JumpVaultDetector",
    "FatigueSystem": "SCR_RSS_FatigueSystem",
    "EncumbranceCache": "SCR_RSS_EncumbranceCache",
    "CollapseTransition": "SCR_RSS_CollapseTransition",
    "ExerciseTracker": "SCR_RSS_ExerciseTracker",
    "EpocState": "SCR_RSS_EpocState",
    "StanceTransitionManager": "SCR_RSS_StanceTransitionManager",
    "StaminaRecoveryCalculator": "SCR_RSS_RecoveryCalculator",
    "SpeedCalculator": "SCR_RSS_SpeedCalculator",
    "RealisticStaminaSpeedSystem": "SCR_RSS_MetabolismMath",
    "StaminaConsumptionCalculator": "SCR_RSS_StaminaConsumptionCalculator",
    "NetworkSyncManager": "SCR_RSS_NetworkSyncManager",
    "UISignalBridge": "SCR_RSS_UISignalBridge",
    "DebugDisplay": "SCR_RSS_DebugDisplay",
    "MudSlipEffects": "SCR_RSS_MudSlipEffects",
    "SCR_StaminaHUDComponent": "SCR_RSS_StaminaHUDComponent",
    "SCR_DebugBatchManager": "SCR_RSS_DebugBatchManager",
    "SCR_SwimmingStaminaModel": "SCR_RSS_SwimmingStaminaModel",
    "SCR_RSS_CharacterSpeedBridge": "SCR_RSS_SpeedBridge",
}


def apply_renames(content: str) -> str:
    for old, new in sorted(CLASS_RENAMES.items(), key=lambda x: -len(x[0])):
        content = re.sub(r"\b" + re.escape(old) + r"\b", new, content)
    return content


def port_file(rel: str) -> None:
    base = os.path.basename(rel)
    if base in SKIP_FILES:
        return
    new_base = FILE_RENAMES.get(base, base)
    src = os.path.join(ARCHIVE, rel)
    dest_rel = os.path.join(os.path.dirname(rel), new_base)
    dest = os.path.join(DEST, dest_rel)
    os.makedirs(os.path.dirname(dest), exist_ok=True)
    with open(src, encoding="utf-8", errors="replace") as f:
        content = apply_renames(f.read())
    with open(dest, "w", encoding="utf-8", newline="\n") as f:
        f.write(content)
    print(f"ported {rel} -> {dest_rel}")


def main() -> None:
    for dirpath, _, files in os.walk(ARCHIVE):
        for name in files:
            if not name.endswith(".c"):
                continue
            rel = os.path.relpath(os.path.join(dirpath, name), ARCHIVE)
            port_file(rel.replace("\\", "/"))


if __name__ == "__main__":
    main()
