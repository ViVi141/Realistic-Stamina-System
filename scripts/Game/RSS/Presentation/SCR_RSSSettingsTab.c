// RSS Settings Tab — 注入到 SettingsSuperMenu 中
// 与 Video / Audio / Interface 同一层级
modded class SCR_SettingsSuperMenu
{
    protected static const string RSS_TAB_IDENTIFIER = "RSSSettings";
    protected static bool s_bRSSTabAdded = false;

    //------------------------------------------------------------------------------------------------
    override void OnMenuOpen()
    {
        super.OnMenuOpen();

        // 仅管理员可见 RSS Tab
        int playerId = GetGame().GetPlayerController().GetPlayerId();
        PlayerManager pm = GetGame().GetPlayerManager();
        bool isAdmin = false;
        if (pm)
        {
            isAdmin = pm.HasPlayerRole(playerId, EPlayerRole.ADMINISTRATOR)
                   || pm.HasPlayerRole(playerId, EPlayerRole.SESSION_ADMINISTRATOR)
                   || pm.HasPlayerRole(playerId, EPlayerRole.GAME_MASTER);
        }

        if (!isAdmin)
            return;

        // 避免重复添加（每次 OnMenuOpen/OnMenuClose 都会触发）
        if (!s_bRSSTabAdded)
        {
            m_SuperMenuComponent.GetTabView().AddTab(
                "{5932EB24D1397F01}UI/layouts/Menus/RSSSettings/RSSSettings.layout",
                "RSS",
                enabled: true,
                identifier: RSS_TAB_IDENTIFIER
            );
            s_bRSSTabAdded = true;

            SCR_RSSAdminMenuUI.OnTabCreated(m_SuperMenuComponent.GetTabView());
        }
    }
};
