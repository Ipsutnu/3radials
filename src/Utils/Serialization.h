#pragma once

#include <SKSE/SKSE.h>

namespace Serialization
{
    void Register();

    void SaveCallback(
        SKSE::SerializationInterface* a_intfc);

    void LoadCallback(
        SKSE::SerializationInterface* a_intfc);

    void RevertCallback(
        SKSE::SerializationInterface* a_intfc);
}