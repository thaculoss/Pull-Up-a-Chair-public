#pragma once

// Observe selected native function objects without replacing their implementations
// or editing PEX. Each object retains its original dispatch and RTTI entry.
#include <cstring>
#include <memory>

namespace easobserver
{
    namespace script = RE::BSScript;
    using Function = script::NF_util::NativeFunctionBase;
    using VM = script::Internal::VirtualMachine;
    using Variable = script::Variable;
    using Frame = script::StackFrame;
    using Object = RE::BSTSmartPointer<script::Object>;
    enum class Kind
    {
        remote,
        animation,
        legacyTrack,
        legacyBegin,
        legacyEnd
    };

    struct Signal
    {
        Kind kind{};
        std::uint64_t epoch{}, handle{}, sequence{};
        RE::VMStackID stack{};
        RE::FormID food{}, spell{}, actor{};
        Object owner;
        std::string command, caller;
        bool dispatched{};
    };

    using CaptureFn = std::optional<Signal> (*)(Kind, Variable &, const Frame &, RE::VMStackID);
    using PublishFn = void (*)(Signal);

    class NativeObserver
    {
        using Dispatch =
            bool (*)(const Function *, Variable &, VM &, RE::VMStackID, Variable &, const Frame &);
        static constexpr std::size_t count = 5, virtuals = 0x17, dispatchSlot = 0x16;

        struct Hook
        {
            RE::BSTSmartPointer<script::IFunction> keep;
            // The first entry is MSVC's complete object locator, used by RTTI.
            std::array<std::uintptr_t, virtuals + 1> table{};
            Dispatch next{};
            Kind kind{};
        };

        // Process-lifetime objects. They must outlive all engine function calls.
        inline static std::array<std::atomic<Hook *>, count> hooks{};
        inline static CaptureFn capture{};
        inline static PublishFn publish{};

        template <std::size_t Index>
        static bool Call(const Function *self,
                         Variable &base,
                         VM &vm,
                         RE::VMStackID stack,
                         Variable &result,
                         const Frame &frame)
        {
            auto hook = hooks[Index].load(std::memory_order_acquire);
            // Published before the object's vptr changes; never removed.
            std::optional<Signal> signal;
            try
            {
                signal = capture(hook->kind, base, frame, stack);
            }
            catch (const std::exception &e)
            {
                logger::error("CAPTURE_ERROR {}", e.what());
            }
            // The original is called exactly once, with every argument unchanged.
            const bool dispatched = hook->next(self, base, vm, stack, result, frame);
            if (signal)
            {
                signal->dispatched = dispatched;
                try
                {
                    publish(std::move(*signal));
                }
                catch (const std::exception &e)
                {
                    logger::error("PUBLISH_ERROR {}", e.what());
                }
            }
            return dispatched;
        }

        static script::IFunction *Find(script::ObjectTypeInfo *type, const char *name, bool global)
        {
            if (!type || !type->IsLinkedValid())
                return nullptr;
            if (global)
            {
                auto first = type->GetGlobalFuncIter();
                for (unsigned i = 0; i < type->GetNumGlobalFuncs(); ++i)
                    if (first[i].func && _stricmp(first[i].func->GetName().c_str(), name) == 0)
                        return first[i].func.get();
            }
            else
            {
                auto first = type->GetMemberFuncIter();
                for (unsigned i = 0; i < type->GetNumMemberFuncs(); ++i)
                    if (first[i].func && _stricmp(first[i].func->GetName().c_str(), name) == 0)
                        return first[i].func.get();
            }
            return nullptr;
        }

      public:
        static void Configure(CaptureFn c, PublishFn p)
        {
            capture = c;
            publish = p;
        }

        template <std::size_t Index>
        static bool
        Install(const char *typeName, const char *name, Kind kind, bool global, unsigned params)
        {
            static_assert(Index < count);
            if (hooks[Index].load())
                return true;
            auto vm = VM::GetSingleton();
            RE::BSTSmartPointer<script::ObjectTypeInfo> type;
            if (!vm || !vm->GetScriptObjectType1(typeName, type) || !type)
            {
                logger::warn("HOOK_MISSING {}.{} type unavailable", typeName, name);
                return false;
            }
            auto fn = Find(type.get(), name, global);
            if (!fn || !fn->GetIsNative() || fn->GetIsStatic() != global ||
                fn->GetParamCount() != params)
            {
                logger::error("HOOK_REJECT {}.{} native/signature mismatch", typeName, name);
                return false;
            }
            auto slot = reinterpret_cast<void *volatile *>(fn);
            auto original = *reinterpret_cast<std::uintptr_t **>(fn);
            auto hook = new Hook;
            hook->keep.reset(fn);
            hook->kind = kind;
            std::copy_n(original - 1, virtuals + 1, hook->table.begin());
            hook->next = reinterpret_cast<Dispatch>(original[dispatchSlot]);
            hook->table[dispatchSlot + 1] = reinterpret_cast<std::uintptr_t>(&Call<Index>);
            hooks[Index].store(hook, std::memory_order_release);
            auto observed =
                InterlockedCompareExchangePointer(slot, hook->table.data() + 1, original);
            if (observed != original)
            {
                logger::error("HOOK_REJECT {}.{} vptr changed concurrently", typeName, name);
                // Keep the published hook alive; no unsafe retry against a new vptr.
                return false;
            }
            logger::info("HOOK_OK {}.{} function={:X} original={:X} params={}",
                         typeName,
                         name,
                         reinterpret_cast<std::uintptr_t>(fn),
                         reinterpret_cast<std::uintptr_t>(hook->next),
                         params);
            for (unsigned i = 0; i < params; ++i)
            {
                RE::BSFixedString param;
                script::TypeInfo paramType;
                fn->GetParam(i, param, paramType);
                logger::info("HOOK_PARAM {}.{} index={} name={}", typeName, name, i, param.c_str());
            }
            return true;
        }
    };
}
