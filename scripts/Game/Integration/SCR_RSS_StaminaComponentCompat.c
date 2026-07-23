//! Compatibility for third-party prefabs that disable SCR_CharacterStaminaComponent
//! (e.g. Conflict PVE Remixed sets Enabled 0 / zero native drains).
class SCR_RSS_StaminaComponentCompat
{
    //! Resolve stamina component and re-activate if a foreign mod left it disabled.
    static SCR_CharacterStaminaComponent ResolveAndEnsureActive(
        IEntity owner,
        CharacterStaminaComponent fromController)
    {
        CharacterStaminaComponent staminaComp = fromController;
        if (!staminaComp && owner)
            staminaComp = CharacterStaminaComponent.Cast(
                owner.FindComponent(CharacterStaminaComponent));

        if (!staminaComp && owner)
            staminaComp = CharacterStaminaComponent.Cast(
                owner.FindComponent(SCR_CharacterStaminaComponent));

        SCR_CharacterStaminaComponent rssStamina =
            SCR_CharacterStaminaComponent.Cast(staminaComp);
        if (!rssStamina)
        {
            Print("[RSS] Compat: SCR_CharacterStaminaComponent missing on character "
                + "(foreign prefab may have removed it)");
            return null;
        }

        if (!rssStamina.IsActive() && owner)
        {
            rssStamina.Activate(owner);
            Print("[RSS] Compat: re-activated SCR_CharacterStaminaComponent "
                + "(foreign prefab had Enabled 0)");
        }

        return rssStamina;
    }
}
