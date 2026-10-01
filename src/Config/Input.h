#pragma once

void RegisterInputSink();
// Mantém o sink do WheelWheel antes do PlayerControls. O Skyrim pode
// reconstruir/reordenar essa lista depois do carregamento inicial.
void MaintainInputSinkPriority();
void ResetWheelInputState();

class InputHandler : public RE::BSTEventSink<RE::InputEvent*>
{
public:
    // Retorna a instância única (Singleton)
    static InputHandler* GetSingleton();

    // Registra o EventSink no gerenciador de inputs do Skyrim
    static void Register();

    // Sobrescreve o método de processamento de eventos do CommonLibSSE
    RE::BSEventNotifyControl ProcessEvent(
        RE::InputEvent* const* a_event,
        RE::BSTEventSource<RE::InputEvent*>*) override;

private:
    InputHandler() = default;
    ~InputHandler() = default;

    InputHandler(const InputHandler&) = delete;
    InputHandler(InputHandler&&) = delete;
    InputHandler& operator=(const InputHandler&) = delete;
    InputHandler& operator=(InputHandler&&) = delete;
};
