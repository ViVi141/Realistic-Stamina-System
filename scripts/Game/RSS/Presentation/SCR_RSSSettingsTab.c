// RSS Settings Tab — 注入到 SettingsSuperMenu 中，与 Video/Audio/Interface 同层级
// SCR_SuperMenuComponent 自动管理 SubMenu 生命周期 (OnTabCreate/OnTabShow/OnTabHide)
modded class SCR_SettingsSuperMenu
{
    protected static const string RSS_TAB_IDENTIFIER = "RSSSettings";
    protected static bool s_bRSSTabAdded = false;

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

        // 仅添加一次（static 标记防止重复）
        if (!s_bRSSTabAdded)
        {
            m_SuperMenuComponent.GetTabView().AddTab(
                "{5932EB24D1397F01}UI/layouts/Menus/RSSSettings/RSSSettings.layout",
                "RSS",
                identifier: RSS_TAB_IDENTIFIER
            );
            s_bRSSTabAdded = true;
        }
    }
};
