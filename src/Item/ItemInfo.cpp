#include "ItemInfo.h"
#include "Language.h"

#include <imgui.h>
#include <algorithm>
#include <cctype>
#include <unordered_map>
#include <cstdint>

namespace ItemInfo
{   
    static std::string ExtractPlainBookText(std::string_view source)
    {
        std::string result;
        result.reserve(source.size());

        const auto appendBreak = [&result](bool paragraph)
        {
            const std::size_t breaks = paragraph ? 2 : 1;
            for (std::size_t i = 0; i < breaks; ++i)
            {
                if (result.empty() || result.back() != '\n')
                    result.push_back('\n');
                else if (paragraph && result.size() < 2 ||
                    (paragraph && result[result.size() - 2] != '\n'))
                    result.push_back('\n');
            }
        };

        for (std::size_t i = 0; i < source.size();)
        {
            // Comentários não fazem parte do conteúdo exibido pelo livro.
            if (source.compare(i, 4, "<!--") == 0)
            {
                const auto end = source.find("-->", i + 4);
                i = end == std::string_view::npos ? source.size() : end + 3;
                continue;
            }

            // Pagebreak só separa páginas no BookMenu; no painel ele vira
            // uma separação de parágrafo.
            if (source.compare(i, 11, "[pagebreak]") == 0)
            {
                appendBreak(true);
                i += 11;
                continue;
            }

            if (source[i] == '<')
            {
                const auto end = source.find('>', i + 1);
                if (end == std::string_view::npos)
                {
                    ++i;
                    continue;
                }

                std::string tag(source.substr(i + 1, end - i - 1));
                std::transform(tag.begin(), tag.end(), tag.begin(),
                    [](unsigned char character)
                    {
                        return static_cast<char>(std::tolower(character));
                    });

                const auto firstSpace = tag.find_first_of(" \t\r\n");
                const std::string_view name(tag.data(),
                    firstSpace == std::string::npos ? tag.size() : firstSpace);

                if (name == "br" || name == "br/")
                    appendBreak(false);
                else if (name == "p" || name == "/p" ||
                    name == "ul" || name == "/ul")
                    appendBreak(true);
                else if (name == "li")
                {
                    appendBreak(false);
                    result += "- ";
                }
                else if (name == "/li")
                    appendBreak(false);

                else if (name == "img")
                {
                    // As letras iluminadas usam o padrão
                    // Illuminated_Letters/X_letter.png. Preservamos sua
                    // inicial como texto, enquanto outras imagens seguem
                    // omitidas do painel.
                    constexpr std::string_view illuminatedPath =
                        "illuminated_letters/";
                    const auto letterOffset = tag.find(illuminatedPath);
                    if (letterOffset != std::string::npos)
                    {
                        const auto letterIndex = letterOffset + illuminatedPath.size();
                        if (letterIndex < tag.size() &&
                            std::isalpha(static_cast<unsigned char>(tag[letterIndex])))
                        {
                            result.push_back(static_cast<char>(std::toupper(
                                static_cast<unsigned char>(tag[letterIndex]))));
                        }
                    }
                }

                // b/i/u/font e quaisquer outras tags são ignoradas; somente
                // o conteúdo textual entre elas é preservado.
                i = end + 1;
                continue;
            }

            if (source[i] != '\r')
                result.push_back(source[i]);
            ++i;
        }

        while (!result.empty() && std::isspace(
            static_cast<unsigned char>(result.back())))
        {
            result.pop_back();
        }

        std::size_t first = 0;
        while (first < result.size() && std::isspace(
            static_cast<unsigned char>(result[first])))
        {
            ++first;
        }
        return result.substr(first);
    }

        
    struct QuantityInfo
    {
        int totalQuantity = 0;
        int instanceQuantity = 0;

        bool instanceFound = false;
    };

    // ============================================================
    // CACHE DE QUANTIDADES
    // ============================================================

    struct CachedInstance
    {
        std::uint16_t uniqueID = 0;
        RE::FormID baseID = 0;

        int quantity = 0;
    };

    struct CachedItem
    {
        int totalQuantity = 0;

        // Quantidade de itens sem UniqueID.
        int withoutUniqueIDQuantity = 0;

        // Instâncias individualizadas.
        std::vector<CachedInstance> instances;
    };

    // Permite invalidar o cache manualmente.
    static bool g_quantityCacheDirty = true;

    class QuantityCacheEventSink final :
        public RE::BSTEventSink<RE::TESContainerChangedEvent>
    {
    public:
        RE::BSEventNotifyControl ProcessEvent(
            const RE::TESContainerChangedEvent* event,
            RE::BSTEventSource<RE::TESContainerChangedEvent>*) override
        {
            const auto* player = RE::PlayerCharacter::GetSingleton();
            if (event && player &&
                (event->oldContainer == player->GetFormID() ||
                    event->newContainer == player->GetFormID()))
            {
                g_quantityCacheDirty = true;
            }
            return RE::BSEventNotifyControl::kContinue;
        }
    };

    static void EnsureQuantityCacheEvents()
    {
        static QuantityCacheEventSink sink;
        static bool registered = false;
        if (registered) return;
        if (auto* source = RE::ScriptEventSourceHolder::GetSingleton())
        {
            source->AddEventSink(&sink);
            registered = true;
        }
    }

    
    // ============================================================
    // SETTINGS - ÁREA DO PAINEL DE INFORMAÇÕES
    // ============================================================

    struct SettingsInfoPanel
    {
        ImVec2 min{ 0.0f, 0.0f };
        ImVec2 max{ 0.0f, 0.0f };

        float width = 0.0f;
        float height = 0.0f;
    };

    static SettingsInfoPanel GetSettingsInfoPanel(
        const ImVec2& screen)
    {
        SettingsInfoPanel panel{};

        // Centro do radial direito virtual.
        const ImVec2 rightRadialCenter(
            screen.x * 0.75f,
            screen.y * 0.50f
        );

        constexpr float radialRadius = 175.0f;

        panel.min = ImVec2(
            rightRadialCenter.x - radialRadius,
            rightRadialCenter.y - radialRadius
        );

        panel.max = ImVec2(
            rightRadialCenter.x,
            rightRadialCenter.y + radialRadius
        );

        panel.width =
            panel.max.x - panel.min.x;

        panel.height =
            panel.max.y - panel.min.y;

        return panel;
    }

    // ============================================================
    // CACHE DE QUANTIDADE POR FRAME
    // ============================================================

    static std::unordered_map<
        RE::FormID,
        CachedItem
    > g_quantityCache;

    static int g_quantityCacheFrame = -1;

        
    static MagicEffectInfo ExtractMagicEffect(
        const RE::Effect* effect)
    {
        MagicEffectInfo result{};

        if (!effect || !effect->baseEffect)
            return result;

        const auto* base =
            effect->baseEffect;

        // ========================================================
        // IDENTIFICAÇÃO
        // ========================================================

        result.formID =
            base->GetFormID();

        const char* name =
            base->GetName();

        if (name)
            result.name = name;

        // ========================================================
        // ATRIBUTOS DO EFEITO
        // ========================================================

        result.magnitude =
            effect->GetMagnitude();

        result.duration =
            static_cast<float>(
                effect->GetDuration()
            );

        result.area =
            static_cast<float>(
                effect->GetArea()
            );

        result.baseCost =
            base->data.baseCost;

        // ========================================================
        // TIPO E ATRIBUTOS MODIFICADOS
        // ========================================================

        result.archetype =
            base->GetArchetype();

        result.primaryAV =
            base->data.primaryAV;

        result.secondaryAV =
            base->data.secondaryAV;

        result.associatedSkill =
            base->data.associatedSkill;

        result.hostile =
            base->IsHostile();

        result.detrimental =
            base->IsDetrimental();

        // ========================================================
        // FORMULÁRIO ASSOCIADO
        // ========================================================

        if (base->data.associatedForm)
        {
            result.associatedFormID =
                base->data.associatedForm->GetFormID();
        }

        // ========================================================
        // SPELL CONCEDIDA PELO EFEITO
        // ========================================================

        if (base->data.equipAbility)
        {
            result.grantedSpellID =
                base->data.equipAbility->GetFormID();
        }

        // ========================================================
        // PERK ASSOCIADO
        // ========================================================

        if (base->data.perk)
        {
            result.grantedPerkID =
                base->data.perk->GetFormID();
        }

        return result;
    }

        
    static std::vector<MagicEffectInfo> ExtractMagicEffects(
        const RE::MagicItem* magicItem)
    {
        std::vector<MagicEffectInfo> result;

        if (!magicItem)
            return result;

        result.reserve(
            magicItem->effects.size()
        );

        for (const auto* effect : magicItem->effects)
        {
            if (!effect || !effect->baseEffect)
                continue;

            result.push_back(
                ExtractMagicEffect(effect)
            );
        }

        return result;
    }

        
    static EnchantmentInfo ExtractEnchantment(
        RE::EnchantmentItem* enchantment,
        bool isInstanceEnchantment,
        float currentCharge)
    {
        EnchantmentInfo result{};

        if (!enchantment)
            return result;

        result.formID =
            enchantment->GetFormID();

        const char* name =
            enchantment->GetName();

        if (name)
            result.name = name;

        result.isInstanceEnchantment =
            isInstanceEnchantment;

        result.currentCharge =
            currentCharge;

        // ========================================================
        // EFEITOS DO ENCANTAMENTO
        // ========================================================

        result.effects =
            ExtractMagicEffects(enchantment);

        return result;
    }

    //FUNCAO AUXILIAR PARA IDENTIFICAR ESCOLA DE MAGIA
    static std::string GetMagicSchoolName(
        RE::ActorValue skill)
    {
        switch (skill)
        {
        case RE::ActorValue::kAlteration:
            return "Alteration";

        case RE::ActorValue::kConjuration:
            return "Conjuration";

        case RE::ActorValue::kDestruction:
            return "Destruction";

        case RE::ActorValue::kIllusion:
            return "Illusion";

        case RE::ActorValue::kRestoration:
            return "Restoration";

        default:
            return Language::Get("none");
        }
    }


    static RE::ExtraDataList* FindItemInstance(
        const RE::InventoryEntryData* entry,
        std::uint16_t uniqueID,
        bool hasUniqueID)
    {
        if (!entry || !entry->extraLists)
            return nullptr;

        // ========================================================
        // PROCURA UMA INSTÂNCIA COM UNIQUE ID
        // ========================================================

        if (hasUniqueID)
        {
            for (auto* extra : *entry->extraLists)
            {
                if (!extra)
                    continue;

                const auto* unique =
                    extra->GetByType<RE::ExtraUniqueID>();

                if (!unique)
                    continue;

                if (unique->uniqueID == uniqueID)
                {
                    return extra;
                }
            }

            return nullptr;
        }

        // ========================================================
        // SEM UNIQUE ID
        //
        // Não escolhe uma instância arbitrária caso existam
        // múltiplas listas de dados extras.
        // ========================================================

        RE::ExtraDataList* candidate = nullptr;

        for (auto* extra : *entry->extraLists)
        {
            if (!extra)
                continue;

            if (extra->HasType<RE::ExtraUniqueID>())
                continue;

            if (candidate)
            {
                // Existem múltiplas instâncias sem identificação.
                return nullptr;
            }

            candidate = extra;
        }

        return candidate;
    }

        
    static bool IsIngredientEffectKnown(
        const RE::IngredientItem* ingredient,
        std::size_t effectIndex)
    {
        if (!ingredient)
            return false;

        if (effectIndex >= 4)
            return false;

        const std::uint16_t knownFlags =
            ingredient->gamedata.knownEffectFlags;

        const std::uint16_t effectMask =
            static_cast<std::uint16_t>(
                1u << effectIndex
            );

        return (knownFlags & effectMask) != 0;
    }
        
        
    static RE::ExtraDataList* GetItemInstanceExtra(
        RE::TESForm* form,
        std::uint16_t uniqueID,
        bool hasUniqueID)
    {
        if (!form)
            return nullptr;

        auto* player =
            RE::PlayerCharacter::GetSingleton();

        if (!player)
            return nullptr;

        auto* object =
            form->As<RE::TESBoundObject>();

        if (!object)
            return nullptr;

        // ========================================================
        // INVENTÁRIO REAL
        // ========================================================

        auto* changes =
            player->GetInventoryChanges();

        if (!changes || !changes->entryList)
            return nullptr;

        RE::InventoryEntryData* entry = nullptr;

        for (auto* current : *changes->entryList)
        {
            if (!current || !current->object)
                continue;

            if (current->object == object)
            {
                entry = current;
                break;
            }
        }

        if (!entry || !entry->extraLists)
            return nullptr;

        // ========================================================
        // IDENTIFICAÇÃO DA INSTÂNCIA
        // ========================================================

        if (hasUniqueID)
        {
            for (auto* extra : *entry->extraLists)
            {
                if (!extra)
                    continue;

                auto* unique =
                    extra->GetByType<
                        RE::ExtraUniqueID
                    >();

                if (!unique)
                    continue;

                if (unique->uniqueID != uniqueID)
                    continue;

                if (unique->baseID != player->GetFormID())
                    continue;

                return extra;
            }

            return nullptr;
        }

        // ========================================================
        // ITEM SEM UNIQUE ID
        //
        // Não seleciona uma instância arbitrária.
        // ========================================================

        RE::ExtraDataList* candidate = nullptr;

        for (auto* extra : *entry->extraLists)
        {
            if (!extra)
                continue;

            if (extra->HasType<RE::ExtraUniqueID>())
                continue;

            if (candidate)
            {
                // Mais de uma lista sem identificação.
                // Não sabemos qual corresponde ao item.
                return nullptr;
            }

            candidate = extra;
        }

        return candidate;
    }
            
        
    static std::vector<MagicEffectInfo>
    ExtractKnownIngredientEffects(
        const RE::IngredientItem* ingredient)
    {
        std::vector<MagicEffectInfo> result;

        if (!ingredient)
            return result;

        const std::size_t effectCount =
            std::min<std::size_t>(
                ingredient->effects.size(),
                4
            );

        for (std::size_t i = 0; i < effectCount; ++i)
        {
            const auto* effect =
                ingredient->effects[i];

            if (!effect || !effect->baseEffect)
                continue;

            // ====================================================
            // EFEITO DESCONHECIDO
            //
            // Não extrai o nome, magnitude ou descrição.
            // ====================================================

            if (!IsIngredientEffectKnown(ingredient, i))
            {
                MagicEffectInfo unknown{};

                unknown.known = false;
                unknown.name = "Unknown";

                result.push_back(
                    std::move(unknown)
                );

                continue;
            }

            // ====================================================
            // EFEITO CONHECIDO
            // ====================================================

            MagicEffectInfo known =
                ExtractMagicEffect(effect);

            known.known = true;

            result.push_back(
                std::move(known)
            );
        }

        return result;
    }

        
    static EnchantmentInfo GetItemEnchantment(
        RE::TESForm* form,
        RE::ExtraDataList* extra)
    {
        EnchantmentInfo result{};

        if (!form)
            return result;

        RE::EnchantmentItem* enchantment =
            nullptr;

        bool instanceEnchantment = false;

        float currentCharge = 0.0f;

        // ========================================================
        // ENCANTAMENTO DA INSTÂNCIA
        // ========================================================

        if (extra)
        {
            if (auto* extraEnchant =
                extra->GetByType<
                    RE::ExtraEnchantment>())
            {
                enchantment =
                    extraEnchant->enchantment;

                instanceEnchantment =
                    enchantment != nullptr;
            }

            if (const auto* charge =
                extra->GetByType<RE::ExtraCharge>())
            {
                currentCharge =
                    charge->charge;
            }
        }

        // ========================================================
        // ENCANTAMENTO DO FORMULÁRIO
        // ========================================================

        if (!enchantment)
        {
            if (auto* weapon =
                form->As<RE::TESObjectWEAP>())
            {
                enchantment =
                    weapon->formEnchanting;
            }
            else if (auto* armor =
                form->As<RE::TESObjectARMO>())
            {
                enchantment =
                    armor->formEnchanting;
            }
        }

        if (!enchantment)
            return result;

        // ========================================================
        // EXTRAI TODOS OS EFEITOS
        // ========================================================

        return ExtractEnchantment(
            enchantment,
            instanceEnchantment,
            currentCharge
        );
    }

    
    
    // ============================================================
    // INVALIDAÇÃO MANUAL
    // ============================================================

    void InvalidateQuantityCache()
    {
        g_quantityCacheDirty = true;
    }

    // ============================================================
    // LIMPEZA COMPLETA
    // ============================================================

    void ClearQuantityCache()
    {
        g_quantityCache.clear();

        g_quantityCacheFrame = -1;
        g_quantityCacheDirty = true;
    }

    // ============================================================
    // RECONSTRÓI O CACHE DO INVENTÁRIO
    // ============================================================

    static void UpdateQuantityCache()
    {
        EnsureQuantityCacheEvents();
        const int currentFrame =
            ImGui::GetFrameCount();

        // O inventário permanece válido entre frames. Eventos reais de
        // entrada/saída do container do jogador invalidam este snapshot.
        if (!g_quantityCacheDirty)
        {
            return;
        }

        g_quantityCacheFrame = currentFrame;
        g_quantityCacheDirty = false;

        g_quantityCache.clear();

        auto* player =
            RE::PlayerCharacter::GetSingleton();

        if (!player)
            return;

        const auto inventory =
            player->GetInventory();

        const RE::FormID playerID =
            player->GetFormID();

        // ========================================================
        // PERCORRE O INVENTÁRIO APENAS UMA VEZ
        // ========================================================

        for (const auto& [object, entry] : inventory)
        {
            if (!object)
                continue;

            const int totalQuantity =
                std::max(0, entry.first);

            if (totalQuantity <= 0)
                continue;

            CachedItem cached{};

            cached.totalQuantity =
                totalQuantity;

            const auto& entryData =
                entry.second;

            int representedCount = 0;
            int withoutUniqueIDCount = 0;

            // ====================================================
            // PERCORRE AS INSTÂNCIAS
            // ====================================================

            if (entryData && entryData->extraLists)
            {
                for (auto* extra :
                    *entryData->extraLists)
                {
                    if (!extra)
                        continue;

                    const int count =
                        std::max(
                            0,
                            extra->GetCount()
                        );

                    representedCount += count;

                    const auto* unique =
                        extra->GetByType<
                            RE::ExtraUniqueID
                        >();

                    if (!unique)
                    {
                        withoutUniqueIDCount += count;
                        continue;
                    }

                    // =================================================
                    // REGISTRA A INSTÂNCIA
                    // =================================================

                    CachedInstance instance{};

                    instance.uniqueID =
                        unique->uniqueID;

                    instance.baseID =
                        unique->baseID;

                    instance.quantity =
                        count;

                    cached.instances.push_back(
                        instance
                    );
                }
            }

            // ====================================================
            // ITENS SEM EXTRADATALIST
            // ====================================================

            const int implicitCount =
                std::max(
                    0,
                    totalQuantity - representedCount
                );

            cached.withoutUniqueIDQuantity =
                withoutUniqueIDCount +
                implicitCount;

            // ====================================================
            // ARMAZENA NO CACHE
            // ====================================================

            g_quantityCache.emplace(
                object->GetFormID(),
                std::move(cached)
            );
        }
    }

    
    // ============================================================
    // CACHE DO ITEM EM PREVIEW
    // ============================================================

    struct PreviewCache
    {
        RE::TESForm* form = nullptr;

        std::uint16_t uniqueID = 0;
        bool hasUniqueID = false;

        std::string displayName;

        Data info{};

        bool valid = false;
    };

    static PreviewCache g_previewCache;

    // ============================================================
    // INVALIDA O PREVIEW
    // ============================================================

    void InvalidatePreviewCache()
    {
        g_previewCache.valid = false;
    }

    // ============================================================
    // CONSULTA AS INFORMAÇÕES COMPLETAS
    // ============================================================

    const Data& GetPreviewInfo(
        RE::TESForm* form,
        std::uint16_t uniqueID,
        bool hasUniqueID,
        const std::string& displayName)
    {
        // ========================================================
        // VERIFICA SE O ITEM MUDOU
        // ========================================================

        const bool sameItem =
            g_previewCache.valid &&
            g_previewCache.form == form &&
            g_previewCache.uniqueID == uniqueID &&
            g_previewCache.hasUniqueID == hasUniqueID &&
            g_previewCache.displayName == displayName;

        if (sameItem)
        {
            return g_previewCache.info;
        }

        // ========================================================
        // ATUALIZA O CACHE
        // ========================================================

        g_previewCache.form =
            form;

        g_previewCache.uniqueID =
            uniqueID;

        g_previewCache.hasUniqueID =
            hasUniqueID;

        g_previewCache.displayName =
            displayName;

        g_previewCache.info =
            Get(
                form,
                uniqueID,
                hasUniqueID,
                displayName
            );

        g_previewCache.valid =
            form != nullptr;

        return g_previewCache.info;
    }
    
    static QuantityInfo GetItemQuantities(
        RE::TESForm* form,
        std::uint16_t uniqueID,
        bool hasUniqueID)
    {
        QuantityInfo result{};

        if (!form)
            return result;

        // ========================================================
        // ATUALIZA O CACHE SE NECESSÁRIO
        // ========================================================

        UpdateQuantityCache();

        // ========================================================
        // LOCALIZA O FORMULÁRIO
        // ========================================================

        const auto it =
            g_quantityCache.find(
                form->GetFormID()
            );

        if (it == g_quantityCache.end())
            return result;

        const CachedItem& cached =
            it->second;

        result.totalQuantity =
            cached.totalQuantity;

        // ========================================================
        // ITEM SEM UNIQUE ID
        // ========================================================

        if (!hasUniqueID)
        {
            result.instanceQuantity =
                cached.withoutUniqueIDQuantity;

            result.instanceFound =
                result.instanceQuantity > 0;

            return result;
        }

        // ========================================================
        // ITEM COM UNIQUE ID
        // ========================================================

        auto* player =
            RE::PlayerCharacter::GetSingleton();

        if (!player)
            return result;

        const RE::FormID playerID =
            player->GetFormID();

        for (const CachedInstance& instance :
            cached.instances)
        {
            if (instance.uniqueID != uniqueID)
                continue;

            if (instance.baseID != playerID)
                continue;

            result.instanceQuantity +=
                instance.quantity;

            result.instanceFound = true;
        }

        return result;
    }


    // ============================================================
    // EXTRAI AS INFORMAÇÕES DO ITEM
    // ============================================================

    Data Get(
        RE::TESForm* form,
        std::uint16_t uniqueID,
        bool hasUniqueID,
        const std::string& displayName)
    {
        Data info{};

        if (!form)
            return info;

        info.form = form;

        info.formID =
            form->GetFormID();

        info.uniqueID = uniqueID;

        info.hasUniqueID = hasUniqueID;

        info.formType =
            form->GetFormType();


        // ========================================================
        // NOME DA INSTÂNCIA
        // ========================================================

        if (!displayName.empty())
        {
            info.name = displayName;
        }
        else
        {
            const char* name =
                form->GetName();

            info.name =
                name ? name : "";
        }

        // ========================================================
        // QUANTIDADE NO INVENTÁRIO
        // ========================================================

        //info.quantity =
        //    GetItemQuantity(form);

        const QuantityInfo quantities =
            GetItemQuantities(
                form,
                uniqueID,
                hasUniqueID
            );

        info.totalQuantity =
            quantities.totalQuantity;

        info.instanceQuantity =
            quantities.instanceQuantity;

        info.instanceFound =
            quantities.instanceFound;

        // ========================================================
        // TEXTO DE LIVROS
        // ========================================================

        if (auto* book = form->As<RE::TESObjectBOOK>())
        {
            RE::BSString rawBookText;
            book->GetDescription(rawBookText, book);
            if (const char* text = rawBookText.c_str(); text && text[0] != '\0')
                info.bookText = ExtractPlainBookText(text);
        }

        // ========================================================
        // EXTRAI ATRIBUTOS DE ARMAS
        // ========================================================

                
        if (auto* weapon =
            form->As<RE::TESObjectWEAP>())
        {
            // ========================================================
            // ATRIBUTOS BÁSICOS
            // ========================================================

            info.damage =
                static_cast<float>(
                    weapon->GetAttackDamage()
                );

            info.attackSpeed =
                weapon->GetSpeed();

            info.reach =
                weapon->GetReach();

            info.stagger =
                weapon->GetStagger();

            info.criticalDamage =
                static_cast<float>(
                    weapon->GetCritDamage()
                );

            info.weaponType =
                weapon->GetWeaponType();

            info.weight =
                weapon->weight;

            info.value =
                weapon->value;

            // ========================================================
            // IDENTIFICAÇÃO DA INSTÂNCIA
            // ========================================================

            RE::ExtraDataList* extra =
                GetItemInstanceExtra(
                    form,
                    uniqueID,
                    hasUniqueID
                );

            // ========================================================
            // MELHORIA / TEMPERAMENTO
            // ========================================================

            if (extra)
            {
                if (const auto* health =
                    extra->GetByType<RE::ExtraHealth>())
                {
                    info.itemHealth =
                        health->health;

                    info.hasItemHealth = true;
                }
            }

            info.hasPhysicalStats = true;
        }

        // ========================================================
        // EXTRAI ATRIBUTOS DE ARMADURA
        // ========================================================

        if (auto* armor = form->As<RE::TESObjectARMO>())
        {
            info.weight = armor->weight;

            info.value = armor->value;

            info.armorRating =
                armor->GetArmorRating();

            info.hasPhysicalStats = true;
        }

        // ========================================================
        // EXTRAI ATRIBUTOS DE MAGIAS
        // ========================================================

        if (auto* spell = form->As<RE::SpellItem>())
        {
            info.magickaCost =
                spell->CalculateMagickaCost(
                    RE::PlayerCharacter::GetSingleton()
                );

            info.chargeTime =
                spell->GetChargeTime();

            info.castDuration =
                spell->GetFixedCastDuration();

            info.range =
                spell->GetRange();

            info.magicSchool =
                GetMagicSchoolName(
                    spell->GetAssociatedSkill()
                );

            info.effects =
                ExtractMagicEffects(spell);

            info.hasMagicStats = true;
        }

        //INGREDIENTS
        if (auto* ingredient =
            form->As<RE::IngredientItem>())
        {
            info.weight =
                ingredient->weight;

            info.value =
                ingredient->value;

            info.effects =
                ExtractKnownIngredientEffects(
                    ingredient
                );

            info.hasPhysicalStats = true;
            info.hasMagicStats = true;
        }

        if (form->As<RE::TESObjectWEAP>() ||
            form->As<RE::TESObjectARMO>())
        {
            RE::ExtraDataList* extra =
                GetItemInstanceExtra(
                    form,
                    uniqueID,
                    hasUniqueID
                );

            info.enchantment =
                GetItemEnchantment(
                    form,
                    extra
                );

            info.isEnchanted =
                info.enchantment.formID != 0;
        }
            
        return info;
    }
}
