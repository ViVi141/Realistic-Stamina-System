//! RSS settings menu — right-panel title/body copy (English).

class SCR_RSS_SettingsDescriptions
{
    //------------------------------------------------------------------------------------------------
    static bool GetDescriptionForWidget(string widgetName, out string title, out string body)
    {
        title = string.Empty;
        body = string.Empty;
        if (!widgetName || widgetName == "")
            return false;

        if (widgetName == "PresetSelector")
        {
            title = "RSS Preset";
            body = "Selects the server stamina model: EliteStandard (highest realism), StandardMilsim (balanced), TacticalAction (faster pacing), or Custom (editable scalars in JSON). Administrator only. Saved to RealisticStaminaSystem.json.";
            return true;
        }

        if (widgetName == "ToggleHUD")
        {
            title = "HUD Display";
            body = "Shows or hides the RSS stats overlay on your screen (stamina, speed, weight, slope, weather, ground, etc.). This is a local client preference only and does not change server configuration.";
            return true;
        }

        if (widgetName == "ToggleServerHUD")
        {
            title = "HUD Server Default";
            body = "Sets the server-wide default for RSS HUD / hint output. Players can still override with HUD Display on their client. Administrator only. Written to RealisticStaminaSystem.json.";
            return true;
        }

        if (widgetName == "ToggleDebug")
        {
            title = "Debug Log";
            body = "Prints detailed RSS calculation logs to the console. Useful for diagnosing presets and AI integration. May add overhead on busy servers. Administrator only.";
            return true;
        }

        if (widgetName == "ToggleMudSlip")
        {
            title = "Mud Slip Mechanic";
            body = "Enables slippery wet-mud ragdoll and camera stress. Off by default on dedicated servers. Disable if you prefer vanilla movement only or are tuning camera feedback.";
            return true;
        }

        if (widgetName == "ToggleAICombat")
        {
            title = "AI Stamina Combat (Experimental)";
            body = "Experimental. When On: AI stamina state machine, speed caps, intent filter (e.g. reduced attack when exhausted), combat accuracy decay, group rest waypoints, and injury-stamina link. When Off: AI still uses RSS drain/recovery and speed multipliers, but not these combat behaviors. Default Off on new dedicated servers.";
            return true;
        }

        if (widgetName == "ToggleDisableAI")
        {
            title = "Disable AI RSS (All)";
            body = "Stops all RSS processing for AI; stamina returns to engine behavior. Also disables experimental AI combat effects. Use for compatibility with other AI mods or performance testing.";
            return true;
        }

        if (widgetName == "ToggleDisableAIStamina")
        {
            title = "Disable AI Stamina (Speed)";
            body = "AI skips RSS stamina drain and recovery but keeps RSS speed multipliers from encumbrance and fatigue. Lighter than Disable AI RSS (All). Does not enable experimental combat effects by itself.";
            return true;
        }

        return false;
    }
}
