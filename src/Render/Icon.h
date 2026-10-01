#pragma once

#include <RE/Skyrim.h>
#include <d3d11.h>

namespace ItemIcon
{
    enum class Type
    {
        Unknown,

        // ============================================================
        // WEAPONS
        // ============================================================

        DefaultWeapon,

        WeaponSword,
        WeaponGreatsword,
        WeaponDaedra,
        WeaponDagger,
        WeaponWarAxe,
        WeaponBattleaxe,
        WeaponMace,
        WeaponHammer,
        WeaponStaff,
        WeaponBow,
        WeaponArrow,
        WeaponPickaxe,
        WeaponWoodAxe,
        WeaponCrossbow,
        WeaponBolt,
        WeaponFishingRod,

        // ============================================================
        // ARMOR - LIGHT
        // ============================================================

        DefaultArmor,

        LightArmorBody,
        LightArmorHead,
        LightArmorHands,
        LightArmorForearms,
        LightArmorFeet,
        LightArmorCalves,
        LightArmorShield,
        LightArmorMask,

        // ============================================================
        // ARMOR - HEAVY
        // ============================================================

        ArmorBody,
        ArmorHead,
        ArmorHands,
        ArmorForearms,
        ArmorFeet,
        ArmorCalves,
        ArmorShield,
        ArmorMask,
        ArmorBracer,
        ArmorDaedra,

        // ============================================================
        // CLOTHING
        // ============================================================

        ClothingBody,
        ClothingRobe,
        ClothingHead,
        ClothingPants,
        ClothingHands,
        ClothingForearms,
        ClothingFeet,
        ClothingCalves,
        ClothingShoes,
        ClothingShield,
        ClothingMask,

        ClothingBackpack,
        ClothingCloak,

        // ============================================================
        // JEWELRY
        // ============================================================

        ArmorAmulet,
        ArmorRing,
        ArmorCirclet,

        // ============================================================
        // BOOKS / SCROLLS
        // ============================================================

        DefaultScroll,

        DefaultBook,
        DefaultBookRead,

        BookTome,
        BookTomeRead,
        BookJournal,
        BookNote,
        BookMap,

        ScrollSpider,

        // ============================================================
        // FOOD
        // ============================================================

        DefaultFood,
        FoodWine,
        FoodBeer,

        // ============================================================
        // INGREDIENT
        // ============================================================

        DefaultIngredient,

        // ============================================================
        // KEY
        // ============================================================

        DefaultKey,
        KeyHouse,

        // ============================================================
        // POTIONS
        // ============================================================

        DefaultPotion,

        PotionHealth,
        PotionStamina,
        PotionMagicka,

        PotionPoison,

        PotionFrost,
        PotionFire,
        PotionShock,

        // ============================================================
        // MISC
        // ============================================================

        DefaultMisc,

        MiscArtifact,
        MiscClutter,
        MiscLockpick,
        MiscSoulGem,

        SoulGemEmpty,
        SoulGemPartial,
        SoulGemFull,

        SoulGemGrandEmpty,
        SoulGemGrandPartial,
        SoulGemGrandFull,

        SoulGemAzura,

        MiscGem,
        MiscOre,
        MiscIngot,
        MiscHide,
        MiscStrips,
        MiscLeather,
        MiscWood,
        MiscRemains,
        MiscTrollSkull,
        MiscTorch,
        MiscGoldSack,
        MiscGold,
        MiscDragonClaw,

        MiscHousePart,
        MiscCamping,
        MiscChitin,
        MiscHorseTack,

        SoulGemTomatoEmpty,
        SoulGemTomatoPartial,
        SoulGemTomatoFull,

        SoulGemAyleidCrystalEmpty,
        SoulGemAyleidCrystalPartial,
        SoulGemAyleidCrystalFull,

        MiscDwarvenScrap,
        MiscElderScroll,
        MiscJar,

        MiscTool,
        MiscToy,
        MiscInstrument,
        MiscBearTrap,

        // ============================================================
        // MAGIC
        // ============================================================

        DefaultAlteration,
        DefaultIllusion,
        DefaultDestruction,
        DefaultConjuration,
        DefaultRestoration,

        DefaultShout,
        DefaultPower,
        DefaultEffect,

        MagicFire,
        MagicFrost,
        MagicShock,

        MagicSun,
        MagicWind,
        MagicVampire,
        MagicWater,
        MagicEarth,

        Count
    };


    bool Initialize();

    void Shutdown();

    Type ResolveType(RE::TESForm* a_form);

    ID3D11ShaderResourceView* Get(RE::TESForm* a_form);

    ID3D11ShaderResourceView* Get(Type a_type);
}