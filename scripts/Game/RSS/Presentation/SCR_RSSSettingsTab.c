// RSS Settings Tab — 注入到 SettingsSuperMenu 中，与 Video/Audio/Interface 同层级
// 所有人可见：非管理员只能调自己的 HUD，管理员可调整服务器配置
modded class SCR_SettingsSuperMenu
{
    protected static const string RSS_TAB_IDENTIFIER = "RSSSettings";

    //------------------------------------------------------------------------------------------------
    override void OnMenuOpen()
    {
        super.OnMenuOpen();

        // 所有人可见（不再限制管理员）
        m_SuperMenuComponent.GetTabView().RemoveTabByIdentifier(RSS_TAB_IDENTIFIER);
        m_SuperMenuComponent.GetTabView().AddTab(
            "{5932EB24D1397F01}UI/layouts/Menus/RSSSettings/RSSSettings.layout",
            "RSS",
            identifier: RSS_TAB_IDENTIFIER
        );
    }
};
