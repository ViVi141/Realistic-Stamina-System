// 配置预设合并、拆分与哈希（从 PlayerBase 抽出以压缩 modded 类行数）
class RSS_PlayerBaseConfigArrays : Managed
{
    static void BuildCombinedPresetArray(SCR_RSS_Settings s, array<float> outCombined)
    {
        if (!outCombined || !s)
            return;
        outCombined.Clear();
        int size = SCR_RSS_Settings.PARAMS_ARRAY_SIZE;
        array<float> elite = new array<float>();
        array<float> standard = new array<float>();
        array<float> tactical = new array<float>();
        array<float> custom = new array<float>();
        SCR_RSS_Settings.WriteParamsToArray(s.m_EliteStandard, elite);
        SCR_RSS_Settings.WriteParamsToArray(s.m_StandardMilsim, standard);
        SCR_RSS_Settings.WriteParamsToArray(s.m_TacticalAction, tactical);
        SCR_RSS_Settings.WriteParamsToArray(s.m_Custom, custom);
        for (int i = 0; i < size; i++)
        {
            if (i < elite.Count())
                outCombined.Insert(elite[i]);
            else
                outCombined.Insert(0.0);
        }
        for (int i = 0; i < size; i++)
        {
            if (i < standard.Count())
                outCombined.Insert(standard[i]);
            else
                outCombined.Insert(0.0);
        }
        for (int i = 0; i < size; i++)
        {
            if (i < tactical.Count())
                outCombined.Insert(tactical[i]);
            else
                outCombined.Insert(0.0);
        }
        for (int i = 0; i < size; i++)
        {
            if (i < custom.Count())
                outCombined.Insert(custom[i]);
            else
                outCombined.Insert(0.0);
        }
    }

    static string CalculateConfigHash(string configVersion, string selectedPreset, array<float> floatSettings, array<int> intSettings)
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
                hashString += string.ToString(intSettings[j]) + ",";
        }
        int hash = 0;
        for (int k = 0; k < hashString.Length(); k++)
        {
            hash = ((hash << 5) - hash) + hashString.ToAscii(k);
            hash = hash & 0x7FFFFFFF;
        }
        return string.ToString(hash);
    }

    static bool SplitCombinedPresetArrays(array<float> combinedPresetParams,
        out array<float> eliteParams, out array<float> standardParams,
        out array<float> tacticalParams, out array<float> customParams)
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
}
