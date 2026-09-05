//! RSS Enforce 全链路实测探针（控制台，非聊天）。
//!
//! 用法（进世界有本地角色后）：
//!   SCR_RSS_PerfProbe.Run();
//!   SCR_RSS_PerfProbe.Run(3000);
//!   SCR_RSS_PerfProbe.RunNearestAi(3000);  // 真实 AI 廉价限速（玩家实体上 ApplyCheap 会 early-out）
//!
//! 输出：脚本日志 + $profile:RSS_PerfProbe.txt

class SCR_RSS_PerfProbe
{
    protected static const string OUT_PATH = "$profile:RSS_PerfProbe.txt";
    protected static const int DEFAULT_ITERS = 2000;
    protected static const int WARMUP_ITERS = 50;

    protected static ref array<string> s_aLines;
    protected static float s_fBaselineMs;

    //------------------------------------------------------------------------------------------------
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

        RunOnController(ctrl, owner, iterations, "local_player");
    }

    //------------------------------------------------------------------------------------------------
    //! 找一只附近非玩家角色实测（含真实 ApplyCheapAiSpeed）。
    static void RunNearestAi(int iterations = DEFAULT_ITERS)
    {
        if (iterations < 100)
            iterations = 100;

        IEntity local = SCR_PlayerController.GetLocalControlledEntity();
        if (!local)
        {
            Print("[RSS_PerfProbe] RunNearestAi: need local player in world", LogLevel.WARNING);
            return;
        }

        IEntity best = FindNearestAiCharacter(local.GetOrigin(), 250.0);
        if (!best)
        {
            Print("[RSS_PerfProbe] RunNearestAi: no AI within 250m", LogLevel.WARNING);
            return;
        }

        SCR_CharacterControllerComponent ctrl = SCR_CharacterControllerComponent.Cast(
            best.FindComponent(SCR_CharacterControllerComponent));
        if (!ctrl || !ctrl.HasRssData())
        {
            Print("[RSS_PerfProbe] RunNearestAi: AI has no RSS data yet", LogLevel.WARNING);
            return;
        }

        PrintFormat("[RSS_PerfProbe] RunNearestAi target=%1", best.ToString());
        RunOnController(ctrl, best, iterations, "nearest_ai");
    }

    protected static IEntity FindNearestAiCharacter(vector origin, float radiusM)
    {
        if (!GetGame())
            return null;

        AIWorld aiWorld = GetGame().GetAIWorld();
        if (!aiWorld)
            return null;

        array<AIAgent> agents = {};
        aiWorld.GetAIAgents(agents);
        if (!agents)
            return null;

        IEntity best = null;
        float bestD2 = radiusM * radiusM;
        int n = agents.Count();
        for (int i = 0; i < n; i++)
        {
            AIAgent ag = agents.Get(i);
            if (!ag)
                continue;
            IEntity ent = ag.GetControlledEntity();
            if (!ent)
                continue;
            SCR_CharacterControllerComponent c = SCR_CharacterControllerComponent.Cast(
                ent.FindComponent(SCR_CharacterControllerComponent));
            if (!c)
                continue;
            if (c.IsPlayerControlled())
                continue;
            float d2 = vector.DistanceSq(origin, ent.GetOrigin());
            if (d2 < bestD2)
            {
                bestD2 = d2;
                best = ent;
            }
        }
        return best;
    }

    //------------------------------------------------------------------------------------------------
    static void RunOnController(
        SCR_CharacterControllerComponent ctrl,
        IEntity owner,
        int iterations,
        string tag)
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
        SCR_RSS_AnaerobicBurst ana = ctrl.RSS_GetWPrimeBurst();

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
        float currentSpeed = 3.5;
        float terrainFactor = 1.0;
        int phase = ctrl.GetCurrentMovementPhase();
        if (phase < 1)
            phase = 2;

        bool isPlayer = ctrl.IsPlayerControlled();
        PrintFormat("[RSS_PerfProbe] START tag=%1 iters=%2 player=%3 owner=%4",
            tag, iterations, isPlayer, owner.ToString());
        AppendLine(string.Format("RSS_PerfProbe START tag=%1 iters=%2 player=%3", tag, iterations, isPlayer));

        // ========== 00 校准 ==========
        Section("00_CALIBRATION");
        s_fBaselineMs = MeasureEmptyLoop(iterations);
        Report("00_empty_loop", s_fBaselineMs, iterations, "tick calibration");

        // ========== 01 限速原子 ==========
        Section("01_ATOMIC_SPEED");
        Report("01_encumbrance", MeasureEncumbrance(enc, iterations), iterations, "CheckAndUpdate");
        Report("01b_phase_v6_mult", MeasurePhaseMult(stamina, phase, encPen, iterations), iterations,
            "CalculateV6PhaseSpeedMultiplier");
        Report("01c_from_inputs", MeasureFromInputs(stamina, encPen, phase, iterations), iterations,
            "CalculateFinalSpeedMultiplierFromInputs (AI-friendly)");
        Report("01d_abs_to_frac", MeasureAbsToFrac(ctrl, iterations), iterations,
            "FractionForAbsoluteSpeed keepSource");
        Report("01e_apply_speed_limit", MeasureApplySpeed(owner, iterations), iterations,
            "SpeedBridge.ApplyStaminaSpeedLimit WRITE");
        Report("01f_engine_top", MeasureEngineTop(ctrl, iterations), iterations,
            "GetOriginalEngineMaxSpeed_Run/Sprint");
        Report("01g_slope_raw", MeasureSlopeRaw(ctrl, vel, iterations), iterations,
            "GetRawSlopeAngle (FloorNormal→Trace)");
        Report("01g2_floor_normal", MeasureFloorNormal(ctrl, iterations), iterations,
            "TryGetCharacterFloorNormal only");
        Report("01g3_trace_normal", MeasureTraceNormal(ctrl, iterations), iterations,
            "TryGetTracedTerrainNormal only");
        Report("01h_grade_percent", MeasureGrade(ctrl, currentSpeed, env, vel, iterations), iterations,
            "CalculateGradePercent");
        Report("01i_update_speed", MeasureUpdateSpeed(
            ctrl, stamina, encPen, collapse, currentSpeed, env, slope, vel, terrainFactor, phase, iterations),
            iterations, "UpdateCoordinator.UpdateSpeed FULL");

        // ========== 02 消耗原子 ==========
        Section("02_ATOMIC_DRAIN");
        Report("02a_metabolism_power", MeasureMetabolism(currentSpeed, enc, iterations), iterations,
            "MetabolismPowerWatts");
        Report("02b_drain_from_power", MeasureDrainFromPower(iterations), iterations,
            "StaminaDrainRatePerSecondFromPowerWatts");
        Report("02c_total_drain_rate", MeasureTotalDrainRate(ctrl, enc, ana, stamina, phase, iterations),
            iterations, "UpdateCoordinator.CalculateTotalDrainRate");
        Report("02d_shared_env_heat", MeasureSharedEnvHeat(nowSec, iterations), iterations,
            "AISharedEnvCache.GetHeatStressMultiplier");
        Report("02e_cp_metab_cap", MeasureCpCap(ctrl, stamina, encPen, currentSpeed, enc, terrainFactor, phase, iterations),
            iterations, "GetMetabolicCorrectedSpeedMultiplier");

        // ========== 03 AI 辅助 ==========
        Section("03_ATOMIC_AI_AUX");
        Report("03a_pos_delta", MeasurePosDelta(owner, lastPos, hasPos, vel, iterations), iterations,
            "CalculateCurrentSpeed");
        Report("03b_terrain_ray", MeasureTerrainSafe(terrain, owner, nowSec, currentSpeed, iterations),
            iterations, "TerrainDetector.GetTerrainFactor");
        Report("03c_env_update", MeasureEnvSafe(env, owner, nowSec, vel, terrainFactor, iterations),
            iterations, "EnvironmentFactor.UpdateEnvironmentFactors");
        Report("03d_lod_nearest_player", MeasureLod(owner, iterations), iterations,
            "GetNearestPlayerDistanceM");
        Report("03e_ai_combat_tick", MeasureCombatSafe(aiMgr, owner, ctrl, nowSec, stamina, currentSpeed, iterations),
            iterations, "AIManager.Tick");
        Report("03f_apply_cheap_ai", MeasureApplyCheapAi(ctrl, owner, enc, iterations), iterations,
            "ApplyCheapAiSpeed (false on player)");

        // ========== 04 组合路径 ==========
        Section("04_COMPOSED_PATHS");
        float msPlayerSpeedStack = MeasureComposedPlayerSpeedStack(
            ctrl, enc, terrain, env, collapse, slope, owner,
            stamina, phase, encPen, nowSec, lastPos, hasPos, vel, iterations);
        Report("A_player_speed_stack", msPlayerSpeedStack, iterations,
            "terrain+env+UpdateSpeed+Apply (no drain)");

        float msLegacy = MeasureComposedLegacy(
            ctrl, enc, terrain, env, collapse, slope, owner,
            stamina, phase, encPen, nowSec, lastPos, hasPos, vel, iterations);
        Report("B_legacy_full", msLegacy, iterations,
            "old AI: speed stack + metab + CP cap");

        float msLight = MeasureComposedLight(enc, owner, stamina, phase, encPen, iterations);
        Report("C_ai_light_v6", msLight, iterations,
            "enc+V6mult+Apply (old light, wrong denom risk)");

        float msAiCheapEquiv = MeasureComposedAiCheapAbs(ctrl, enc, owner, iterations);
        Report("D_ai_cheap_abs", msAiCheapEquiv, iterations,
            "6.2.28 cheap: march abs/phaseTop+Apply");

        float msFromInputsPath = MeasureComposedFromInputs(ctrl, owner, stamina, encPen, phase, iterations);
        Report("E_from_inputs_path", msFromInputsPath, iterations,
            "FromInputs+Apply (recommended AI speed)");

        float msAiPipe = MeasureComposedAiPipeline(
            ctrl, enc, owner, stamina, phase, encPen, lastPos, hasPos, vel, nowSec, iterations);
        Report("F_ai_pipeline", msAiPipe, iterations,
            "6.2.28: cheapAbs+pos+sharedHeat+metab+drain");

        float msDrainCheapOld = MeasureComposedDrainCheap(
            enc, owner, stamina, phase, encPen, lastPos, hasPos, vel, iterations);
        Report("G_drain_cheap_old", msDrainCheapOld, iterations,
            "6.2.27: V6mult+pos+metab+drain");

        // ========== TAKEAWAY ==========
        Section("05_TAKEAWAY");
        float baseNet = Math.Max(msAiCheapEquiv - s_fBaselineMs, 0.001);
        float rPlayer = Math.Max(msPlayerSpeedStack - s_fBaselineMs, 0.0) / baseNet;
        float rLegacy = Math.Max(msLegacy - s_fBaselineMs, 0.0) / baseNet;
        float rFrom = Math.Max(msFromInputsPath - s_fBaselineMs, 0.0) / baseNet;
        float rPipe = Math.Max(msAiPipe - s_fBaselineMs, 0.0) / baseNet;
        float rLight = Math.Max(msLight - s_fBaselineMs, 0.0) / baseNet;

        Print("[RSS_PerfProbe] === ratios vs D_ai_cheap_abs (net) ===");
        PrintFormat("[RSS_PerfProbe] A_player_speed=%1x  B_legacy=%2x  E_fromInputs=%3x  F_pipeline=%4x  C_v6light=%5x",
            rPlayer, rLegacy, rFrom, rPipe, rLight);
        AppendLine(string.Format(
            "TAKEAWAY vs_D_cheapAbs A_player=%1 B_legacy=%2 E_fromInputs=%3 F_pipeline=%4 C_v6light=%5",
            rPlayer, rLegacy, rFrom, rPipe, rLight));

        WriteFile();
        PrintFormat("[RSS_PerfProbe] DONE wrote %1", OUT_PATH);
    }

    //------------------------------------------------------------------------------------------------
    protected static void Section(string name)
    {
        PrintFormat("[RSS_PerfProbe] --- %1 ---", name);
        AppendLine(string.Format("--- %1 ---", name));
    }

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
        PrintFormat(
            "[RSS_PerfProbe] %1 total=%2ms net=%3ms us/call=%4 | %5",
            name, totalMs, net, usPer, notes);
        AppendLine(string.Format(
            "%1 totalMs=%2 netMs=%3 usPerCall=%4 | %5",
            name, totalMs, net, usPer, notes));
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
        float sink = 0.0;
        int w;
        for (w = 0; w < WARMUP_ITERS; w++)
            sink = SCR_RSS_SpeedCalculator.CalculateV6PhaseSpeedMultiplier(stamina, phase, encPen);
        int t0 = System.GetTickCount();
        for (int i = 0; i < iterations; i++)
            sink = SCR_RSS_SpeedCalculator.CalculateV6PhaseSpeedMultiplier(stamina, phase, encPen);
        if (sink < 0.0)
            Print("[RSS_PerfProbe] phase");
        return System.GetTickCount(t0);
    }

    protected static float MeasureFromInputs(float stamina, float encPen, int phase, int iterations)
    {
        bool exhausted = SCR_RSS_MetabolismMath.IsExhausted(stamina);
        float sink = 0.0;
        int w;
        for (w = 0; w < WARMUP_ITERS; w++)
        {
            sink = SCR_RSS_UpdateCoordinator.CalculateFinalSpeedMultiplierFromInputs(
                stamina, encPen, false, phase, exhausted, true, 3.5, 5.0, -1.0);
        }
        int t0 = System.GetTickCount();
        for (int i = 0; i < iterations; i++)
        {
            sink = SCR_RSS_UpdateCoordinator.CalculateFinalSpeedMultiplierFromInputs(
                stamina, encPen, false, phase, exhausted, true, 3.5, 5.0, -1.0);
        }
        if (sink < 0.0)
            Print("[RSS_PerfProbe] fromInputs");
        return System.GetTickCount(t0);
    }

    protected static float MeasureAbsToFrac(SCR_CharacterControllerComponent ctrl, int iterations)
    {
        float top = ctrl.GetOriginalEngineMaxSpeed_Run();
        if (top < 0.1)
            top = SCR_RSS_MetabolismMath.TARGET_RUN_SPEED;
        float target = SCR_RSS_ConfigBridge.GetMarchRunSpeedMs();
        float sink = 0.0;
        int w;
        for (w = 0; w < WARMUP_ITERS; w++)
            sink = SCR_RSS_SpeedBridge.FractionForAbsoluteSpeed(target, top, true);
        int t0 = System.GetTickCount();
        for (int i = 0; i < iterations; i++)
            sink = SCR_RSS_SpeedBridge.FractionForAbsoluteSpeed(target, top, true);
        if (sink < 0.0)
            Print("[RSS_PerfProbe] absFrac");
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

    protected static float MeasureEngineTop(SCR_CharacterControllerComponent ctrl, int iterations)
    {
        float sink = 0.0;
        int w;
        for (w = 0; w < WARMUP_ITERS; w++)
        {
            sink = ctrl.GetOriginalEngineMaxSpeed_Run();
            sink = sink + ctrl.GetOriginalEngineMaxSpeed_Sprint();
        }
        int t0 = System.GetTickCount();
        for (int i = 0; i < iterations; i++)
        {
            sink = ctrl.GetOriginalEngineMaxSpeed_Run();
            sink = sink + ctrl.GetOriginalEngineMaxSpeed_Sprint();
        }
        if (sink < 0.0)
            Print("[RSS_PerfProbe] engineTop");
        return System.GetTickCount(t0);
    }

    protected static float MeasureSlopeRaw(
        SCR_CharacterControllerComponent ctrl, vector vel, int iterations)
    {
        float sink = 0.0;
        int w;
        for (w = 0; w < WARMUP_ITERS; w++)
            sink = SCR_RSS_SpeedCalculator.GetRawSlopeAngle(ctrl, vel);
        int t0 = System.GetTickCount();
        for (int i = 0; i < iterations; i++)
            sink = SCR_RSS_SpeedCalculator.GetRawSlopeAngle(ctrl, vel);
        if (sink < -100.0)
            Print("[RSS_PerfProbe] slope");
        return System.GetTickCount(t0);
    }

    protected static float MeasureFloorNormal(SCR_CharacterControllerComponent ctrl, int iterations)
    {
        vector n;
        bool ok = false;
        int w;
        for (w = 0; w < WARMUP_ITERS; w++)
            ok = SCR_RSS_SpeedCalculator.TryGetCharacterFloorNormal(ctrl, n);
        int t0 = System.GetTickCount();
        for (int i = 0; i < iterations; i++)
            ok = SCR_RSS_SpeedCalculator.TryGetCharacterFloorNormal(ctrl, n);
        if (!ok && n[1] < -2.0)
            Print("[RSS_PerfProbe] floor");
        return System.GetTickCount(t0);
    }

    protected static float MeasureTraceNormal(SCR_CharacterControllerComponent ctrl, int iterations)
    {
        vector n;
        bool ok = false;
        int w;
        for (w = 0; w < WARMUP_ITERS; w++)
            ok = SCR_RSS_SpeedCalculator.TryGetTracedTerrainNormal(ctrl, n);
        int t0 = System.GetTickCount();
        for (int i = 0; i < iterations; i++)
            ok = SCR_RSS_SpeedCalculator.TryGetTracedTerrainNormal(ctrl, n);
        if (!ok && n[1] < -2.0)
            Print("[RSS_PerfProbe] traceN");
        return System.GetTickCount(t0);
    }

    protected static float MeasureGrade(
        SCR_CharacterControllerComponent ctrl,
        float speed,
        SCR_RSS_EnvironmentFactor env,
        vector vel,
        int iterations)
    {
        float angle = 0.0;
        float sink = 0.0;
        int w;
        for (w = 0; w < WARMUP_ITERS; w++)
        {
            RSS_GradeCalculationResult g = SCR_RSS_SpeedCalculator.CalculateGradePercent(
                ctrl, speed, null, angle, env, vel);
            sink = g.gradePercent;
        }
        int t0 = System.GetTickCount();
        for (int i = 0; i < iterations; i++)
        {
            RSS_GradeCalculationResult g2 = SCR_RSS_SpeedCalculator.CalculateGradePercent(
                ctrl, speed, null, angle, env, vel);
            sink = g2.gradePercent;
        }
        if (sink < -100.0)
            Print("[RSS_PerfProbe] grade");
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
        float sink = 0.0;
        int w;
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
            Print("[RSS_PerfProbe] updateSpeed");
        return System.GetTickCount(t0);
    }

    protected static float MeasureMetabolism(float speedMs, SCR_RSS_EncumbranceCache enc, int iterations)
    {
        float weight = 100.0;
        if (enc && enc.IsCacheValid())
            weight = enc.GetCurrentWeight() + SCR_RSS_MetabolismMath.CHARACTER_WEIGHT;
        if (speedMs < 0.5)
            speedMs = 3.5;

        float sink = 0.0;
        int w;
        for (w = 0; w < WARMUP_ITERS; w++)
            sink = SCR_RSS_MetabolismModel.MetabolismPowerWatts(speedMs, weight, 5.0, 1.2, true, 2);
        int t0 = System.GetTickCount();
        for (int i = 0; i < iterations; i++)
            sink = SCR_RSS_MetabolismModel.MetabolismPowerWatts(speedMs, weight, 5.0, 1.2, true, 2);
        if (sink < 0.0)
            Print("[RSS_PerfProbe] metab");
        return System.GetTickCount(t0);
    }

    protected static float MeasureDrainFromPower(int iterations)
    {
        float sink = 0.0;
        int w;
        for (w = 0; w < WARMUP_ITERS; w++)
            sink = SCR_RSS_MetabolismModel.StaminaDrainRatePerSecondFromPowerWatts(450.0, 320.0);
        int t0 = System.GetTickCount();
        for (int i = 0; i < iterations; i++)
            sink = SCR_RSS_MetabolismModel.StaminaDrainRatePerSecondFromPowerWatts(450.0, 320.0);
        if (sink < 0.0)
            Print("[RSS_PerfProbe] drain");
        return System.GetTickCount(t0);
    }

    protected static float MeasureTotalDrainRate(
        SCR_CharacterControllerComponent ctrl,
        SCR_RSS_EncumbranceCache enc,
        SCR_RSS_AnaerobicBurst ana,
        float stamina,
        int phase,
        int iterations)
    {
        RSS_StaminaDrainTickParams p = new RSS_StaminaDrainTickParams();
        p.useSwimmingModel = false;
        p.currentSpeed = 3.5;
        p.gearWeightKg = 30.0;
        if (enc && enc.IsCacheValid())
            p.gearWeightKg = enc.GetCurrentWeight();
        p.encumbranceSpeedPenalty = 0.05;
        p.bodyPlusGearWeightKg = p.gearWeightKg + SCR_RSS_MetabolismMath.CHARACTER_WEIGHT;
        p.totalWeightWithWetAndBody = p.bodyPlusGearWeightKg;
        p.gradePercent = 5.0;
        p.terrainFactor = 1.0;
        p.velocityForDrain = "3.5 0 0";
        p.owner = ctrl.GetOwner();
        p.controller = ctrl;
        p.environmentFactor = null;
        p.isSprinting = false;
        p.currentMovementPhase = phase;
        p.speedRatio = 3.5 / SCR_RSS_MetabolismMath.GAME_MAX_SPEED;
        p.heatStressMultiplier = 1.0;
        p.isSprintActive = false;
        p.staminaPercent = stamina;
        p.combatStimActive = false;
        p.encumbranceCache = enc;
        p.fatigueSystem = null;
        p.exerciseTracker = null;
        p.epocState = null;
        p.currentTimeSec = 0.0;
        p.currentTimeForExerciseMs = 0.0;
        p.appliedSpeedLimitMs = -1.0;
        p.effectiveCriticalPowerWatts = SCR_RSS_ConfigBridge.GetCriticalPowerWatts();
        p.wPrimePool01 = 1.0;
        if (ana && ana.GetCpModel())
        {
            p.effectiveCriticalPowerWatts = ana.GetCpModel().GetEffectiveCriticalPowerWatts();
            p.wPrimePool01 = ana.GetCpModel().GetPool01();
        }

        float sink = 0.0;
        int w;
        for (w = 0; w < WARMUP_ITERS; w++)
        {
            RSS_StaminaDrainTickResult r = SCR_RSS_UpdateCoordinator.CalculateTotalDrainRate(p);
            sink = r.totalDrainRate;
        }
        int t0 = System.GetTickCount();
        for (int i = 0; i < iterations; i++)
        {
            RSS_StaminaDrainTickResult r2 = SCR_RSS_UpdateCoordinator.CalculateTotalDrainRate(p);
            sink = r2.totalDrainRate;
        }
        if (sink < -100.0)
            Print("[RSS_PerfProbe] totalDrain");
        return System.GetTickCount(t0);
    }

    protected static float MeasureSharedEnvHeat(float nowSec, int iterations)
    {
        float sink = 0.0;
        int w;
        for (w = 0; w < WARMUP_ITERS; w++)
            sink = SCR_RSS_AISharedEnvCache.GetHeatStressMultiplier(nowSec);
        int t0 = System.GetTickCount();
        for (int i = 0; i < iterations; i++)
            sink = SCR_RSS_AISharedEnvCache.GetHeatStressMultiplier(nowSec + (i * 0.001));
        if (sink < 0.0)
            Print("[RSS_PerfProbe] heat");
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

        float sink = 0.0;
        int w;
        for (w = 0; w < WARMUP_ITERS; w++)
        {
            sink = SCR_RSS_DrainCalculator.GetMetabolicCorrectedSpeedMultiplier(
                0.85, currentSpeed, phase, encPen, weight, 5.0, terrainFactor,
                exhausted, 5.5, nowSec, null, currentSpeed);
        }
        int t0 = System.GetTickCount();
        for (int i = 0; i < iterations; i++)
        {
            sink = SCR_RSS_DrainCalculator.GetMetabolicCorrectedSpeedMultiplier(
                0.85, currentSpeed, phase, encPen, weight, 5.0, terrainFactor,
                exhausted, 5.5, nowSec, null, currentSpeed);
        }
        if (sink < 0.0)
            Print("[RSS_PerfProbe] cp");
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

    protected static float MeasureTerrainSafe(
        SCR_RSS_TerrainDetector terrain, IEntity owner, float nowSec, float speed, int iterations)
    {
        if (!terrain)
            return -1.0;
        int w;
        for (w = 0; w < WARMUP_ITERS; w++)
            terrain.GetTerrainFactor(owner, nowSec, speed);
        int t0 = System.GetTickCount();
        for (int i = 0; i < iterations; i++)
            terrain.GetTerrainFactor(owner, nowSec + (i * 0.001), speed);
        return System.GetTickCount(t0);
    }

    protected static float MeasureEnvSafe(
        SCR_RSS_EnvironmentFactor env, IEntity owner, float nowSec, vector vel, float terrainFactor, int iterations)
    {
        if (!env)
            return -1.0;
        int w;
        for (w = 0; w < WARMUP_ITERS; w++)
            env.UpdateEnvironmentFactors(nowSec, owner, vel, terrainFactor, 0.0);
        int t0 = System.GetTickCount();
        for (int i = 0; i < iterations; i++)
            env.UpdateEnvironmentFactors(nowSec + (i * 0.01), owner, vel, terrainFactor, 0.0);
        return System.GetTickCount(t0);
    }

    protected static float MeasureLod(IEntity owner, int iterations)
    {
        float sink = 0.0;
        int w;
        for (w = 0; w < WARMUP_ITERS; w++)
            sink = SCR_RSS_AIUpdateInterval.GetNearestPlayerDistanceM(owner);
        int t0 = System.GetTickCount();
        for (int i = 0; i < iterations; i++)
            sink = SCR_RSS_AIUpdateInterval.GetNearestPlayerDistanceM(owner);
        if (sink < -2.0)
            Print("[RSS_PerfProbe] lod");
        return System.GetTickCount(t0);
    }

    protected static float MeasureCombatSafe(
        SCR_RSS_AIManager aiMgr,
        IEntity owner,
        SCR_CharacterControllerComponent ctrl,
        float nowSec,
        float stamina,
        float currentSpeed,
        int iterations)
    {
        if (!aiMgr)
            return -1.0;
        int w;
        for (w = 0; w < WARMUP_ITERS; w++)
            aiMgr.Tick(owner, ctrl, nowSec, 0.2, stamina, 0.0, currentSpeed, false, 50.0);
        int t0 = System.GetTickCount();
        for (int i = 0; i < iterations; i++)
            aiMgr.Tick(owner, ctrl, nowSec + (i * 0.001), 0.2, stamina, 0.0, currentSpeed, false, 50.0);
        return System.GetTickCount(t0);
    }

    protected static float MeasureApplyCheapAi(
        SCR_CharacterControllerComponent ctrl,
        IEntity owner,
        SCR_RSS_EncumbranceCache enc,
        int iterations)
    {
        if (ctrl.IsPlayerControlled())
            return -1.0;

        float frac;
        float encOut;
        float sta;
        int ph;
        bool exh;
        int w;
        for (w = 0; w < WARMUP_ITERS; w++)
        {
            SCR_PlayerBaseAiLightTickHelper.ApplyCheapAiSpeed(
                ctrl, owner, enc, 1.0, frac, encOut, sta, ph, exh);
        }
        int t0 = System.GetTickCount();
        for (int i = 0; i < iterations; i++)
        {
            SCR_PlayerBaseAiLightTickHelper.ApplyCheapAiSpeed(
                ctrl, owner, enc, 1.0, frac, encOut, sta, ph, exh);
        }
        return System.GetTickCount(t0);
    }

    //------------------------------------------------------------------------------------------------
    protected static float MeasureComposedLight(
        SCR_RSS_EncumbranceCache enc,
        IEntity owner,
        float stamina,
        int phase,
        float encPen,
        int iterations)
    {
        float sink = 0.0;
        int w;
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

    protected static float MeasureComposedAiCheapAbs(
        SCR_CharacterControllerComponent ctrl,
        SCR_RSS_EncumbranceCache enc,
        IEntity owner,
        int iterations)
    {
        float sink = 0.0;
        int w;
        for (w = 0; w < WARMUP_ITERS; w++)
        {
            if (enc)
                enc.CheckAndUpdate();
            float target = SCR_RSS_ConfigBridge.GetMarchRunSpeedMs();
            float top = ctrl.GetOriginalEngineMaxSpeed_Run();
            if (top < 0.1)
                top = SCR_RSS_MetabolismMath.TARGET_RUN_SPEED;
            sink = SCR_RSS_SpeedBridge.FractionForAbsoluteSpeed(target, top, true);
            SCR_RSS_SpeedBridge.ApplyStaminaSpeedLimit(owner, sink);
        }
        int t0 = System.GetTickCount();
        for (int i = 0; i < iterations; i++)
        {
            if (enc)
                enc.CheckAndUpdate();
            float target2 = SCR_RSS_ConfigBridge.GetMarchRunSpeedMs();
            float top2 = ctrl.GetOriginalEngineMaxSpeed_Run();
            if (top2 < 0.1)
                top2 = SCR_RSS_MetabolismMath.TARGET_RUN_SPEED;
            sink = SCR_RSS_SpeedBridge.FractionForAbsoluteSpeed(target2, top2, true);
            SCR_RSS_SpeedBridge.ApplyStaminaSpeedLimit(owner, sink);
        }
        return System.GetTickCount(t0);
    }

    protected static float MeasureComposedFromInputs(
        SCR_CharacterControllerComponent ctrl,
        IEntity owner,
        float stamina,
        float encPen,
        int phase,
        int iterations)
    {
        bool exhausted = SCR_RSS_MetabolismMath.IsExhausted(stamina);
        float sink = 0.0;
        int w;
        for (w = 0; w < WARMUP_ITERS; w++)
        {
            sink = SCR_RSS_UpdateCoordinator.CalculateFinalSpeedMultiplierFromInputs(
                stamina, encPen, false, phase, exhausted, true, 3.5, 5.0, -1.0);
            SCR_RSS_SpeedBridge.ApplyStaminaSpeedLimit(owner, Math.Clamp(sink, 0.01, 0.999));
        }
        int t0 = System.GetTickCount();
        for (int i = 0; i < iterations; i++)
        {
            sink = SCR_RSS_UpdateCoordinator.CalculateFinalSpeedMultiplierFromInputs(
                stamina, encPen, false, phase, exhausted, true, 3.5, 5.0, -1.0);
            SCR_RSS_SpeedBridge.ApplyStaminaSpeedLimit(owner, Math.Clamp(sink, 0.01, 0.999));
        }
        return System.GetTickCount(t0);
    }

    protected static float MeasureComposedPlayerSpeedStack(
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
        float sink = 0.0;
        int w;
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
        }
        return System.GetTickCount(t0);
    }

    protected static float MeasureComposedAiPipeline(
        SCR_CharacterControllerComponent ctrl,
        SCR_RSS_EncumbranceCache enc,
        IEntity owner,
        float stamina,
        int phase,
        float encPen,
        vector lastPos,
        bool hasPos,
        vector vel,
        float nowSec,
        int iterations)
    {
        vector lp = lastPos;
        bool hp = hasPos;
        vector cv = vel;
        float weight = 100.0;
        if (enc && enc.IsCacheValid())
            weight = enc.GetCurrentWeight() + SCR_RSS_MetabolismMath.CHARACTER_WEIGHT;

        float sink = 0.0;
        int w;
        for (w = 0; w < WARMUP_ITERS; w++)
        {
            if (enc)
                enc.CheckAndUpdate();
            float target = SCR_RSS_ConfigBridge.GetMarchRunSpeedMs();
            float top = ctrl.GetOriginalEngineMaxSpeed_Run();
            if (top < 0.1)
                top = SCR_RSS_MetabolismMath.TARGET_RUN_SPEED;
            sink = SCR_RSS_SpeedBridge.FractionForAbsoluteSpeed(target, top, true);
            SCR_RSS_SpeedBridge.ApplyStaminaSpeedLimit(owner, sink);
            RSS_SpeedCalculationResult r = SCR_RSS_UpdateCoordinator.CalculateCurrentSpeed(
                owner, lp, hp, cv, 0.2);
            lp = r.lastPositionSample;
            hp = r.hasLastPositionSample;
            cv = r.computedVelocity;
            sink = SCR_RSS_AISharedEnvCache.GetHeatStressMultiplier(nowSec);
            float p = SCR_RSS_MetabolismModel.MetabolismPowerWatts(3.5, weight, 5.0, 1.0, true, 2);
            sink = SCR_RSS_MetabolismModel.StaminaDrainRatePerSecondFromPowerWatts(p, 320.0);
        }
        int t0 = System.GetTickCount();
        for (int i = 0; i < iterations; i++)
        {
            if (enc)
                enc.CheckAndUpdate();
            float target2 = SCR_RSS_ConfigBridge.GetMarchRunSpeedMs();
            float top2 = ctrl.GetOriginalEngineMaxSpeed_Run();
            if (top2 < 0.1)
                top2 = SCR_RSS_MetabolismMath.TARGET_RUN_SPEED;
            sink = SCR_RSS_SpeedBridge.FractionForAbsoluteSpeed(target2, top2, true);
            SCR_RSS_SpeedBridge.ApplyStaminaSpeedLimit(owner, sink);
            RSS_SpeedCalculationResult r2 = SCR_RSS_UpdateCoordinator.CalculateCurrentSpeed(
                owner, lp, hp, cv, 0.2);
            lp = r2.lastPositionSample;
            hp = r2.hasLastPositionSample;
            cv = r2.computedVelocity;
            sink = SCR_RSS_AISharedEnvCache.GetHeatStressMultiplier(nowSec + (i * 0.001));
            float p2 = SCR_RSS_MetabolismModel.MetabolismPowerWatts(3.5, weight, 5.0, 1.0, true, 2);
            sink = SCR_RSS_MetabolismModel.StaminaDrainRatePerSecondFromPowerWatts(p2, 320.0);
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

        float sink = 0.0;
        int w;
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

        float sink = 0.0;
        int w;
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
