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
            body = "Experimental. When On: tired AI change movement tier, block Attack intents when exhausted, lose perception/fire rate/aim as stamina drops, and wounded AI drain faster / recover slower. Needs AI stamina drain enabled (Disable AI Stamina Drain = Off) to drive the state machine. When Off: no combat/behavior layer — only movement limit and optional drain. Default Off.";
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
            body = "On (default): cheap speed only (encumbrance + gait + Tobler slope). No stamina drain / no CP cruise cap. Offline vs player speed gap: ~mean 15% |max ~79% (worst when W' empty on hills). Off: AI stamina pipeline — same metabolic core as players + Tobler + CP/Sprint caps (still NOT player UpdateSpeed). Offline gap vs player: ~mean 1% |max ~2%. Higher CPU (~4× cheap path). Turn Off for foot-speed parity and Fatigue Behaviors.";
            return true;
        }

        return false;
    }
}
