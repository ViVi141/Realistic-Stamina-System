//! RSS Enforce 链路实测探针（控制台调用，非聊天）。
//!
//! Workbench / 游戏 Script Console 示例：
//!   SCR_RSS_PerfProbe.Run();
//!   SCR_RSS_PerfProbe.Run(3000);
//!
//! 结果：Print 到脚本日志，并写入 $profile:RSS_PerfProbe.txt

class SCR_RSS_PerfProbe
{
    protected static const string OUT_PATH = "$profile:RSS_PerfProbe.txt";
    protected static const int DEFAULT_ITERS = 2000;
    protected static const int WARMUP_ITERS = 50;

    protected static ref array<string> s_aLines;
    protected static float s_fBaselineMs;

    //------------------------------------------------------------------------------------------------
    //! 控制台入口：对本地控制角色跑全链路实测。
    static void Run(int iterations = DEFAULT_ITERS)
    {
        if (iterations < 100)
            iterations = 100;

        IEntity owner = SCR_PlayerController.GetLocalControlledEntity();
        if (!owner)
        {
            Print("[RSS_PerfProbe] no local controlled entity — enter world as player first", LogLevel.WARNING);
            return;
        }

        SCR_CharacterControllerComponent ctrl = SCR_CharacterControllerComponent.Cast(
            owner.FindComponent(SCR_CharacterControllerComponent));
        if (!ctrl)
        {
            Print("[RSS_PerfProbe] no SCR_CharacterControllerComponent", LogLevel.WARNING);
            return;
        }

        if (!ctrl.HasRssData())
        {
            Print("[RSS_PerfProbe] RSS not initialized on local player yet", LogLevel.WARNING);
            return;
        }

        RunOnController(ctrl, owner, iterations);
    }

    //------------------------------------------------------------------------------------------------
    //! 对指定控制器实测（亦可从其它脚本传入 AI 实体）。
    static void RunOnController(SCR_CharacterControllerComponent ctrl, IEntity owner, int iterations)
    {
        if (!ctrl || !owner)
            return;
        if (iterations < 100)
            iterations = 100;

        s_aLines = new array<string>();
        s_fBaselineMs = 0.0;

        SCR_RSS_EncumbranceCache enc = ctrl.GetRssEncumbranceCache();
        SCR_RSS_TerrainDetector terrain = ctrl.GetRssTerrainDetector();
        SCR_RSS_EnvironmentFactor env = ctrl.GetRssEnvironmentFactor();
        SCR_RSS_CollapseTransition collapse = ctrl.GetRssCollapseTransition();
        SCR_RSS_SlopeSpeedTransition slope = ctrl.GetRssSlopeSpeedTransition();
        SCR_RSS_AIManager aiMgr = ctrl.GetRssAIManager();

        float nowSec = 0.0;
        SCR_RSS_RuntimeGuard.TryGetWorldTimeSec(nowSec);

        float stamina = ctrl.GetRssAerobicPercent();
        stamina = Math.Clamp(stamina, 0.0, 1.0);
        float encPen = 0.0;
        if (enc)
        {
            enc.CheckAndUpdate();
            encPen = enc.GetSpeedPenaltyFraction();
        }

        vector lastPos = owner.GetOrigin();
        bool hasPos = true;
        vector vel = vector.Zero;
        float currentSpeed = 0.0;
        float terrainFactor = 1.0;
        int phase = ctrl.GetCurrentMovementPhase();
        if (phase < 1)
            phase = 2;

        PrintFormat("[RSS_PerfProbe] START iters=%1 owner=%2", iterations, owner.ToString());
        AppendLine(string.Format("RSS_PerfProbe START iters=%1", iterations));

        // empty loop calibration
        s_fBaselineMs = MeasureEmptyLoop(iterations);
        Report("00_empty_loop", s_fBaselineMs, iterations, "tick calibration");

        // --- atomic kernels ---
        float msEnc = MeasureEncumbrance(enc, iterations);
        Report("01_encumbrance", msEnc, iterations, "CheckAndUpdate");

        float msPhase = MeasurePhaseMult(stamina, phase, encPen, iterations);
        Report("02_phase_speed_mult", msPhase, iterations, "CalculateV6PhaseSpeedMultiplier");

        float msApply = MeasureApplySpeed(owner, iterations);
        Report("03_apply_speed_limit", msApply, iterations, "SpeedBridge.ApplyStaminaSpeedLimit");

        float msPos = MeasurePosDelta(owner, lastPos, hasPos, vel, iterations);
        Report("04_pos_delta_speed", msPos, iterations, "UpdateCoordinator.CalculateCurrentSpeed");

        float msTerrain = 0.0;
        if (terrain)
        {
            msTerrain = MeasureTerrain(terrain, owner, nowSec, currentSpeed, iterations);
            Report("05_terrain_ray", msTerrain, iterations, "TerrainDetector.GetTerrainFactor");
            terrainFactor = terrain.GetTerrainFactor(owner, nowSec, currentSpeed);
        }
        else
        {
            Report("05_terrain_ray", -1.0, iterations, "SKIP no detector");
        }

        float msEnv = 0.0;
        if (env)
        {
            msEnv = MeasureEnv(env, owner, nowSec, vel, terrainFactor, iterations);
            Report("06_env_update", msEnv, iterations, "EnvironmentFactor.UpdateEnvironmentFactors");
        }
        else
        {
            Report("06_env_update", -1.0, iterations, "SKIP no env");
        }

        float msUpdateSpeed = MeasureUpdateSpeed(
            ctrl, stamina, encPen, collapse, currentSpeed, env, slope, vel, terrainFactor, phase, iterations);
        Report("07_update_speed", msUpdateSpeed, iterations, "UpdateCoordinator.UpdateSpeed (legacy Speed)");

        float msMetab = MeasureMetabolism(currentSpeed, enc, iterations);
        Report("08_metabolism_power", msMetab, iterations, "MetabolismModel.MetabolismPowerWatts");

        float msDrain = MeasureDrainFromPower(iterations);
        Report("09_drain_from_power", msDrain, iterations, "StaminaDrainRatePerSecondFromPowerWatts");

        float msCpCap = MeasureCpCap(ctrl, stamina, encPen, currentSpeed, enc, terrainFactor, phase, iterations);
        Report("10_cp_metab_cap", msCpCap, iterations, "GetMetabolicCorrectedSpeedMultiplier");

        float msLod = MeasureLod(owner, iterations);
        Report("11_lod_nearest_player", msLod, iterations, "AIUpdateInterval.GetNearestPlayerDistanceM");

        float msCombat = 0.0;
        if (aiMgr)
        {
            msCombat = MeasureCombat(aiMgr, owner, ctrl, nowSec, stamina, currentSpeed, iterations);
            Report("12_ai_combat_tick", msCombat, iterations, "AIManager.Tick (combat layer)");
        }
        else
        {
            Report("12_ai_combat_tick", -1.0, iterations, "SKIP no AIManager (player OK)");
        }

        // --- composed paths (match 6.2.27 control flow) ---
        float msLight = MeasureComposedLight(enc, owner, stamina, phase, encPen, iterations);
        Report("A_path_light_only", msLight, iterations, "Speed禁用On / cheap speed");

        float msDrainCheap = MeasureComposedDrainCheap(
            enc, owner, stamina, phase, encPen, lastPos, hasPos, vel, iterations);
        Report("B_path_drain_cheap", msDrainCheap, iterations, "6.2.27 Speed=Off");

        float msLegacy = MeasureComposedLegacy(
            ctrl, enc, terrain, env, collapse, slope, owner,
            stamina, phase, encPen, nowSec, lastPos, hasPos, vel, iterations);
        Report("C_path_legacy_heavy", msLegacy, iterations, "pre-fix full Speed stack");

        float lightNet = Math.Max(msLight - s_fBaselineMs, 0.001);
        float ratioLegacyLight = Math.Max(msLegacy - s_fBaselineMs, 0.0) / lightNet;
        float ratioCheapLight = Math.Max(msDrainCheap - s_fBaselineMs, 0.0) / lightNet;
        float ratioLegacyCheap = Math.Max(msLegacy - s_fBaselineMs, 0.0)
            / Math.Max(msDrainCheap - s_fBaselineMs, 0.001);

        Print("[RSS_PerfProbe] === Speed takeaway (net of empty loop) ===");
        PrintFormat("[RSS_PerfProbe] legacy/light=%1x  drain_cheap/light=%2x  legacy/drain_cheap=%3x",
            ratioLegacyLight, ratioCheapLight, ratioLegacyCheap);
        AppendLine(string.Format(
            "TAKEAWAY legacy/light=%1 drain_cheap/light=%2 legacy/drain_cheap=%3",
            ratioLegacyLight, ratioCheapLight, ratioLegacyCheap));

        WriteFile();
        PrintFormat("[RSS_PerfProbe] DONE wrote %1", OUT_PATH);
    }

    //------------------------------------------------------------------------------------------------
    protected static void Report(string name, float totalMs, int iterations, string notes)
    {
        if (totalMs < 0.0)
        {
            PrintFormat("[RSS_PerfProbe] %1 SKIP | %2", name, notes);
            AppendLine(string.Format("%1 SKIP | %2", name, notes));
            return;
        }

        float net = totalMs - s_fBaselineMs;
        if (net < 0.0)
            net = 0.0;
        float usPer = (net * 1000.0) / iterations;
        float msPer1k = usPer;
        PrintFormat(
            "[RSS_PerfProbe] %1 total=%2ms net=%3ms us/call=%4 ms/1k=%5 | %6",
            name, totalMs, net, usPer, msPer1k, notes);
        AppendLine(string.Format(
            "%1 totalMs=%2 netMs=%3 usPerCall=%4 msPer1k=%5 | %6",
            name, totalMs, net, usPer, msPer1k, notes));
    }

    protected static void AppendLine(string line)
    {
        if (!s_aLines)
            s_aLines = new array<string>();
        s_aLines.Insert(line);
    }

    protected static void WriteFile()
    {
        FileHandle fh = FileIO.OpenFile(OUT_PATH, FileMode.WRITE);
        if (!fh)
        {
            Print("[RSS_PerfProbe] cannot open profile file", LogLevel.WARNING);
            return;
        }
        if (s_aLines)
        {
            int n = s_aLines.Count();
            for (int i = 0; i < n; i++)
                fh.WriteLine(s_aLines.Get(i));
        }
        fh.Close();
    }

    //------------------------------------------------------------------------------------------------
    protected static float MeasureEmptyLoop(int iterations)
    {
        int t0 = System.GetTickCount();
        float sink = 0.0;
        for (int i = 0; i < iterations; i++)
            sink = sink + 0.000001;
        if (sink < 0.0)
            Print("[RSS_PerfProbe] sink");
        return System.GetTickCount(t0);
    }

    protected static float MeasureEncumbrance(SCR_RSS_EncumbranceCache enc, int iterations)
    {
        if (!enc)
            return -1.0;
        int w;
        for (w = 0; w < WARMUP_ITERS; w++)
            enc.CheckAndUpdate();
        int t0 = System.GetTickCount();
        for (int i = 0; i < iterations; i++)
            enc.CheckAndUpdate();
        return System.GetTickCount(t0);
    }

    protected static float MeasurePhaseMult(float stamina, int phase, float encPen, int iterations)
    {
        int w;
        float sink = 0.0;
        for (w = 0; w < WARMUP_ITERS; w++)
            sink = SCR_RSS_SpeedCalculator.CalculateV6PhaseSpeedMultiplier(stamina, phase, encPen);
        int t0 = System.GetTickCount();
        for (int i = 0; i < iterations; i++)
            sink = SCR_RSS_SpeedCalculator.CalculateV6PhaseSpeedMultiplier(stamina, phase, encPen);
        if (sink < 0.0)
            Print("[RSS_PerfProbe] phase sink");
        return System.GetTickCount(t0);
    }

    protected static float MeasureApplySpeed(IEntity owner, int iterations)
    {
        int w;
        for (w = 0; w < WARMUP_ITERS; w++)
            SCR_RSS_SpeedBridge.ApplyStaminaSpeedLimit(owner, 0.85);
        int t0 = System.GetTickCount();
        for (int i = 0; i < iterations; i++)
            SCR_RSS_SpeedBridge.ApplyStaminaSpeedLimit(owner, 0.85);
        return System.GetTickCount(t0);
    }

    protected static float MeasurePosDelta(
        IEntity owner, vector lastPos, bool hasPos, vector vel, int iterations)
    {
        vector lp = lastPos;
        bool hp = hasPos;
        vector cv = vel;
        int w;
        for (w = 0; w < WARMUP_ITERS; w++)
        {
            RSS_SpeedCalculationResult r = SCR_RSS_UpdateCoordinator.CalculateCurrentSpeed(
                owner, lp, hp, cv, 0.2);
            lp = r.lastPositionSample;
            hp = r.hasLastPositionSample;
            cv = r.computedVelocity;
        }
        int t0 = System.GetTickCount();
        for (int i = 0; i < iterations; i++)
        {
            RSS_SpeedCalculationResult r2 = SCR_RSS_UpdateCoordinator.CalculateCurrentSpeed(
                owner, lp, hp, cv, 0.2);
            lp = r2.lastPositionSample;
            hp = r2.hasLastPositionSample;
            cv = r2.computedVelocity;
        }
        return System.GetTickCount(t0);
    }

    protected static float MeasureTerrain(
        SCR_RSS_TerrainDetector terrain, IEntity owner, float nowSec, float speed, int iterations)
    {
        int w;
        for (w = 0; w < WARMUP_ITERS; w++)
            terrain.GetTerrainFactor(owner, nowSec, speed);
        int t0 = System.GetTickCount();
        for (int i = 0; i < iterations; i++)
            terrain.GetTerrainFactor(owner, nowSec + (i * 0.001), speed);
        return System.GetTickCount(t0);
    }

    protected static float MeasureEnv(
        SCR_RSS_EnvironmentFactor env, IEntity owner, float nowSec, vector vel, float terrainFactor, int iterations)
    {
        int w;
        for (w = 0; w < WARMUP_ITERS; w++)
            env.UpdateEnvironmentFactors(nowSec, owner, vel, terrainFactor, 0.0);
        int t0 = System.GetTickCount();
        for (int i = 0; i < iterations; i++)
            env.UpdateEnvironmentFactors(nowSec + (i * 0.01), owner, vel, terrainFactor, 0.0);
        return System.GetTickCount(t0);
    }

    protected static float MeasureUpdateSpeed(
        SCR_CharacterControllerComponent ctrl,
        float stamina,
        float encPen,
        SCR_RSS_CollapseTransition collapse,
        float currentSpeed,
        SCR_RSS_EnvironmentFactor env,
        SCR_RSS_SlopeSpeedTransition slope,
        vector vel,
        float terrainFactor,
        int phase,
        int iterations)
    {
        int w;
        float sink = 0.0;
        for (w = 0; w < WARMUP_ITERS; w++)
        {
            sink = SCR_RSS_UpdateCoordinator.UpdateSpeed(
                ctrl, stamina, encPen, collapse, currentSpeed, env, slope, vel, terrainFactor, phase);
        }
        int t0 = System.GetTickCount();
        for (int i = 0; i < iterations; i++)
        {
            sink = SCR_RSS_UpdateCoordinator.UpdateSpeed(
                ctrl, stamina, encPen, collapse, currentSpeed, env, slope, vel, terrainFactor, phase);
        }
        if (sink < 0.0)
            Print("[RSS_PerfProbe] updateSpeed sink");
        return System.GetTickCount(t0);
    }

    protected static float MeasureMetabolism(float speedMs, SCR_RSS_EncumbranceCache enc, int iterations)
    {
        float weight = 100.0;
        if (enc && enc.IsCacheValid())
            weight = enc.GetCurrentWeight() + SCR_RSS_MetabolismMath.CHARACTER_WEIGHT;
        if (speedMs < 0.5)
            speedMs = 3.5;

        int w;
        float sink = 0.0;
        for (w = 0; w < WARMUP_ITERS; w++)
        {
            sink = SCR_RSS_MetabolismModel.MetabolismPowerWatts(
                speedMs, weight, 5.0, 1.2, true, 2);
        }
        int t0 = System.GetTickCount();
        for (int i = 0; i < iterations; i++)
        {
            sink = SCR_RSS_MetabolismModel.MetabolismPowerWatts(
                speedMs, weight, 5.0, 1.2, true, 2);
        }
        if (sink < 0.0)
            Print("[RSS_PerfProbe] metab sink");
        return System.GetTickCount(t0);
    }

    protected static float MeasureDrainFromPower(int iterations)
    {
        int w;
        float sink = 0.0;
        for (w = 0; w < WARMUP_ITERS; w++)
            sink = SCR_RSS_MetabolismModel.StaminaDrainRatePerSecondFromPowerWatts(450.0, 320.0);
        int t0 = System.GetTickCount();
        for (int i = 0; i < iterations; i++)
            sink = SCR_RSS_MetabolismModel.StaminaDrainRatePerSecondFromPowerWatts(450.0, 320.0);
        if (sink < 0.0)
            Print("[RSS_PerfProbe] drain sink");
        return System.GetTickCount(t0);
    }

    protected static float MeasureCpCap(
        SCR_CharacterControllerComponent ctrl,
        float stamina,
        float encPen,
        float currentSpeed,
        SCR_RSS_EncumbranceCache enc,
        float terrainFactor,
        int phase,
        int iterations)
    {
        float weight = 100.0;
        if (enc && enc.IsCacheValid())
            weight = enc.GetCurrentWeight() + SCR_RSS_MetabolismMath.CHARACTER_WEIGHT;
        if (currentSpeed < 0.5)
            currentSpeed = 3.5;

        float nowSec = 0.0;
        SCR_RSS_RuntimeGuard.TryGetWorldTimeSec(nowSec);
        bool exhausted = SCR_RSS_MetabolismMath.IsExhausted(stamina);
        float applied = currentSpeed;

        int w;
        float sink = 0.0;
        for (w = 0; w < WARMUP_ITERS; w++)
        {
            sink = SCR_RSS_DrainCalculator.GetMetabolicCorrectedSpeedMultiplier(
                0.85, currentSpeed, phase, encPen, weight, 5.0, terrainFactor,
                exhausted, 5.5, nowSec, null, applied);
        }
        int t0 = System.GetTickCount();
        for (int i = 0; i < iterations; i++)
        {
            sink = SCR_RSS_DrainCalculator.GetMetabolicCorrectedSpeedMultiplier(
                0.85, currentSpeed, phase, encPen, weight, 5.0, terrainFactor,
                exhausted, 5.5, nowSec, null, applied);
        }
        if (sink < 0.0)
            Print("[RSS_PerfProbe] cp sink");
        return System.GetTickCount(t0);
    }

    protected static float MeasureLod(IEntity owner, int iterations)
    {
        int w;
        float sink = 0.0;
        for (w = 0; w < WARMUP_ITERS; w++)
            sink = SCR_RSS_AIUpdateInterval.GetNearestPlayerDistanceM(owner);
        int t0 = System.GetTickCount();
        for (int i = 0; i < iterations; i++)
            sink = SCR_RSS_AIUpdateInterval.GetNearestPlayerDistanceM(owner);
        if (sink < -2.0)
            Print("[RSS_PerfProbe] lod sink");
        return System.GetTickCount(t0);
    }

    protected static float MeasureCombat(
        SCR_RSS_AIManager aiMgr,
        IEntity owner,
        SCR_CharacterControllerComponent ctrl,
        float nowSec,
        float stamina,
        float currentSpeed,
        int iterations)
    {
        int w;
        for (w = 0; w < WARMUP_ITERS; w++)
        {
            aiMgr.Tick(owner, ctrl, nowSec, 0.2, stamina, 0.0, currentSpeed, false, 50.0);
        }
        int t0 = System.GetTickCount();
        for (int i = 0; i < iterations; i++)
        {
            aiMgr.Tick(owner, ctrl, nowSec + (i * 0.001), 0.2, stamina, 0.0, currentSpeed, false, 50.0);
        }
        return System.GetTickCount(t0);
    }

    protected static float MeasureComposedLight(
        SCR_RSS_EncumbranceCache enc,
        IEntity owner,
        float stamina,
        int phase,
        float encPen,
        int iterations)
    {
        int w;
        float sink = 0.0;
        for (w = 0; w < WARMUP_ITERS; w++)
        {
            if (enc)
                enc.CheckAndUpdate();
            sink = SCR_RSS_SpeedCalculator.CalculateV6PhaseSpeedMultiplier(stamina, phase, encPen);
            SCR_RSS_SpeedBridge.ApplyStaminaSpeedLimit(owner, sink);
        }
        int t0 = System.GetTickCount();
        for (int i = 0; i < iterations; i++)
        {
            if (enc)
                enc.CheckAndUpdate();
            sink = SCR_RSS_SpeedCalculator.CalculateV6PhaseSpeedMultiplier(stamina, phase, encPen);
            SCR_RSS_SpeedBridge.ApplyStaminaSpeedLimit(owner, sink);
        }
        return System.GetTickCount(t0);
    }

    protected static float MeasureComposedDrainCheap(
        SCR_RSS_EncumbranceCache enc,
        IEntity owner,
        float stamina,
        int phase,
        float encPen,
        vector lastPos,
        bool hasPos,
        vector vel,
        int iterations)
    {
        vector lp = lastPos;
        bool hp = hasPos;
        vector cv = vel;
        float weight = 100.0;
        if (enc && enc.IsCacheValid())
            weight = enc.GetCurrentWeight() + SCR_RSS_MetabolismMath.CHARACTER_WEIGHT;

        int w;
        float sink = 0.0;
        for (w = 0; w < WARMUP_ITERS; w++)
        {
            if (enc)
                enc.CheckAndUpdate();
            sink = SCR_RSS_SpeedCalculator.CalculateV6PhaseSpeedMultiplier(stamina, phase, encPen);
            SCR_RSS_SpeedBridge.ApplyStaminaSpeedLimit(owner, sink);
            RSS_SpeedCalculationResult r = SCR_RSS_UpdateCoordinator.CalculateCurrentSpeed(
                owner, lp, hp, cv, 0.2);
            lp = r.lastPositionSample;
            hp = r.hasLastPositionSample;
            cv = r.computedVelocity;
            float p = SCR_RSS_MetabolismModel.MetabolismPowerWatts(3.5, weight, 0.0, 1.0, true, 2);
            sink = SCR_RSS_MetabolismModel.StaminaDrainRatePerSecondFromPowerWatts(p, 320.0);
        }
        int t0 = System.GetTickCount();
        for (int i = 0; i < iterations; i++)
        {
            if (enc)
                enc.CheckAndUpdate();
            sink = SCR_RSS_SpeedCalculator.CalculateV6PhaseSpeedMultiplier(stamina, phase, encPen);
            SCR_RSS_SpeedBridge.ApplyStaminaSpeedLimit(owner, sink);
            RSS_SpeedCalculationResult r2 = SCR_RSS_UpdateCoordinator.CalculateCurrentSpeed(
                owner, lp, hp, cv, 0.2);
            lp = r2.lastPositionSample;
            hp = r2.hasLastPositionSample;
            cv = r2.computedVelocity;
            float p2 = SCR_RSS_MetabolismModel.MetabolismPowerWatts(3.5, weight, 0.0, 1.0, true, 2);
            sink = SCR_RSS_MetabolismModel.StaminaDrainRatePerSecondFromPowerWatts(p2, 320.0);
        }
        return System.GetTickCount(t0);
    }

    protected static float MeasureComposedLegacy(
        SCR_CharacterControllerComponent ctrl,
        SCR_RSS_EncumbranceCache enc,
        SCR_RSS_TerrainDetector terrain,
        SCR_RSS_EnvironmentFactor env,
        SCR_RSS_CollapseTransition collapse,
        SCR_RSS_SlopeSpeedTransition slope,
        IEntity owner,
        float stamina,
        int phase,
        float encPen,
        float nowSec,
        vector lastPos,
        bool hasPos,
        vector vel,
        int iterations)
    {
        vector lp = lastPos;
        bool hp = hasPos;
        vector cv = vel;
        float weight = 100.0;
        if (enc && enc.IsCacheValid())
            weight = enc.GetCurrentWeight() + SCR_RSS_MetabolismMath.CHARACTER_WEIGHT;
        bool exhausted = SCR_RSS_MetabolismMath.IsExhausted(stamina);

        int w;
        float sink = 0.0;
        for (w = 0; w < WARMUP_ITERS; w++)
        {
            if (enc)
                enc.CheckAndUpdate();
            RSS_SpeedCalculationResult r = SCR_RSS_UpdateCoordinator.CalculateCurrentSpeed(
                owner, lp, hp, cv, 0.2);
            lp = r.lastPositionSample;
            hp = r.hasLastPositionSample;
            cv = r.computedVelocity;
            float spd = r.currentSpeed;
            float tf = 1.0;
            if (terrain)
                tf = terrain.GetTerrainFactor(owner, nowSec, spd);
            if (env)
                env.UpdateEnvironmentFactors(nowSec, owner, cv, tf, 0.0);
            sink = SCR_RSS_UpdateCoordinator.UpdateSpeed(
                ctrl, stamina, encPen, collapse, spd, env, slope, cv, tf, phase);
            SCR_RSS_SpeedBridge.ApplyStaminaSpeedLimit(owner, sink);
            float p = SCR_RSS_MetabolismModel.MetabolismPowerWatts(spd, weight, 5.0, tf, true, phase);
            sink = SCR_RSS_MetabolismModel.StaminaDrainRatePerSecondFromPowerWatts(p, 320.0);
            sink = SCR_RSS_DrainCalculator.GetMetabolicCorrectedSpeedMultiplier(
                0.85, spd, phase, encPen, weight, 5.0, tf, exhausted, 5.5, nowSec, null, spd);
        }

        int t0 = System.GetTickCount();
        for (int i = 0; i < iterations; i++)
        {
            if (enc)
                enc.CheckAndUpdate();
            RSS_SpeedCalculationResult r2 = SCR_RSS_UpdateCoordinator.CalculateCurrentSpeed(
                owner, lp, hp, cv, 0.2);
            lp = r2.lastPositionSample;
            hp = r2.hasLastPositionSample;
            cv = r2.computedVelocity;
            float spd2 = r2.currentSpeed;
            float tf2 = 1.0;
            if (terrain)
                tf2 = terrain.GetTerrainFactor(owner, nowSec + (i * 0.001), spd2);
            if (env)
                env.UpdateEnvironmentFactors(nowSec + (i * 0.01), owner, cv, tf2, 0.0);
            sink = SCR_RSS_UpdateCoordinator.UpdateSpeed(
                ctrl, stamina, encPen, collapse, spd2, env, slope, cv, tf2, phase);
            SCR_RSS_SpeedBridge.ApplyStaminaSpeedLimit(owner, sink);
            float p2 = SCR_RSS_MetabolismModel.MetabolismPowerWatts(spd2, weight, 5.0, tf2, true, phase);
            sink = SCR_RSS_MetabolismModel.StaminaDrainRatePerSecondFromPowerWatts(p2, 320.0);
            sink = SCR_RSS_DrainCalculator.GetMetabolicCorrectedSpeedMultiplier(
                0.85, spd2, phase, encPen, weight, 5.0, tf2, exhausted, 5.5, nowSec, null, spd2);
        }
        return System.GetTickCount(t0);
    }
}
