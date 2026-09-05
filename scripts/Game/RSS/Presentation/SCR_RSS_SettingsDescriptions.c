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

        if (widgetName == "ToggleBreathSound")
        {
            title = "Breath Sounds";
            body = "Plays RSS exertion breathing (inhale/exhale) driven by the cardiorespiratory axis. Local client preference only; does not change stamina or server configuration. Off stops playback immediately.";
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
            title = "AI Fatigue Behaviors";
            body = "Experimental. When On: tired AI change movement tier, block Attack when exhausted, lose perception/fire rate/aim as stamina drops, wounded AI drain faster / recover slower, and FRESH AI may use Sprint speed caps. Needs Disable AI Stamina Drain = Off. When Off: no combat layer; AI foot-speed stays at Run ceiling (BT sprint does not outrun a jogging player). Default Off.";
            return true;
        }

        if (widgetName == "ToggleDisableAI")
        {
            title = "Disable All AI RSS";
            body = "Stops the entire RSS loop for AI (no speed limit, no drain, no fatigue behaviors). Engine handles AI stamina. Use for max performance or AI-mod compatibility. Overrides the other two AI toggles while On.";
            return true;
        }

        if (widgetName == "ToggleDisableAIStamina")
        {
            title = "Disable AI Stamina Drain";
            body = "On (default): cheap speed only (encumbrance + gait + Tobler). No drain / no CP cruise. Sprint intent is forced to Run. Off: AI stamina pipeline (same metabolic core + Tobler + CP caps; still NOT player UpdateSpeed). Without Fatigue Behaviors, sprint intent stays capped at Run so AI do not outrun a jogging player. Turn On Fatigue Behaviors if you want AI short sprints when W' allows. Higher CPU when Off (~4× cheap path).";
            return true;
        }

        return false;
    }
}
