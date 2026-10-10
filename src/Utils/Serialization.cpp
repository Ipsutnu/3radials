
#include "Serialization.h"
#include "Logger.h"
#include "Config.h"
#include "PCH.h"
#include "Animation/Track/TrackEditor.h"

#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>

#include <cstdint>
#include <vector>
#include <utility>

namespace Serialization
{
    // ============================================================
    // IDENTIFICAÇÃO
    // ============================================================

    constexpr std::uint32_t kSerializationID = 'WHLW';
    constexpr std::uint32_t kRecordRadials    = 'RADS';
    constexpr std::uint32_t kRecordQuickDraw  = 'QDRW';
    constexpr std::uint32_t kQuickDrawVersion = 1;

    // Versão antiga: apenas FormID.
    constexpr std::uint32_t kVersionLegacy = 1;

    // Versão nova: FormID + UniqueID + hasUniqueID.
    constexpr std::uint32_t kVersion = 2;

    // Proteção contra dados inválidos ou corrompidos.
    constexpr std::uint32_t kMaxRadialItems = 4096;

    constexpr std::uint32_t kRadialLockRecord =
        'W' |
        ('L' << 8) |
        ('C' << 16) |
        ('K' << 24);

    // ============================================================
    // SERIALIZATION - RADIAL LOCKS
    // ============================================================

    constexpr std::uint32_t kRadialLockVersion = 5;

    struct RadialLockSaveData
    {
        std::uint8_t topLocked = 0;
        std::uint8_t bottomLocked = 0;
        std::uint8_t sideLocked = 0;

        // NOVO - LIBERAÇÃO DO MOVIMENTO DO MOUSE
        std::uint8_t topMouseUnlocked = 0;
        std::uint8_t bottomMouseUnlocked = 0;
        std::uint8_t sideScrollLocked = 1;
        std::uint8_t overflowEraserSelected = 0;
        std::uint8_t sideMouseLocked = 1;
    };


    // ============================================================
    // LIMPA AS LISTAS
    // ============================================================

    static void ClearRadials()
    {
        Menu::g_sideItems.clear();
        Menu::g_topItems.clear();
        Menu::g_bottomItems.clear();
    }

    void SaveRadialLocks(
        SKSE::SerializationInterface* serialization)
    {
        if (!serialization)
            return;

        RadialLockSaveData data{};

        // ========================================================
        // TRAVAS DOS RADIAIS
        // ========================================================

        data.topLocked =
            g_lockTopRadial ? 1 : 0;

        data.bottomLocked =
            g_lockBottomRadial ? 1 : 0;

        data.sideLocked =
            g_lockSideRadial ? 1 : 0;

        // ========================================================
        // LIBERAÇÃO DO MOVIMENTO DO MOUSE
        // ========================================================

        data.topMouseUnlocked =
            g_unlockTopMouse ? 1 : 0;

        data.bottomMouseUnlocked =
            g_unlockBottomMouse ? 1 : 0;

        data.sideScrollLocked =
            Config::g_lockSideScroll ? 1 : 0;
        data.overflowEraserSelected =
            TrackEditor::IsOverflowEraserSelected() ? 1 : 0;
        data.sideMouseLocked = g_lockSideMouse ? 1 : 0;

        // ========================================================
        // ABRE O REGISTRO
        // ========================================================

        if (!serialization->OpenRecord(
            kRadialLockRecord,
            kRadialLockVersion))
        {
            Logger::GetSingleton().Print(
                "SERIALIZATION | Failed to open radial lock record"
            );

            return;
        }

        // ========================================================
        // SALVA
        // ========================================================

        if (!serialization->WriteRecordData(
            &data,
            sizeof(data)))
        {
            Logger::GetSingleton().Print(
                "SERIALIZATION | Failed to save radial locks"
            );

            return;
        }

        Logger::GetSingleton().Print(
            "SERIALIZATION | Locks saved | "
            "TOP={} BOTTOM={} SIDE={} | "
            "MOUSE TOP={} BOTTOM={} SIDE={} | SCROLL={}",
            data.topLocked,
            data.bottomLocked,
            data.sideLocked,
            data.topMouseUnlocked,
            data.bottomMouseUnlocked,
            data.sideMouseLocked,
            data.sideScrollLocked
        );
    }

    void LoadRadialLocks(
        SKSE::SerializationInterface* serialization,
        std::uint32_t version,
        std::uint32_t length)
    {
        if (!serialization)
            return;

        // ========================================================
        // VALIDA A VERSÃO
        // ========================================================

        if (version != 1 && version != 2 && version != 3 && version != 4 &&
            version != kRadialLockVersion)
        {
            Logger::GetSingleton().Print(
                "SERIALIZATION | Unsupported radial lock version={}",
                version
            );

            return;
        }

        // ========================================================
        // VERSÃO ANTIGA - SOMENTE AS TRÊS TRAVAS
        // ========================================================

        if (version == 1)
        {
            struct LegacyRadialLockSaveData
            {
                std::uint8_t topLocked;
                std::uint8_t bottomLocked;
                std::uint8_t sideLocked;
            };

            if (length != sizeof(LegacyRadialLockSaveData))
            {
                Logger::GetSingleton().Print(
                    "SERIALIZATION | Invalid legacy radial lock record"
                );

                return;
            }

            LegacyRadialLockSaveData data{};

            if (serialization->ReadRecordData(
                &data,
                sizeof(data)) != sizeof(data))
            {
                Logger::GetSingleton().Print(
                    "SERIALIZATION | Failed to load legacy radial locks"
                );

                return;
            }

            g_lockTopRadial =
                data.topLocked != 0;

            g_lockBottomRadial =
                data.bottomLocked != 0;

            g_lockSideRadial =
                data.sideLocked != 0;

            // Opções novas permanecem desativadas em saves antigos.
            g_unlockTopMouse = false;
            g_unlockBottomMouse = false;
            g_lockSideMouse = true;

            Logger::GetSingleton().Print(
                "SERIALIZATION | Legacy locks loaded | "
                "TOP={} BOTTOM={} SIDE={}",
                g_lockTopRadial,
                g_lockBottomRadial,
                g_lockSideRadial
            );

            return;
        }

        // ========================================================
        // VERSÃO 2 - TRAVAS + MOVIMENTO DO MOUSE
        // ========================================================

        if (version == 2)
        {
            struct Version2RadialLockSaveData
            {
                std::uint8_t topLocked;
                std::uint8_t bottomLocked;
                std::uint8_t sideLocked;
                std::uint8_t topMouseUnlocked;
                std::uint8_t bottomMouseUnlocked;
            };
            if (length != sizeof(Version2RadialLockSaveData)) return;
            Version2RadialLockSaveData data{};
            if (serialization->ReadRecordData(&data, sizeof(data)) != sizeof(data)) return;
            g_lockTopRadial = data.topLocked != 0;
            g_lockBottomRadial = data.bottomLocked != 0;
            g_lockSideRadial = data.sideLocked != 0;
            g_unlockTopMouse = data.topMouseUnlocked != 0;
            g_unlockBottomMouse = data.bottomMouseUnlocked != 0;
            Config::g_lockSideScroll = true;
            g_lockSideMouse = true;
            return;
        }

        if (version == 3)
        {
            struct VersionRadialLockSaveData
            {
                std::uint8_t topLocked;
                std::uint8_t bottomLocked;
                std::uint8_t sideLocked;
                std::uint8_t topMouseUnlocked;
                std::uint8_t bottomMouseUnlocked;
                std::uint8_t sideScrollLocked;
            };
            if (length != sizeof(VersionRadialLockSaveData)) return;
            VersionRadialLockSaveData data{};
            if (serialization->ReadRecordData(&data, sizeof(data)) != sizeof(data)) return;
            g_lockTopRadial = data.topLocked != 0;
            g_lockBottomRadial = data.bottomLocked != 0;
            g_lockSideRadial = data.sideLocked != 0;
            g_unlockTopMouse = data.topMouseUnlocked != 0;
            g_unlockBottomMouse = data.bottomMouseUnlocked != 0;
            Config::g_lockSideScroll = data.sideScrollLocked != 0;
            g_lockSideMouse = true;
            TrackEditor::SetOverflowEraserSelected(false);
            return;
        }

        // Versão 4: Lock Scroll + seletor de borracha, antes do Lock Cam do Side.
        if (version == 4)
        {
            struct Version4RadialLockSaveData
            {
                std::uint8_t topLocked;
                std::uint8_t bottomLocked;
                std::uint8_t sideLocked;
                std::uint8_t topMouseUnlocked;
                std::uint8_t bottomMouseUnlocked;
                std::uint8_t sideScrollLocked;
                std::uint8_t overflowEraserSelected;
            };
            if (length != sizeof(Version4RadialLockSaveData)) return;
            Version4RadialLockSaveData data{};
            if (serialization->ReadRecordData(&data, sizeof(data)) != sizeof(data)) return;
            g_lockTopRadial = data.topLocked != 0;
            g_lockBottomRadial = data.bottomLocked != 0;
            g_lockSideRadial = data.sideLocked != 0;
            g_unlockTopMouse = data.topMouseUnlocked != 0;
            g_unlockBottomMouse = data.bottomMouseUnlocked != 0;
            Config::g_lockSideScroll = data.sideScrollLocked != 0;
            TrackEditor::SetOverflowEraserSelected(data.overflowEraserSelected != 0);
            g_lockSideMouse = true;
            return;
        }

        // ========================================================
        // VERSÃO ATUAL - INCLUI LOCK SCROLL
        // ========================================================

        if (length != sizeof(RadialLockSaveData))
        {
            Logger::GetSingleton().Print(
                "SERIALIZATION | Invalid radial lock record | length={}",
                length
            );

            return;
        }

        RadialLockSaveData data{};

        if (serialization->ReadRecordData(
            &data,
            sizeof(data)) != sizeof(data))
        {
            Logger::GetSingleton().Print(
                "SERIALIZATION | Failed to load radial locks"
            );

            return;
        }

        // ========================================================
        // RESTAURA AS TRAVAS
        // ========================================================

        g_lockTopRadial =
            data.topLocked != 0;

        g_lockBottomRadial =
            data.bottomLocked != 0;

        g_lockSideRadial =
            data.sideLocked != 0;

        // ========================================================
        // RESTAURA AS OPÇÕES DE MOVIMENTO DO MOUSE
        // ========================================================

        g_unlockTopMouse =
            data.topMouseUnlocked != 0;

        g_unlockBottomMouse =
            data.bottomMouseUnlocked != 0;

        g_lockSideMouse =
            data.sideMouseLocked != 0;

        Config::g_lockSideScroll =
            data.sideScrollLocked != 0;

        TrackEditor::SetOverflowEraserSelected(
            data.overflowEraserSelected != 0);

        // ========================================================
        // LOG
        // ========================================================

        Logger::GetSingleton().Print(
            "SERIALIZATION | Locks loaded | "
            "TOP={} BOTTOM={} SIDE={} | "
            "MOUSE TOP={} BOTTOM={} SIDE={} | SCROLL={}",
            g_lockTopRadial,
            g_lockBottomRadial,
            g_lockSideRadial,
            g_unlockTopMouse,
            g_unlockBottomMouse,
            g_lockSideMouse,
            Config::g_lockSideScroll
        );
    }

    // ============================================================
    // SAVE DE UM VECTOR
    // ============================================================

    static bool SaveRadial(
        SKSE::SerializationInterface* a_intfc,
        const std::vector<Menu::RadialItem>& a_items)
    {
        if (!a_intfc)
            return false;

        if (a_items.size() > kMaxRadialItems)
        {
            spdlog::error(
                "SERIALIZATION SAVE | too many items: {}",
                a_items.size()
            );

            return false;
        }

        const std::uint32_t count =
            static_cast<std::uint32_t>(a_items.size());

        if (!a_intfc->WriteRecordData(
                &count,
                sizeof(count)))
        {
            return false;
        }

        // ========================================================
        // SALVA CADA ITEM
        // ========================================================

        for (const auto& item : a_items)
        {
            const RE::FormID formID =
                item.form
                    ? item.form->GetFormID()
                    : 0;

            const std::uint16_t uniqueID =
                item.uniqueID;

            // Salva como uint8_t para manter
            // o formato binário independente de bool.
            const std::uint8_t hasUniqueID =
                item.hasUniqueID ? 1 : 0;

            // ----------------------------------------------------
            // FORM ID
            // ----------------------------------------------------

            if (!a_intfc->WriteRecordData(
                    &formID,
                    sizeof(formID)))
            {
                return false;
            }

            // ----------------------------------------------------
            // UNIQUE ID
            // ----------------------------------------------------

            if (!a_intfc->WriteRecordData(
                    &uniqueID,
                    sizeof(uniqueID)))
            {
                return false;
            }

            // ----------------------------------------------------
            // HAS UNIQUE ID
            // ----------------------------------------------------

            if (!a_intfc->WriteRecordData(
                    &hasUniqueID,
                    sizeof(hasUniqueID)))
            {
                return false;
            }

            spdlog::info(
                "SERIALIZATION SAVE ITEM | form={:08X} | uniqueID={} | hasUniqueID={}",
                formID,
                uniqueID,
                hasUniqueID != 0
            );
        }

        return true;
    }


    // ============================================================
    // LOAD DE UM VECTOR
    // ============================================================

    static bool LoadRadial(
        SKSE::SerializationInterface* a_intfc,
        std::vector<Menu::RadialItem>& a_items,
        std::uint32_t a_version)
    {
        if (!a_intfc)
            return false;

        std::uint32_t count = 0;

        if (!a_intfc->ReadRecordData(
                &count,
                sizeof(count)))
        {
            return false;
        }

        // ========================================================
        // VALIDA QUANTIDADE
        // ========================================================

        if (count > kMaxRadialItems)
        {
            spdlog::error(
                "SERIALIZATION LOAD | invalid count={}",
                count
            );

            return false;
        }

        a_items.clear();
        a_items.reserve(count);

        // ========================================================
        // CARREGA CADA ITEM
        // ========================================================

        for (std::uint32_t i = 0; i < count; ++i)
        {
            RE::FormID oldFormID = 0;

            std::uint16_t uniqueID = 0;

            std::uint8_t hasUniqueID = 0;

            // ----------------------------------------------------
            // FORM ID
            // ----------------------------------------------------

            if (!a_intfc->ReadRecordData(
                    &oldFormID,
                    sizeof(oldFormID)))
            {
                return false;
            }

            // ----------------------------------------------------
            // UNIQUE ID
            //
            // Somente existe na versão 2.
            // ----------------------------------------------------

            if (a_version >= kVersion)
            {
                if (!a_intfc->ReadRecordData(
                        &uniqueID,
                        sizeof(uniqueID)))
                {
                    return false;
                }

                if (!a_intfc->ReadRecordData(
                        &hasUniqueID,
                        sizeof(hasUniqueID)))
                {
                    return false;
                }

                if (hasUniqueID > 1)
                {
                    spdlog::error(
                        "SERIALIZATION LOAD | invalid identity flag"
                    );

                    return false;
                }
            }

            // ----------------------------------------------------
            // ITEM NULO
            // ----------------------------------------------------

            if (oldFormID == 0)
                continue;

            // ====================================================
            // RESOLVE FORM ID
            //
            // Necessário quando o load order muda.
            // ====================================================

            RE::FormID newFormID = 0;

            if (!a_intfc->ResolveFormID(
                    oldFormID,
                    newFormID))
            {
                spdlog::warn(
                    "SERIALIZATION LOAD | unresolved form={:08X}",
                    oldFormID
                );

                continue;
            }

            RE::TESForm* form =
                RE::TESForm::LookupByID(newFormID);

            if (!form)
            {
                spdlog::warn(
                    "SERIALIZATION LOAD | form not found={:08X}",
                    newFormID
                );

                continue;
            }

            // ====================================================
            // COMPATIBILIDADE COM SAVES ANTIGOS
            //
            // A versão 1 não possui UniqueID.
            //
            // Não podemos reconstruir uma adaga específica
            // usando apenas seu FormID.
            // ====================================================

            if (a_version == kVersionLegacy)
            {
                const bool isWeapon =
                    form->As<RE::TESObjectWEAP>() != nullptr;

                const bool isArmor =
                    form->As<RE::TESObjectARMO>() != nullptr;

                if (isWeapon || isArmor)
                {
                    spdlog::warn(
                        "SERIALIZATION LEGACY | skipping item without identity | form={:08X}",
                        newFormID
                    );

                    continue;
                }
            }

            // ====================================================
            // RECONSTRÓI RADIAL ITEM
            // ====================================================

            Menu::RadialItem item{};

            // ----------------------------------------------------
            // FORM
            // ----------------------------------------------------

            item.form = form;

            // ----------------------------------------------------
            // IDENTIFICAÇÃO INDIVIDUAL
            // ----------------------------------------------------

            item.uniqueID = uniqueID;

            item.hasUniqueID =
                hasUniqueID != 0;

            // ----------------------------------------------------
            // NOME
            //
            // Nome-base. O nome personalizado da instância
            // deve continuar sendo calculado pelo sistema
            // de exibição do radial.
            // ----------------------------------------------------

            const char* name =
                form->GetName();

            item.name =
                name ? name : "";

            // ----------------------------------------------------
            // ÍCONE
            //
            // Texturas não são serializadas.
            // ----------------------------------------------------

            item.icon = ImTextureID(0);

            // ----------------------------------------------------
            // POSIÇÃO
            //
            // Reconstrói a ordem original da lista.
            // ----------------------------------------------------

            item.slot =
                static_cast<int>(a_items.size());

            item.valid = true;

            // ----------------------------------------------------
            // INSERE
            // ----------------------------------------------------

            a_items.push_back(
                std::move(item)
            );

            spdlog::info(
                "SERIALIZATION LOAD ITEM | form={:08X} | uniqueID={} | hasUniqueID={} | slot={}",
                newFormID,
                uniqueID,
                hasUniqueID != 0,
                a_items.back().slot
            );
        }

        return true;
    }


    // ============================================================
    // SAVE CALLBACK
    // ============================================================

    void SaveCallback(
        SKSE::SerializationInterface* a_intfc)
    {
        if (!a_intfc)
            return;

        if (!a_intfc->OpenRecord(
                kRecordRadials,
                kVersion))
        {
            spdlog::error(
                "SERIALIZATION SAVE | OpenRecord failed"
            );

            return;
        }

        // ========================================================
        // A ORDEM PRECISA SER IGUAL NO LOAD
        // ========================================================

        if (!SaveRadial(
                a_intfc,
                Menu::g_sideItems))
        {
            spdlog::error(
                "SERIALIZATION SAVE | side failed"
            );

            return;
        }

        if (!SaveRadial(
                a_intfc,
                Menu::g_topItems))
        {
            spdlog::error(
                "SERIALIZATION SAVE | top failed"
            );

            return;
        }

        if (!SaveRadial(
                a_intfc,
                Menu::g_bottomItems))
        {
            spdlog::error(
                "SERIALIZATION SAVE | bottom failed"
            );

            return;
        }

        // ============================================================
        // SALVA AS TRAVAS DOS RADIAIS EM UM REGISTRO SEPARADO
        // ============================================================

        SaveRadialLocks(a_intfc);

        if (a_intfc->OpenRecord(kRecordQuickDraw, kQuickDrawVersion))
        {
            Menu::SaveQuickDraw(a_intfc);
        }
        else
        {
            spdlog::error("SERIALIZATION SAVE | quick draw OpenRecord failed");
        }

        spdlog::info(
            "SERIALIZATION SAVE COMPLETE | side={} | top={} | bottom={}",
            Menu::g_sideItems.size(),
            Menu::g_topItems.size(),
            Menu::g_bottomItems.size()
        );
    }


    // ============================================================
    // LOAD CALLBACK
    // ============================================================

    void LoadCallback(
        SKSE::SerializationInterface* a_intfc)
    {
        if (!a_intfc)
            return;

        ClearRadials();
        Menu::ClearQuickDraw();

        // ============================================================
        // REDEFINE AS TRAVAS ANTES DO CARREGAMENTO
        // ============================================================

        g_lockTopRadial = false;
        g_lockBottomRadial = false;
        g_lockSideRadial = false;

        g_unlockTopMouse = false;
        g_unlockBottomMouse = false;
        g_lockSideMouse = true;

        Config::g_lockSideScroll = true;

        g_radialToggleLocked = false;

        std::uint32_t type = 0;
        std::uint32_t version = 0;
        std::uint32_t length = 0;

        while (a_intfc->GetNextRecordInfo(
            type,
            version,
            length))
        {
            switch (type)
            {
            // ========================================================
            // REGISTRO DOS RADIAIS
            // ========================================================

            case kRecordRadials:
            {
                // ----------------------------------------------------
                // VALIDA VERSÃO
                // ----------------------------------------------------

                if (version != kVersion &&
                    version != kVersionLegacy)
                {
                    spdlog::warn(
                        "SERIALIZATION LOAD | unsupported version={}",
                        version
                    );

                    break;
                }

                // ====================================================
                // CARREGA EM LISTAS TEMPORÁRIAS
                // ====================================================

                std::vector<Menu::RadialItem> sideItems;
                std::vector<Menu::RadialItem> topItems;
                std::vector<Menu::RadialItem> bottomItems;

                // ----------------------------------------------------
                // SIDE
                // ----------------------------------------------------

                if (!LoadRadial(
                        a_intfc,
                        sideItems,
                        version))
                {
                    spdlog::error(
                        "SERIALIZATION LOAD | side failed"
                    );

                    ClearRadials();
                    return;
                }

                // ----------------------------------------------------
                // TOP
                // ----------------------------------------------------

                if (!LoadRadial(
                        a_intfc,
                        topItems,
                        version))
                {
                    spdlog::error(
                        "SERIALIZATION LOAD | top failed"
                    );

                    ClearRadials();
                    return;
                }

                // ----------------------------------------------------
                // BOTTOM
                // ----------------------------------------------------

                if (!LoadRadial(
                        a_intfc,
                        bottomItems,
                        version))
                {
                    spdlog::error(
                        "SERIALIZATION LOAD | bottom failed"
                    );

                    ClearRadials();
                    return;
                }

                // ====================================================
                // TRANSFERE AS LISTAS RECONSTRUÍDAS
                // ====================================================

                Menu::g_sideItems =
                    std::move(sideItems);

                Menu::g_topItems =
                    std::move(topItems);

                Menu::g_bottomItems =
                    std::move(bottomItems);

                spdlog::info(
                    "SERIALIZATION LOAD COMPLETE | version={} | side={} | top={} | bottom={}",
                    version,
                    Menu::g_sideItems.size(),
                    Menu::g_topItems.size(),
                    Menu::g_bottomItems.size()
                );

                break;
            }

            // ========================================================
            // REGISTRO DAS TRAVAS
            // ========================================================

            case kRadialLockRecord:
            {
                LoadRadialLocks(
                    a_intfc,
                    version,
                    length
                );

                spdlog::info(
                    "SERIALIZATION LOAD | Radial locks loaded"
                );

                break;
            }

            case kRecordQuickDraw:
            {
                if (!Menu::LoadQuickDraw(a_intfc, version, length))
                    spdlog::warn("SERIALIZATION LOAD | quick draw data ignored");
                break;
            }

            // ========================================================
            // REGISTRO DESCONHECIDO
            // ========================================================

            default:
            {
                spdlog::warn(
                    "SERIALIZATION LOAD | Unknown record type={:08X}",
                    type
                );

                break;
            }

            }
        }
    }


    // ============================================================
    // REVERT CALLBACK
    // ============================================================

    void RevertCallback(
        SKSE::SerializationInterface*)
    {
        // ============================================================
        // LIMPA OS RADIAIS
        // ============================================================

        ClearRadials();
        Menu::ClearQuickDraw();

        // ============================================================
        // REDEFINE AS CONFIGURAÇÕES DAS TRAVAS
        // ============================================================

        g_lockTopRadial = false;
        g_lockBottomRadial = false;
        g_lockSideRadial = false;

        g_unlockTopMouse = false;
        g_unlockBottomMouse = false;
        g_lockSideMouse = true;

        // Estado temporário da tecla G.
        g_radialToggleLocked = false;

        // Evita carregar um evento de soltura pendente
        // para o próximo personagem.
        g_ignoreNextGRelease = false;

        spdlog::info(
            "SERIALIZATION REVERT | radials and locks reset"
        );
    }


    // ============================================================
    // REGISTER
    // ============================================================

    void Register()
    {
        auto* serialization =
            SKSE::GetSerializationInterface();

        if (!serialization)
            return;

        serialization->SetUniqueID(
            kSerializationID
        );

        serialization->SetSaveCallback(
            SaveCallback
        );

        serialization->SetLoadCallback(
            LoadCallback
        );

        serialization->SetRevertCallback(
            RevertCallback
        );
    }
}
