// RSS Settings Tab — 注入到 SettingsSuperMenu 中，与 Video/Audio/Interface 同层级
// 每次菜单打开时重新添加 Tab（TabView 在菜单关闭时销毁）
modded class SCR_SettingsSuperMenu
{
    protected static const string RSS_TAB_IDENTIFIER = "RSSSettings";

    //------------------------------------------------------------------------------------------------
    override void OnMenuOpen()
    {
        super.OnMenuOpen();

        // 仅管理员可见
        int playerId = GetGame().GetPlayerController().GetPlayerId();
        PlayerManager pm = GetGame().GetPlayerManager();
        if (!pm) return;

        bool isAdmin = pm.HasPlayerRole(playerId, EPlayerRole.ADMINISTRATOR)
                    || pm.HasPlayerRole(playerId, EPlayerRole.SESSION_ADMINISTRATOR)
                    || pm.HasPlayerRole(playerId, EPlayerRole.GAME_MASTER);
        if (!isAdmin) return;

        // 每次打开都重新添加（TabView 在菜单关闭时销毁，需重建）
        m_SuperMenuComponent.GetTabView().RemoveTabByIdentifier(RSS_TAB_IDENTIFIER);
        m_SuperMenuComponent.GetTabView().AddTab(
            "{5932EB24D1397F01}UI/layouts/Menus/RSSSettings/RSSSettings.layout",
            "RSS",
            identifier: RSS_TAB_IDENTIFIER
        );
    }
};
