#include "Item.h"
#include "RenderManager.h"
#include "Resolution.h"
#include "Config.h"

#include "Logger.h"

namespace ItemPreview
{
    namespace
    {
        bool g_visible = false;
        
        RE::TESForm* g_form = nullptr;
        std::uint16_t g_uniqueID = 0;
        bool g_hasUniqueID = false;

        RE::NiAVObject* g_attachedModel = nullptr;

        static RE::NiAVObject* s_debugModel = nullptr;
        static int s_debugFrames = 0;

        std::unique_ptr<RE::InventoryEntryData> g_currentEntry = nullptr;

        // ============================================================
        // PROFUNDIDADE DA CENA (itemPos.y)
        // ============================================================
        //
        // A cena de inventario do Skyrim usa itemPos.y como PROFUNDIDADE
        // relativa a camera, nao como "distancia positiva pra frente".
        // O valor vanilla gira em torno de -500. Um valor positivo pequeno
        // (150, como estava antes) coloca o modelo fora da regiao que o
        // frustum da cena projeta corretamente -- ele carrega (radius > 0,
        // confirmado no log), mas nunca aparece na tela.
        constexpr float kSceneDepth = -500.0f;

        float g_itemPosX = 0.0f;
        float g_itemPosZ = 0.0f;
        float g_itemScale = 1.0f;
        float g_sizeScale = 1.0f;

        float g_rotationX = 0.0f;
        float g_rotationY = 0.0f;
        float g_rotationZ = 0.0f;

        RE::NiMatrix3 g_baseRotation;
        bool g_baseRotationCaptured = false;

        float g_rotation = 0.0f;
        float g_rotationDirection = 1.0f;
        constexpr float kRotationSpeed = 0.012f;

        // Onde no HUD (pixels de tela) o item deve projetar.
        ImVec2 g_hudPos{ 0.0f, 0.0f };
        bool g_hasHudPos = false;
        bool g_hudPositionDirty = true;

        bool g_modelReady = false;

        // Alguns previews de magia atualizam o world bound enquanto seus
        // efeitos visuais animam. A escala precisa sempre partir do bound
        // inicial do modelo, nunca do bound já animado/escalado.
        RE::NiAVObject* g_normalizationModel = nullptr;
        float g_normalizationBaseRadius = 0.0f;
        float g_normalizationBaseScale = 1.0f;

        // Âncora independente do world bound. Efeitos de magia podem mover
        // o bound visual durante a animação; isso não deve reposicionar o
        // objeto quando apenas sua escala está sendo editada.
        RE::NiAVObject* g_centerAnchorModel = nullptr;
        RE::NiPoint3 g_centerAnchorTranslation{};
        float g_centerAnchorHudX = 0.0f;
        float g_centerAnchorHudZ = 0.0f;


        static constexpr float kPreloadScale = 0.01f;
        static constexpr float kTargetRadius = 10.0f;

        // ============================================================
        // HELPERS
        // ============================================================
        

        RE::InventoryEntryData* PrepareInventoryEntry(RE::TESForm* form)
        {
            if (!form)
                return nullptr;

            auto* bound = form->As<RE::TESBoundObject>();
            if (!bound)
                return nullptr;

            auto* player = RE::PlayerCharacter::GetSingleton();
            if (player)
            {
                auto inventory = player->GetInventory();
                auto it = inventory.find(bound);

                if (it != inventory.end() && it->second.second)
                {
                    g_currentEntry = std::make_unique<RE::InventoryEntryData>(*it->second.second);
                    return g_currentEntry.get();
                }
            }

            g_currentEntry = std::make_unique<RE::InventoryEntryData>(bound, 1);
            return g_currentEntry.get();
        }

        RE::NiAVObject* FindLoadedModel(RE::TESForm* form)
        {
            if (!form)
                return nullptr;

            auto* manager = RE::Inventory3DManager::GetSingleton();
            if (!manager)
                return nullptr;

            int index = 0;

            for (auto& loaded : manager->GetRuntimeData().loadedModels)
            {
                if (loaded.itemBase == form && loaded.spModel)
                {
                    return loaded.spModel.get();
                }

                ++index;
            }

            return nullptr;
        }

        bool AttachModelToUIScene(
            RE::TESForm* form,
            RE::INTERFACE_LIGHT_SCHEME scheme)
        {
            if (!form)
                return false;

            auto* manager = RE::Inventory3DManager::GetSingleton();
            if (!manager)
                return false;

            auto* scene = RE::UI3DSceneManager::GetSingleton();
            if (!scene)
                return false;

            auto* model = FindLoadedModel(form);

            if (!model)
                return false;

            // Evita anexar duas vezes o mesmo modelo.
            if (g_attachedModel == model)
                return true;

            // Se existia outro modelo, remove primeiro.
            if (g_attachedModel)
            {
                scene->DetachChild(g_attachedModel);
                g_attachedModel = nullptr;
            }

            scene->AttachChild(model, scheme);

            Logger::GetSingleton().Print(
                "AFTER ATTACH: "
                "m0=0x{:X} m1=0x{:X} m2=0x{:X} m3=0x{:X} "
                "m4=0x{:X} m5=0x{:X} m6=0x{:X} m7=0x{:X}",
                reinterpret_cast<std::uintptr_t>(scene->menuObjects[0].get()),
                reinterpret_cast<std::uintptr_t>(scene->menuObjects[1].get()),
                reinterpret_cast<std::uintptr_t>(scene->menuObjects[2].get()),
                reinterpret_cast<std::uintptr_t>(scene->menuObjects[3].get()),
                reinterpret_cast<std::uintptr_t>(scene->menuObjects[4].get()),
                reinterpret_cast<std::uintptr_t>(scene->menuObjects[5].get()),
                reinterpret_cast<std::uintptr_t>(scene->menuObjects[6].get()),
                reinterpret_cast<std::uintptr_t>(scene->menuObjects[7].get()));

            g_attachedModel = model;

            Logger::GetSingleton().Print(
                "ATTACH TEST: model=0x{:X}",
                reinterpret_cast<std::uintptr_t>(model));

            Logger::GetSingleton().Print(
                "ItemPreview: modelo anexado a UI3DSceneManager. FormID={:08X}",
                form->GetFormID()
            );

            return true;
        }

        static void NormalizeModelScale(RE::NiAVObject* model)
        {
            if (!model)
                return;

            model->UpdateWorldBound();

            const float currentRadius =
                model->worldBound.radius;

            if (!std::isfinite(currentRadius) ||
                currentRadius <= 0.000001f)
            {
                return;
            }

            if (g_normalizationModel != model ||
                !std::isfinite(g_normalizationBaseRadius) ||
                g_normalizationBaseRadius <= 0.000001f)
            {
                g_normalizationModel = model;
                g_normalizationBaseRadius = currentRadius;
                g_normalizationBaseScale = model->local.scale;
            }

            // ============================================================
            // AJUSTE VISUAL POR CATEGORIA
            // ============================================================

            float visualFactor = 1.0f;
            Config::ItemPreviewCategory category =
                Config::ItemPreviewCategory::Misc;

            if (g_form)
            {
                if (g_form->As<RE::AlchemyItem>())
                {
                    visualFactor = 0.55f;
                    category = Config::ItemPreviewCategory::Potion;
                }
                // ScrollItem herda SpellItem. Ele precisa ser identificado
                // antes para que seu próprio multiplicador seja aplicado.
                else if (g_form->As<RE::ScrollItem>())
                {
                    visualFactor = 1.0f;
                    category = Config::ItemPreviewCategory::Scroll;
                }
                else if (g_form->As<RE::SpellItem>())
                {
                    visualFactor = 1.50f;
                    category = Config::ItemPreviewCategory::Spell;
                }
                else if (g_form->As<RE::TESObjectWEAP>())
                {
                    visualFactor = 1.2f;
                    category = Config::ItemPreviewCategory::Weapon;
                }
                else if (g_form->As<RE::TESObjectARMO>())
                {
                    visualFactor = 1.0f;
                    category = Config::ItemPreviewCategory::Armor;
                }
                else if (g_form->As<RE::TESAmmo>())
                {
                    visualFactor = 1.2f;
                    category = Config::ItemPreviewCategory::Ammo;
                }
                else if (g_form->As<RE::TESObjectBOOK>())
                {
                    visualFactor = 1.1f;
                    category = Config::ItemPreviewCategory::Book;
                }
                else if (g_form->As<RE::TESKey>())
                {
                    visualFactor = 1.0f;
                    category = Config::ItemPreviewCategory::Key;
                }
                else if (g_form->As<RE::TESSoulGem>())
                {
                    visualFactor = 1.0f;
                    category = Config::ItemPreviewCategory::SoulGem;
                }
                else if (g_form->As<RE::IngredientItem>())
                {
                    visualFactor = 1.1f;
                    category = Config::ItemPreviewCategory::Ingredient;
                }
                else if (g_form->As<RE::TESObjectMISC>())
                {
                    visualFactor = 1.0f;
                    category = Config::ItemPreviewCategory::Misc;
                }
            }

            visualFactor *= Config::GetItemPreviewCategoryMultiplier(category);

            const float targetRadius =
                kTargetRadius * visualFactor * g_sizeScale;

            // ============================================================
            // NORMALIZAÇÃO
            // ============================================================

            const float newScale = g_normalizationBaseScale *
                (targetRadius / g_normalizationBaseRadius);

            model->local.scale = newScale;

            RE::NiUpdateData updateData;
            model->Update(updateData);

            //Logger::GetSingleton().Print(
            //    "BOUND NORMALIZE: "
            //    "radiusBefore={} "
            //    "target={} "
            //    "factor={} "
            //    "oldScale={} "
            //    "multiplier={} "
            //    "newScale={} "
            //    "radiusAfter={}",
            //    currentRadius,
            //    targetRadius,
            //    visualFactor,
            //    oldScale,
            //    multiplier,
            //    newScale,
            //    model->worldBound.radius
            //);

            g_scaleNormalized = true;
        }

        void ConfigureManager()
        {
            auto* manager =
                RE::Inventory3DManager::GetSingleton();

            if (!manager)
                return;

            manager->itemPos =
                RE::NiPoint3(
                    g_itemPosX,
                    kSceneDepth,
                    g_itemPosZ
                );

            manager->itemPosCopy =
                manager->itemPos;
        }

        // ============================================================
        // POSICIONAMENTO NA TELA
        // ============================================================
        //
        // Converte um ponto de tela (pixels) para x/z de mundo na profundidade
        // kSceneDepth, usando o viewFrustum da cena 3D da UI. Sem isso, x=0/z=0
        // nao necessariamente cai no centro da tela -- depende da geometria
        // do frustum daquela cena especifica.
        void ApplyHudPosition()
        {
            if (!g_hasHudPos)
                return;

            auto* scn = RE::UI3DSceneManager::GetSingleton();
            if (!scn)
                return;

            const auto sz = RE::BSGraphics::Renderer::GetScreenSize();
            if (sz.width <= 0 || sz.height <= 0)
                return;

            const auto& vf = scn->viewFrustum;
            const float ty = kSceneDepth;

            const float world_minx = -vf.fLeft * ty;
            const float world_minz = -vf.fBottom * ty;
            const float world_width  = -vf.fRight * ty - world_minx;
            const float world_height = -vf.fTop * ty - world_minz;

            if (world_width == 0.0f || world_height == 0.0f)
                return;

            // Keep the requested position virtual.  Re-evaluate it here so a
            // live resolution change moves an already-visible model without
            // waiting for a new selection.
            const ImVec2 realHudPos = Resolution::ToReal(g_hudPos);
            const float ratio_x = world_width / static_cast<float>(sz.width);
            const float ratio_y = world_height / static_cast<float>(sz.height);

            g_itemPosX = -realHudPos.x * ratio_x - world_minx;
            g_itemPosZ = -realHudPos.y * ratio_y - world_minz;
        }

        static void CenterModelOnHud(RE::NiAVObject* model)
        {
            if (!model)
                return;

            if (g_centerAnchorModel != model)
            {
                model->UpdateWorldBound();
                const RE::NiPoint3 boundCenter = model->worldBound.center;
                model->local.translate.x += g_itemPosX - boundCenter.x;
                model->local.translate.z += g_itemPosZ - boundCenter.z;
                g_centerAnchorModel = model;
                g_centerAnchorTranslation = model->local.translate;
                g_centerAnchorHudX = g_itemPosX;
                g_centerAnchorHudZ = g_itemPosZ;
            }
            else
            {
                // Mantém o ponto de ancoragem inicial e aplica somente o
                // deslocamento pedido pelo HUD. Assim o bound animado de uma
                // Spell não consegue arrastar o preview para fora da tela.
                model->local.translate.x = g_centerAnchorTranslation.x +
                    (g_itemPosX - g_centerAnchorHudX);
                model->local.translate.z = g_centerAnchorTranslation.z +
                    (g_itemPosZ - g_centerAnchorHudZ);
            }

            RE::NiUpdateData updateData;
            model->Update(updateData);
        }

        void ApplyPreviewTransform()
        {
            ApplyHudPosition();
            ConfigureManager();
        }

        //MEASURE LOGGER POR TEMPO (MS)
        //template <class Func>
        //double MeasureMs(Func&& func)
        //{
        //    const auto begin =
        //        std::chrono::steady_clock::now();

        //    std::forward<Func>(func)();

        //    const auto end =
        //        std::chrono::steady_clock::now();

        //    return std::chrono::duration<double, std::milli>(
        //        end - begin
        //    ).count();
        //}

        static RE::ExtraDataList* GetItemExtraByUniqueID(
            RE::TESBoundObject* object,
            std::uint16_t uniqueID,
            bool hasUniqueID)
        {
            if (!object || !hasUniqueID)
                return nullptr;

            auto* player =
                RE::PlayerCharacter::GetSingleton();

            if (!player)
                return nullptr;

            auto inventory = player->GetInventory();

            auto it = inventory.find(object);

            if (it == inventory.end())
                return nullptr;

            auto& entry = it->second.second;

            if (!entry || !entry->extraLists)
                return nullptr;

            for (auto* extra : *entry->extraLists)
            {
                if (!extra)
                    continue;

                auto* unique =
                    extra->GetByType<RE::ExtraUniqueID>();

                if (!unique)
                    continue;

                if (unique->uniqueID == uniqueID &&
                    unique->baseID == player->GetFormID())
                {
                    return extra;
                }
            }

            return nullptr;
        }

        bool BeginPreview(
            RE::TESForm* form,
            std::uint16_t uniqueID,
            bool hasUniqueID
        )
        {
            if (!form)
                return false;

            auto* manager =
                RE::Inventory3DManager::GetSingleton();

            if (!manager)
                return false;

            auto* bound = form->As<RE::TESBoundObject>();

            if (!bound)
                return false;

            // ============================================================
            // LOCALIZA A INSTÂNCIA EXATA
            // ============================================================

            RE::ExtraDataList* extra = nullptr;

            if (hasUniqueID)
            {
                extra = GetItemExtraByUniqueID(
                    bound,
                    uniqueID,
                    hasUniqueID
                );

                // Não carregar outra instância por engano.
                if (!extra)
                {
                    Logger::GetSingleton().Print(
                        "ItemPreview: Instancia nao encontrada | form={:08X} | uniqueID={}",
                        form->GetFormID(),
                        uniqueID
                    );

                    return false;
                }
            }

            // ============================================================
            // INICIA O PREVIEW 3D
            // ============================================================

            const auto scheme =
                form->As<RE::SpellItem>()
                ? RE::INTERFACE_LIGHT_SCHEME::kInventoryMagic
                : RE::INTERFACE_LIGHT_SCHEME::kInventory;

            manager->Begin3D(scheme);

            ApplyPreviewTransform();

            g_scaleNormalized = false;

            // ============================================================
            // PREPARA O MODELO
            // ============================================================

            // O primeiro Render() precisa acontecer para criar o modelo.
            // Fazemos o modelo nascer praticamente invisível.

            manager->itemScale = kPreloadScale;
            manager->itemScaleCopy = kPreloadScale;

            // ============================================================
            // CARREGA A INSTÂNCIA SELECIONADA
            // ============================================================

            manager->LoadInventoryItem(
                bound,
                extra
            );

            // ============================================================
            // REINICIA O ESTADO DO PREVIEW
            // ============================================================

            g_modelReady = false;
            g_baseRotationCaptured = false;
            g_rotation = 0.0f;

            return true;
        }




    }

    

    void SetHudPosition(ImVec2 a_screenPos)
    {
        // UI callers use virtual coordinates. ApplyHudPosition converts it at
        // projection time so the preview follows live viewport changes.
        if (std::abs(a_screenPos.x - g_hudPos.x) > 0.001f ||
            std::abs(a_screenPos.y - g_hudPos.y) > 0.001f)
        {
            g_hudPositionDirty = true;
        }
        g_hudPos = a_screenPos;
        g_hasHudPos = true;
    }

    void SetSizeScale(float a_scale)
    {
        const float next = std::clamp(a_scale, 0.01f, 5.0f);
        if (std::abs(next - g_sizeScale) <= 0.0001f)
            return;
        g_sizeScale = next;
        // A normalização usa o bound atual, portanto pode ser reaplicada
        // continuamente sem acumular escala quando o slider se move.
        g_scaleNormalized = false;
    }

    void InvalidateSizeScale()
    {
        g_scaleNormalized = false;
    }

    bool IsReady()
    {
        return g_modelReady;
    }

    bool Show(
        RE::TESForm* form,
        std::uint16_t uniqueID,
        bool hasUniqueID)
    {
        if (!form)
            return false;

        auto* manager = RE::Inventory3DManager::GetSingleton();
        if (!manager)
            return false;

        if (g_visible &&
            g_form == form &&
            g_uniqueID == uniqueID &&
            g_hasUniqueID == hasUniqueID
        )
        {
            // Mesmo item: não recarrega, mas mantém o manager
            // sincronizado com a posição atual do HUD.
            ApplyPreviewTransform();

            auto* model = FindLoadedModel(form);

            return model != nullptr;
        }

        if (g_visible)
        {

            auto* scene = RE::UI3DSceneManager::GetSingleton();

            if (scene && g_attachedModel)
            {
                scene->DetachChild(g_attachedModel);
                g_attachedModel = nullptr;
            }

            manager->UnloadInventoryItem();
            manager->End3D();

            g_currentEntry.reset();
            g_visible = false;
            g_form = nullptr;
            g_modelReady = false;
        }

        // Cada preview recém-carregado ganha sua própria referência estável
        // de normalização; ponteiros de modelos podem ser reutilizados pelo
        // gerenciador de inventário.
        g_normalizationModel = nullptr;
        g_normalizationBaseRadius = 0.0f;
        g_normalizationBaseScale = 1.0f;
        g_centerAnchorModel = nullptr;
        g_centerAnchorTranslation = {};
        g_centerAnchorHudX = 0.0f;
        g_centerAnchorHudZ = 0.0f;

        if (!BeginPreview(
            form,
            uniqueID,
            hasUniqueID
        ))
        {
            Logger::GetSingleton().Print(
                "ItemPreview: Falha ao iniciar preview 3D para o FormID {:08X}",
                form->GetFormID()
            );
            g_currentEntry.reset();
            return false;
        }

        g_form = form;

        g_uniqueID = uniqueID;
        g_hasUniqueID = hasUniqueID;

        g_visible = true;

        return true;
    }

    bool LoadInFlight(RE::Inventory3DManager* mgr)
    {
        auto& rt = mgr->GetRuntimeData();
        if (rt.loadTask)
            return true;

        for (auto& lm : rt.loadedModels)
        {
            if (!lm.spModel)
                return true;
        }

        return false;
    }

    void TeardownWhenIdle(int tries = 0)
    {
        auto* manager = RE::Inventory3DManager::GetSingleton();
        if (!manager)
            return;

        if (LoadInFlight(manager))
        {
            if (tries >= 300) // ~5s a 60fps, desiste e deixa a cena como esta
            {
                Logger::GetSingleton().Print(
                    "ItemPreview: load travado, teardown abortado"
                );
                return;
            }

            SKSE::GetTaskInterface()->AddTask([tries]() {
                TeardownWhenIdle(tries + 1);
            });
            return;
        }

        manager->UnloadInventoryItem();

        // reconfere DEPOIS do unload -- Unload pode disparar
        // um novo load em alguns casos
        if (LoadInFlight(manager))
        {
            SKSE::GetTaskInterface()->AddTask([tries]() {
                TeardownWhenIdle(tries + 1);
            });
            return;
        }

        manager->End3D();
    }

    void Update()
    {
        if (!g_visible || !g_form)
            return;

        auto* manager = RE::Inventory3DManager::GetSingleton();

        if (!manager)
            return;

        // Mantém posição/escala do preview atualizadas
        ApplyPreviewTransform();

        auto* model = FindLoadedModel(g_form);

        // Modelo ainda está carregando
        if (!model)
        {
            // Necessário para efetivamente criar certos previews.
            manager->Render();

            model = FindLoadedModel(g_form);

            if (!model)
            {
                g_modelReady = false;
                return;
            }
        }

        // Modelo existe, mas ainda não terminou de inicializar
        if (model->worldBound.radius <= 0.0f)
        {
            manager->Render();
            g_modelReady = false;
            return;
        }

        // Normaliza e/ou move o modelo já carregado. Alterar X/Y não troca o
        // item nem a escala, então precisa de uma invalidação própria.
        const bool needsCentering = !g_scaleNormalized || g_hudPositionDirty;
        if (!g_scaleNormalized)
        {
            NormalizeModelScale(model);
        }
        if (needsCentering)
        {
            CenterModelOnHud(model);
            g_hudPositionDirty = false;
        }

        // Rotação contínua
        if (g_rotationDirection != 0.0f)
        {
            RE::NiMatrix3 deltaRotation;

            float rotationDirection = g_rotationDirection;

            // AlchemyItem gira invertido em relação aos demais previews.
            if (g_form && g_form->As<RE::IngredientItem>())
            {
                rotationDirection *= -1.0f;
            }

            deltaRotation.SetEulerAnglesXYZ(
                0.0f,
                0.0f,
                kRotationSpeed * rotationDirection
            );

            model->local.rotate =
                deltaRotation * model->local.rotate;

            RE::NiUpdateData updateData;
            model->Update(updateData);
        }

        g_modelReady = true;

        // Renderiza o preview
        manager->Render();
    }

    void SetRotationDirection(float a_direction)
    {
        if (a_direction > 0.0f)
            g_rotationDirection = 1.0f;
        else if (a_direction < 0.0f)
            g_rotationDirection = -1.0f;
        else
            g_rotationDirection = 0.0f;
    }

    void Hide()
    {
        if (!g_visible)
            return;

        auto* scene = RE::UI3DSceneManager::GetSingleton();

        if (scene && g_attachedModel)
        {
            scene->DetachChild(g_attachedModel);
            g_attachedModel = nullptr;
        }

        g_currentEntry.reset();
        g_visible = false;
        g_form = nullptr;
        g_modelReady = false;
        g_uniqueID = 0;
        g_hasUniqueID = false;

        TeardownWhenIdle();
    }

    bool IsVisible()
    {
        return g_visible;
    }

    RE::TESForm* GetCurrentForm()
    {
        return g_form;
    }

}
