// PlayerBase split: 成员变量
modded class SCR_CharacterControllerComponent
{
    protected float m_fLastSecondSpeed = 0.0; // 上一秒的速度
    protected float m_fCurrentSecondSpeed = 0.0; // 当前秒的速度
    protected bool m_bHasPreviousSpeed = false; // 是否已有上一秒的速度数据
    protected const int SPEED_SAMPLE_INTERVAL_MS = 1000; // 每秒采集一次速度样本
    
    protected float m_fLastStaminaPercent = 1.0;
    protected float m_fLastSpeedMultiplier = 1.0;
    protected float m_fLastStaminaUpdateTime = -1.0; // 上次体力更新时间（秒，GetWorldTime），用于按时间计算 timeDelta
    protected SCR_CharacterStaminaComponent m_pStaminaComponent; // 体力组件引用
    protected bool m_bLastExhaustedState = false; // 上次精疲力尽状态，用于状态切换日志
    
    protected ref NetworkSyncManager m_pNetworkSyncManager;
    protected string m_sLastSpeedSource = "";  // 调试用：上次速度计算来源（Server/Client）
    protected float m_fLastReconnectTime = -1.0; // 上次重连时间
    protected const float CONFIG_FETCH_TIMEOUT_SEC = 30.0; // 配置获取超时（秒），超时后每 30 秒打印一次警告
    protected float m_fLastConfigTimeoutWarningTime = -1.0; // 上次打印超时警告时间，避免刷屏

    // Native-style config replication:
    // Clients receive settings via GameMode RplProp (see SCR_RSS_ServerBootstrap.c),
    // so we do not use per-player request/ACK/broadcast fallback anymore.
    protected bool m_bRssWaitingGameModeConfig = false;
    protected float m_fRssConfigWaitStartTime = -1.0;

    protected ref CollapseTransition m_pCollapseTransition;
    protected ref SlopeSpeedTransition m_pSlopeSpeedTransition;
    
    protected ref ExerciseTracker m_pExerciseTracker;
    
    protected ref JumpVaultDetector m_pJumpVaultDetector;
    
    protected ref EncumbranceCache m_pEncumbranceCache;
    protected SCR_CharacterInventoryStorageComponent m_pCachedInventoryComponent; // 缓存库存组件（EncumbranceCache 无效时的回退）
    
    protected ref FatigueSystem m_pFatigueSystem;
    
    protected ref TerrainDetector m_pTerrainDetector;
    
    protected ref EnvironmentFactor m_pEnvironmentFactor;
    
    protected ref UISignalBridge m_pUISignalBridge;
    
    protected ref EpocState m_pEpocState;
    
    protected ref StanceTransitionManager m_pStanceTransitionManager;
    
    protected bool m_bWasSwimming = false; // 上一帧是否在游泳
    protected float m_fWetWeightStartTime = -1.0; // 湿重开始时间（上岸时间）
    protected float m_fCurrentWetWeight = 0.0; // 当前湿重（kg）
    protected bool m_bSwimmingVelocityDebugPrinted = false; // 是否已输出游泳速度调试信息

    protected vector m_vLastPositionSample = vector.Zero;
    protected bool m_bHasLastPositionSample = false;
    protected vector m_vComputedVelocity = vector.Zero;
    protected CompartmentAccessComponent m_pCompartmentAccess;
    protected CharacterAnimationComponent m_pAnimComponent;
    protected ChimeraCharacter m_pCachedOwnerCharacter; // 缓存角色实体引用（避免每帧 Cast）

    protected float m_fAnimSpeedCompensation = 1.0;
    protected float m_fLastAnimCompensationUpdateTime = -1.0;
    protected static const float ANIM_COMPENSATION_UPDATE_INTERVAL = 2.0;
    
    protected float m_fSprintStartTime = -1.0;       // 本次冲刺开始时间（世界时间秒），-1 表示未在爆发/已进入平稳期
    protected bool m_bLastWasSprinting = false;       // 上一帧是否处于 Sprint 状态，用于检测进入/离开冲刺
    protected float m_fSprintCooldownUntil = -1.0;    // 冷却结束时间（世界时间秒），此时间前再次冲刺不触发爆发，直接平稳期

    protected int m_iCombatStimPhase = ERSS_CombatStimPhase.NONE;
    protected float m_fCombatStimPhaseEndsAt = -1.0;
    protected int m_iCombatStimDelayInjectionCount = 0;
    //! 注射 Buff 前记录的 GetBleedingScale()，用于阶段结束时还原；-1 表示未处于 Buff
    protected float m_fRSS_CombatStimBleedingBaseline = -1.0;
    
    protected ref RSS_MudSlipRunner m_pMudSlipRunner;
    protected float m_fRssMudSlipCameraShake01 = 0.0;
    protected float m_fLastRssSpeedMultiplierApplied = 1.0;
    protected bool m_bRssStaminaLoopActive = false;
    protected bool m_bIsDeleted = false; // 实体删除标记：阻止 CallLater 回调在删除后继续触发
    
    // ====== AI 子系统管理器（接管所有 AI 行为层/编队/代理/节流）======
    // 原 L80-91 的 6 个 AI 成员变量、L668-681 编队速度、L870-933 模块链、
    // L462-481 proxy 检测、L1078-1081 通知、L1540-1577 getter/setter、
    // L1850-1874 距离 LOD 均已收拢至此。
    protected ref SCR_RSS_AIManager m_pAIManager;
    
    // ====== RSS v5 系统组件 ======
    protected ref SCR_AnaerobicBurstState m_pAnaerobicState;
    protected ref SCR_MetabolicSpeedLimiter m_pMetabolicLimiter;
    protected SCR_AnaerobicStateComponent_V5 m_pAnaerobicNetworkComponent;

    protected int m_iAiLoopRetryCount = 0;
    protected const int AI_LOOP_MAX_RETRIES = 5;
}
