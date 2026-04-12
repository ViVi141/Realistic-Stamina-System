//! RSS：战斗兴奋剂阶段画面叠层（ImageWidget 渐变），仅本地第一人称可见。
//! 不依赖 PostProcess static 与官方特效类绑定，与 StaminaHUD 一样由 Workspace 创建布局。

class SCR_RSS_CombatStimOverlay
{
	protected static const ResourceName OVERLAY_LAYOUT = "{A37B29E84F6C105D}UI/layouts/HUD/RSS/CombatStimOverlay.layout";

	protected static Widget s_wRoot;
	protected static ImageWidget s_wVignette;

	//! RUSH：轻微暖色 vignette；CRASH：冷色 + 更强暗角
	protected static const float RUSH_OPACITY = 0.1;
	protected static const float CRASH_OPACITY = 0.32;

	//------------------------------------------------------------------------------------------------
	static void OnHudStart()
	{
		EnsureCreated();
	}

	//------------------------------------------------------------------------------------------------
	static void OnHudStop()
	{
		Destroy();
	}

	//------------------------------------------------------------------------------------------------
	protected static void EnsureCreated()
	{
		if (s_wRoot)
			return;

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
			return;

		s_wRoot = workspace.CreateWidgets(OVERLAY_LAYOUT);
		if (!s_wRoot)
			return;

		s_wVignette = ImageWidget.Cast(s_wRoot.FindAnyWidget("RSSCombatStimVignette"));
		if (s_wVignette)
		{
			s_wVignette.SetOpacity(0);
			s_wVignette.SetVisible(false);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static void Destroy()
	{
		if (s_wRoot)
		{
			s_wRoot.RemoveFromHierarchy();
			s_wRoot = null;
		}

		s_wVignette = null;
	}

	//------------------------------------------------------------------------------------------------
	static void Tick()
	{
		if (!GetGame())
			return;

		if (Replication.IsServer())
			return;

		EnsureCreated();
		if (!s_wRoot || !s_wVignette)
			return;

		PlayerController pc = GetGame().GetPlayerController();
		if (!pc)
		{
			HideOverlay();
			return;
		}

		if (!GetGame().GetCameraManager())
		{
			HideOverlay();
			return;
		}

		CameraBase currentCamera = GetGame().GetCameraManager().CurrentCamera();
		PlayerCamera playerCamera = PlayerCamera.Cast(pc.GetPlayerCamera());
		if (!playerCamera || currentCamera != playerCamera)
		{
			HideOverlay();
			return;
		}

		IEntity local = SCR_PlayerController.GetLocalControlledEntity();
		if (!local)
		{
			HideOverlay();
			return;
		}

		ChimeraCharacter character = ChimeraCharacter.Cast(local);
		if (!character)
		{
			HideOverlay();
			return;
		}

		CameraHandlerComponent cam = CameraHandlerComponent.Cast(character.FindComponent(CameraHandlerComponent));
		if (!cam)
		{
			HideOverlay();
			return;
		}

		if (cam.IsInThirdPerson())
		{
			HideOverlay();
			return;
		}

		SCR_CharacterControllerComponent ctrl =
			SCR_CharacterControllerComponent.Cast(character.GetCharacterController());
		if (!ctrl)
		{
			HideOverlay();
			return;
		}

		int phase = ctrl.RSS_GetCombatStimPhase();

		if (phase == ERSS_CombatStimPhase.RUSH)
		{
			s_wVignette.SetVisible(true);
			s_wVignette.SetColor(Color.FromRGBA(255, 200, 115, 255));
			s_wVignette.SetOpacity(RUSH_OPACITY);
			return;
		}

		if (phase == ERSS_CombatStimPhase.CRASH)
		{
			s_wVignette.SetVisible(true);
			s_wVignette.SetColor(Color.FromRGBA(105, 128, 158, 255));
			s_wVignette.SetOpacity(CRASH_OPACITY);
			return;
		}

		HideOverlay();
	}

	//------------------------------------------------------------------------------------------------
	protected static void HideOverlay()
	{
		if (!s_wVignette)
			return;

		s_wVignette.SetOpacity(0);
		s_wVignette.SetVisible(false);
	}
}
