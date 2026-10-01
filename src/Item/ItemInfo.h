#pragma once

#include <RE/Skyrim.h>

#include <cstdint>
#include <string>
#include <vector>

namespace ItemInfo
{
        
    struct MagicEffectInfo
    {
        RE::FormID formID = 0;

        std::string name;
        std::string description;

        float magnitude = 0.0f;
        float duration = 0.0f;
        float area = 0.0f;

        float baseCost = 0.0f;

        RE::ActorValue primaryAV =
            RE::ActorValue::kNone;

        RE::ActorValue secondaryAV =
            RE::ActorValue::kNone;

        RE::ActorValue associatedSkill =
            RE::ActorValue::kNone;

        RE::EffectArchetype archetype =
            RE::EffectArchetype::kNone;

        bool hostile = false;
        bool detrimental = false;

        // Formulários associados ao efeito.
        RE::FormID associatedFormID = 0;
        RE::FormID grantedSpellID = 0;
        RE::FormID grantedPerkID = 0;

        // Alquimia: somente informações descobertas.
        bool known = true;
    };

    struct EnchantmentInfo
    {
        RE::FormID formID = 0;

        std::string name;

        bool isInstanceEnchantment = false;

        float currentCharge = 0.0f;
        float maximumCharge = 0.0f;

        std::vector<MagicEffectInfo> effects;
    };

    struct Data
    {
        RE::TESForm* form = nullptr;

        RE::FormID formID = 0;

        std::uint16_t uniqueID = 0;
        bool hasUniqueID = false;

        std::string name;

        RE::FormType formType = RE::FormType::None;

        EnchantmentInfo enchantment;

        // ====================================================
        // INVENTÁRIO
        // ====================================================

        //int quantity = 0;
        int totalQuantity = 0;
        int instanceQuantity = 0;

        bool instanceFound = false;

        bool isEquipped = false;

        // ====================================================
        // PROPRIEDADES GERAIS
        // ====================================================

        float weight = 0.0f;
        int value = 0;

        // ====================================================
        // ARMAS
        // ====================================================

        float damage = 0.0f;
        float attackSpeed = 0.0f;

        float itemHealth = 1.0f;
        bool hasItemHealth = false;

        // ====================================================
        // ARMADURAS
        // ====================================================

        float armorRating = 0.0f;

        
        float reach = 0.0f;
        float stagger = 0.0f;
        float criticalDamage = 0.0f;

        RE::WEAPON_TYPE weaponType =
            RE::WEAPON_TYPE::kHandToHandMelee;

        // ====================================================
        // ENCANTAMENTOS
        // ====================================================

        bool isEnchanted = false;

        std::vector<EnchantmentInfo> enchantments;

        // ====================================================
        // SPELLS
        // ====================================================

        std::string magicSchool;

        float magickaCost = 0.0f;

        float chargeTime = 0.0f;
        float castDuration = 0.0f;
        float range = 0.0f;

        std::vector<MagicEffectInfo> effects;

        // ====================================================
        // ESTADO DOS DADOS
        // ====================================================

        bool hasPhysicalStats = false;
        bool hasMagicStats = false;

            
    };

    // ============================================================
    // CONTROLE DOS CACHES
    // ============================================================

    void InvalidateQuantityCache();

    void ClearQuantityCache();

    void InvalidatePreviewCache();

    // ============================================================
    // PREVIEW
    // ============================================================

    const Data& GetPreviewInfo(
        RE::TESForm* form,
        std::uint16_t uniqueID,
        bool hasUniqueID,
        const std::string& displayName
    );

    Data Get(
        RE::TESForm* form,
        std::uint16_t uniqueID = 0,
        bool hasUniqueID = false,
        const std::string& displayName = {}
    );
}