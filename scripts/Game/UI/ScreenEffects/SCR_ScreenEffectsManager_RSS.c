//! 屏幕特效管理器：挂载战斗兴奋剂全屏叠层（Workspace 布局，与 PostProcess static 无关）。

modded class SCR_ScreenEffectsManager
{
	override void DisplayStartDraw(IEntity owner)
	{
		super.DisplayStartDraw(owner);
		SCR_RSS_CombatStimOverlay.OnHudStart();
	}

	override event void DisplayUpdate(IEntity owner, float timeSlice)
	{
		super.DisplayUpdate(owner, timeSlice);
		SCR_RSS_CombatStimOverlay.Tick();
	}

	override event void DisplayStopDraw(IEntity owner)
	{
		SCR_RSS_CombatStimOverlay.OnHudStop();
		super.DisplayStopDraw(owner);
	}
}
