//! RSS 配置同步构建与拆解工具（从 PlayerBase.c 拆分）
class SCR_RSS_ConfigSyncUtils
{
    static array<float> BuildPresetArray(SCR_RSS_Params p)
    {
        array<float> values = new array<float>();
        SCR_RSS_Settings.WriteParamsToArray(p, values);
        return values;
    }

    static void BuildSettingsArrays(SCR_RSS_Settings s, array<float> floatSettings, array<int> intSettings, array<bool> boolSettings)
    {
        SCR_RSS_Settings.WriteSettingsToArrays(s, floatSettings, intSettings, boolSettings);
    }

    static void BuildCombinedPresetArray(SCR_RSS_Settings s, array<float> outCombined)
    {
        if (!outCombined || !s)
            return;

        outCombined.Clear();
        int size = SCR_RSS_Settings.PARAMS_ARRAY_SIZE;
        array<float> elite = BuildPresetArray(s.m_EliteStandard);
        array<float> standard = BuildPresetArray(s.m_StandardMilsim);
        array<float> tactical = BuildPresetArray(s.m_TacticalAction);
        array<float> custom = BuildPresetArray(s.m_Custom);

        AppendPresetWithPadding(outCombined, elite, size);
        AppendPresetWithPadding(outCombined, standard, size);
        AppendPresetWithPadding(outCombined, tactical, size);
        AppendPresetWithPadding(outCombined, custom, size);
    }

    static void BuildConfigArrays(SCR_RSS_Settings settings, out array<float> combinedPresetParams, out array<float> floatSettings, out array<int> intSettings, out array<bool> boolSettings)
    {
        if (!settings)
            return;

        combinedPresetParams = new array<float>();
        floatSettings = new array<float>();
        intSettings = new array<int>();
        boolSettings = new array<bool>();

        BuildCombinedPresetArray(settings, combinedPresetParams);
        BuildSettingsArrays(settings, floatSettings, intSettings, boolSettings);
    }

    static string CalculateConfigHash(string configVersion, string selectedPreset, array<float> floatSettings, array<int> intSettings, array<bool> boolSettings)
    {
        string hashString = configVersion + "|" + selectedPreset + "|";

        if (floatSettings && floatSettings.Count() > 0)
        {
            for (int i = 0; i < floatSettings.Count(); i++)
            {
                int roundedValue = Math.Round(floatSettings[i] * 1000);
                hashString += string.ToString(roundedValue) + ",";
            }
        }

        if (intSettings && intSettings.Count() > 0)
        {
            for (int j = 0; j < intSettings.Count(); j++)
            {
                hashString += string.ToString(intSettings[j]) + ",";
            }
        }

        if (boolSettings && boolSettings.Count() > 0)
        {
            for (int bi = 0; bi < boolSettings.Count(); bi++)
            {
                if (boolSettings[bi])
                    hashString += "1,";
                else
                    hashString += "0,";
            }
        }

        int hash = 0;
        for (int k = 0; k < hashString.Length(); k++)
        {
            hash = ((hash << 5) - hash) + hashString.ToAscii(k);
            hash = hash & 0x7FFFFFFF;
        }

        return string.ToString(hash);
    }

    static bool SplitCombinedPresetParams(array<float> combinedPresetParams, out array<float> eliteParams, out array<float> standardParams, out array<float> tacticalParams, out array<float> customParams)
    {
        int size = SCR_RSS_Settings.PARAMS_ARRAY_SIZE;
        if (!combinedPresetParams || combinedPresetParams.Count() < size * 4)
            return false;

        eliteParams = new array<float>();
        standardParams = new array<float>();
        tacticalParams = new array<float>();
        customParams = new array<float>();

        for (int i = 0; i < size; i++)
            eliteParams.Insert(combinedPresetParams[i]);
        for (int i = size; i < size * 2; i++)
            standardParams.Insert(combinedPresetParams[i]);
        for (int i = size * 2; i < size * 3; i++)
            tacticalParams.Insert(combinedPresetParams[i]);
        for (int i = size * 3; i < size * 4; i++)
            customParams.Insert(combinedPresetParams[i]);

        return true;
    }

    protected static void AppendPresetWithPadding(array<float> outCombined, array<float> preset, int size)
    {
        for (int i = 0; i < size; i++)
        {
            if (i < preset.Count())
                outCombined.Insert(preset[i]);
            else
                outCombined.Insert(0.0);
        }
    }
}
